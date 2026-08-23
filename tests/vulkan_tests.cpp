// M8A automated tests for AREngine::Rendering::Vulkan's pure-logic
// helpers: physical device ranking, graphics-queue-family selection,
// and Vulkan version decoding/formatting. Deliberately calls ZERO real
// Vulkan API functions (no vkCreateInstance, no vkEnumeratePhysicalDevices,
// ...) — only uses Vulkan's plain C structs/enums as synthetic test
// data, so this runs on any machine with the Vulkan SDK headers
// available at compile time, without needing a Vulkan-capable GPU or
// driver at runtime. See docs/ARCHITECTURE.md, "M8A Implementation
// Notes" for the caveat this does NOT eliminate (the whole Rendering
// library links the Vulkan loader import library, so the loader DLL
// must still be resolvable for this executable to even start).
//
// The actual hardware bring-up (instance/device/queue creation) is
// exercised only by the separate, manual arengine_vulkan_demo — not
// part of this suite, since CTest must not depend on a GPU being
// present.

#include "AREngine/Core/Math/ViewProjection.hpp"
#include "AREngine/Rendering/MeshData.hpp"
#include "AREngine/Rendering/ProceduralMesh.hpp"

#include "vulkan/VulkanCheckerboard.hpp"
#include "vulkan/VulkanClipSpace.hpp"
#include "vulkan/VulkanDepthFormat.hpp"
#include "vulkan/VulkanMemory.hpp"
#include "vulkan/VulkanPhysicalDevice.hpp"
#include "vulkan/VulkanQueueFamilies.hpp"
#include "vulkan/VulkanResult.hpp"
#include "vulkan/VulkanSwapchainSupport.hpp"
#include "vulkan/VulkanVersion.hpp"
#include "vulkan/VulkanVertex.hpp"

#include <cstddef>
#include <cstdio>
#include <limits>
#include <numbers>

namespace
{
    int g_failureCount = 0;

    void Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", description);
            ++g_failureCount;
        }
    }

    using namespace AREngine::Rendering::Vulkan;
    using namespace AREngine::Core::Math;

    VkQueueFamilyProperties MakeQueueFamily(VkQueueFlags flags)
    {
        VkQueueFamilyProperties props{};
        props.queueFlags = flags;
        props.queueCount = 1;
        return props;
    }

    void TestRankPhysicalDeviceType()
    {
        const int discrete = RankPhysicalDeviceType(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
        const int integrated = RankPhysicalDeviceType(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
        const int cpu = RankPhysicalDeviceType(VK_PHYSICAL_DEVICE_TYPE_CPU);
        const int virtualGpu = RankPhysicalDeviceType(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);
        const int other = RankPhysicalDeviceType(VK_PHYSICAL_DEVICE_TYPE_OTHER);

        Check(discrete < integrated, "A discrete GPU ranks better (lower) than an integrated GPU");
        Check(integrated < cpu, "An integrated GPU ranks better than a CPU (software) device");
        Check(cpu == virtualGpu && virtualGpu == other,
              "CPU, virtual GPU, and other device types all rank equally last");
    }

    void TestFindGraphicsQueueFamilyEmpty()
    {
        const std::vector<VkQueueFamilyProperties> empty;
        Check(!FindGraphicsQueueFamily(empty).has_value(), "No queue families at all means no graphics family found");
    }

    void TestFindGraphicsQueueFamilyNoneQualify()
    {
        const std::vector<VkQueueFamilyProperties> families{
            MakeQueueFamily(VK_QUEUE_TRANSFER_BIT),
            MakeQueueFamily(VK_QUEUE_COMPUTE_BIT),
        };
        Check(!FindGraphicsQueueFamily(families).has_value(),
              "No family with VK_QUEUE_GRAPHICS_BIT means no graphics family found");
    }

    void TestFindGraphicsQueueFamilyReturnsFirstMatch()
    {
        const std::vector<VkQueueFamilyProperties> families{
            MakeQueueFamily(VK_QUEUE_TRANSFER_BIT),
            MakeQueueFamily(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT),
            MakeQueueFamily(VK_QUEUE_GRAPHICS_BIT), // a second graphics-capable family, should not be picked
        };
        const auto result = FindGraphicsQueueFamily(families);
        Check(result.has_value() && *result == 1, "The first graphics-capable family (index 1) is selected");
    }

    void TestDecodeVulkanVersion()
    {
        const std::uint32_t packed = VK_MAKE_API_VERSION(0, 1, 3, 5);
        const VulkanVersionParts parts = DecodeVulkanVersion(packed);
        Check(parts.major == 1, "DecodeVulkanVersion extracts the major version");
        Check(parts.minor == 3, "DecodeVulkanVersion extracts the minor version");
        Check(parts.patch == 5, "DecodeVulkanVersion extracts the patch version");
    }

    void TestFormatVulkanVersion()
    {
        Check(FormatVulkanVersion(kTargetApiVersion) == "1.2.0",
              "AREngine's target Vulkan API version formats as \"1.2.0\"");
        Check(FormatVulkanVersion(VK_MAKE_API_VERSION(0, 1, 0, 0)) == "1.0.0",
              "FormatVulkanVersion formats an arbitrary version correctly");
    }

    void TestVkResultToString()
    {
        Check(VkResultToString(VK_SUCCESS) == "VK_SUCCESS", "VK_SUCCESS has a readable name");
        Check(VkResultToString(VK_ERROR_DEVICE_LOST) == "VK_ERROR_DEVICE_LOST",
              "A known error code has a readable name");
        Check(VkResultToString(static_cast<VkResult>(-12345)) == "VkResult(-12345)",
              "An unrecognized VkResult falls back to a numeric representation, not a crash");
    }

    // --- M8B pure-logic checks ---

    void TestHasSeparatePresentQueue()
    {
        Check(!HasSeparatePresentQueue(QueueFamilyIndices{0, 0}), "Same family for graphics and present is not separate");
        Check(HasSeparatePresentQueue(QueueFamilyIndices{0, 1}), "Different families for graphics and present is separate");
    }

    void TestGetUniqueQueueFamiliesSameFamily()
    {
        const auto unique = GetUniqueQueueFamilies(QueueFamilyIndices{2, 2});
        Check(unique.size() == 1 && unique[0] == 2, "Same graphics/present family yields exactly one unique family");
    }

    void TestGetUniqueQueueFamiliesDifferentFamilies()
    {
        const auto unique = GetUniqueQueueFamilies(QueueFamilyIndices{0, 3});
        Check(unique.size() == 2 && unique[0] == 0 && unique[1] == 3,
              "Different graphics/present families yields both, graphics first");
    }

    void TestIsSwapchainSupportAdequate()
    {
        Check(IsSwapchainSupportAdequate(true, true), "Formats and present modes both present is adequate");
        Check(!IsSwapchainSupportAdequate(false, true), "No formats is not adequate");
        Check(!IsSwapchainSupportAdequate(true, false), "No present modes is not adequate");
        Check(!IsSwapchainSupportAdequate(false, false), "Neither formats nor present modes is not adequate");
    }

    void TestChooseSurfaceFormatPrefersSrgbBgra()
    {
        const std::vector<VkSurfaceFormatKHR> available{
            {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        };
        const VkSurfaceFormatKHR chosen = ChooseSurfaceFormat(available);
        Check(chosen.format == VK_FORMAT_B8G8R8A8_SRGB && chosen.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
              "SRGB BGRA is preferred when available, regardless of list order");
    }

    void TestChooseSurfaceFormatFallsBackToFirst()
    {
        const std::vector<VkSurfaceFormatKHR> available{
            {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
            {VK_FORMAT_R5G6B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
        };
        const VkSurfaceFormatKHR chosen = ChooseSurfaceFormat(available);
        Check(chosen.format == VK_FORMAT_R8G8B8A8_UNORM, "Falls back to the first listed format when SRGB BGRA is unavailable");
    }

    void TestChoosePresentModePrefersFifo()
    {
        const std::vector<VkPresentModeKHR> available{VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};
        Check(ChoosePresentMode(available) == VK_PRESENT_MODE_FIFO_KHR, "FIFO is chosen even when other modes are listed first");
    }

    void TestChooseSwapchainImageCountClampsToMax()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        capabilities.minImageCount = 2;
        capabilities.maxImageCount = 2;
        Check(ChooseSwapchainImageCount(capabilities) == 2, "minImageCount+1 is clamped down to a nonzero maxImageCount");
    }

    void TestChooseSwapchainImageCountNoMaxLimit()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        capabilities.minImageCount = 2;
        capabilities.maxImageCount = 0; // 0 means "no maximum"
        Check(ChooseSwapchainImageCount(capabilities) == 3, "minImageCount+1 is used as-is when maxImageCount is 0 (no maximum)");
    }

    void TestChooseSwapchainExtentUsesCurrentExtentWhenFixed()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        capabilities.currentExtent = {800, 600};
        const VkExtent2D extent = ChooseSwapchainExtent(capabilities, 1920, 1080);
        Check(extent.width == 800 && extent.height == 600, "currentExtent is used as-is when the surface dictates a fixed size");
    }

    void TestChooseSwapchainExtentClampsWindowSize()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        capabilities.currentExtent = {std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()};
        capabilities.minImageExtent = {100, 100};
        capabilities.maxImageExtent = {1000, 1000};
        const VkExtent2D extent = ChooseSwapchainExtent(capabilities, 50, 2000);
        Check(extent.width == 100, "Window width below the surface minimum is clamped up");
        Check(extent.height == 1000, "Window height above the surface maximum is clamped down");
    }

    // --- M8D pure-logic checks ---

    VkPhysicalDeviceMemoryProperties MakeSyntheticMemoryProperties()
    {
        // A small, made-up memory layout - not a real GPU's - with four
        // types, including two (1 and 3) that share the same property
        // flags, specifically so a test can prove FindMemoryType
        // actually honors the type-index bitmask and doesn't just
        // return the first property-matching type regardless of it.
        VkPhysicalDeviceMemoryProperties props{};
        props.memoryTypeCount = 4;

        props.memoryTypes[0].propertyFlags = 0; // no relevant properties at all
        props.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        props.memoryTypes[2].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        props.memoryTypes[3].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        return props;
    }

    void TestFindMemoryTypeMatchesHostVisible()
    {
        const VkPhysicalDeviceMemoryProperties props = MakeSyntheticMemoryProperties();
        // typeFilter allows all four types (bits 0-3 set).
        const std::uint32_t index = FindMemoryType(props, 0b1111,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(index == 1, "FindMemoryType selects the first type with both required HOST_VISIBLE and HOST_COHERENT bits");
    }

    void TestFindMemoryTypeMatchesDeviceLocal()
    {
        const VkPhysicalDeviceMemoryProperties props = MakeSyntheticMemoryProperties();
        const std::uint32_t index = FindMemoryType(props, 0b1111, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Check(index == 2, "FindMemoryType selects the DEVICE_LOCAL type");
    }

    void TestFindMemoryTypeRespectsTypeFilterBitmask()
    {
        const VkPhysicalDeviceMemoryProperties props = MakeSyntheticMemoryProperties();
        // Type 1 has the exact properties being searched for, but bit 1
        // is excluded from the filter (0b1101 = types 0, 2, 3 allowed) -
        // if the bitmask were being ignored, this would incorrectly
        // return 1 instead of 3.
        const std::uint32_t index = FindMemoryType(props, 0b1101,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        Check(index == 3, "FindMemoryType skips a property-matching type excluded by the type filter bitmask");
    }

    void TestVertexBindingDescription()
    {
        const VkVertexInputBindingDescription binding = GetVertexBindingDescription();
        Check(binding.binding == 0, "MeshVertex uses binding 0");
        Check(binding.stride == sizeof(AREngine::Rendering::MeshVertex), "MeshVertex binding stride equals sizeof(MeshVertex)");
        Check(binding.inputRate == VK_VERTEX_INPUT_RATE_VERTEX, "MeshVertex binding input rate is per-vertex, not per-instance");
    }

    void TestVertexAttributeDescriptions()
    {
        using AREngine::Rendering::MeshVertex;

        const auto attributes = GetVertexAttributeDescriptions();
        Check(attributes.size() == 3, "MeshVertex has exactly 3 attributes (position, color, uv)");

        Check(attributes[0].location == 0, "Position is at shader location 0");
        Check(attributes[0].format == VK_FORMAT_R32G32B32_SFLOAT, "Position is a 3-component float format (Vec3)");
        Check(attributes[0].offset == offsetof(MeshVertex, position), "Position offset matches the real struct layout");

        Check(attributes[1].location == 1, "Color is at shader location 1");
        Check(attributes[1].format == VK_FORMAT_R32G32B32_SFLOAT, "Color is a 3-component float format (Vec3)");
        Check(attributes[1].offset == offsetof(MeshVertex, color), "Color offset matches the real struct layout");

        Check(attributes[2].location == 2, "UV is at shader location 2");
        Check(attributes[2].format == VK_FORMAT_R32G32_SFLOAT, "UV is a 2-component float format (Vec2)");
        Check(attributes[2].offset == offsetof(MeshVertex, uv), "UV offset matches the real struct layout");
    }

    // --- M8E pure-logic checks ---

    void TestCheckerboardByteSize()
    {
        const auto pixels = GenerateCheckerboardRGBA8(64, 64, 8);
        Check(pixels.size() == 64u * 64u * 4u, "Checkerboard byte size is width * height * 4 (RGBA8, tightly packed)");
    }

    void TestCheckerboardAlternatesTiles()
    {
        // tileSize=2: tile (0,0) covers pixels x/y in [0,2), tile (1,0)
        // covers x in [2,4) - adjacent tiles along a row must differ.
        const auto pixels = GenerateCheckerboardRGBA8(4, 2, 2);

        auto pixelAt = [&](std::uint32_t x, std::uint32_t y)
        {
            const std::size_t index = (static_cast<std::size_t>(y) * 4 + x) * 4;
            return pixels[index]; // R channel is enough to distinguish black (0) from white (255)
        };

        Check(pixelAt(0, 0) != pixelAt(2, 0), "Adjacent tiles along a row have different colors");
        Check(pixelAt(0, 0) == pixelAt(1, 0), "Pixels within the same tile share the same color");
    }

    void TestCheckerboardFullyOpaque()
    {
        const auto pixels = GenerateCheckerboardRGBA8(8, 8, 4);
        bool allOpaque = true;
        for (std::size_t i = 3; i < pixels.size(); i += 4)
        {
            if (pixels[i] != 255)
            {
                allOpaque = false;
                break;
            }
        }
        Check(allOpaque, "Every checkerboard pixel's alpha channel is fully opaque (255)");
    }

    // --- M8F pure-logic checks ---

    DepthFormatCandidate MakeCandidate(VkFormat format, bool supportsDepthStencilAttachment)
    {
        DepthFormatCandidate candidate;
        candidate.format = format;
        candidate.properties.optimalTilingFeatures =
            supportsDepthStencilAttachment ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT : 0;
        return candidate;
    }

    void TestSelectDepthFormatPrefersFirstSupported()
    {
        const std::vector<DepthFormatCandidate> candidates{
            MakeCandidate(VK_FORMAT_D32_SFLOAT, true),
            MakeCandidate(VK_FORMAT_D32_SFLOAT_S8_UINT, true),
            MakeCandidate(VK_FORMAT_D24_UNORM_S8_UINT, true),
        };
        Check(SelectDepthFormat(candidates) == VK_FORMAT_D32_SFLOAT,
              "SelectDepthFormat picks the first candidate when all are supported");
    }

    void TestSelectDepthFormatSkipsUnsupported()
    {
        const std::vector<DepthFormatCandidate> candidates{
            MakeCandidate(VK_FORMAT_D32_SFLOAT, false), // not supported on this synthetic device
            MakeCandidate(VK_FORMAT_D32_SFLOAT_S8_UINT, false),
            MakeCandidate(VK_FORMAT_D24_UNORM_S8_UINT, true),
        };
        Check(SelectDepthFormat(candidates) == VK_FORMAT_D24_UNORM_S8_UINT,
              "SelectDepthFormat skips candidates lacking DEPTH_STENCIL_ATTACHMENT_BIT and picks the first that has it");
    }

    void TestApplyVulkanYFlipNegatesOnlyYScale()
    {
        Mat4 m = Mat4::Identity();
        m.Set(0, 0, 3.0f);
        m.Set(1, 1, 2.5f);
        m.Set(2, 3, 7.0f);

        const Mat4 flipped = ApplyVulkanYFlip(m);
        Check(flipped.At(1, 1) == -2.5f, "ApplyVulkanYFlip negates the Y-scale term");
        Check(flipped.At(0, 0) == 3.0f, "ApplyVulkanYFlip leaves the X-scale term unchanged");
        Check(flipped.At(2, 3) == 7.0f, "ApplyVulkanYFlip leaves unrelated entries unchanged");
    }

    void TestApplyVulkanYFlipComposesWithProjection()
    {
        // Reproduces the exact check M8F originally had baked into
        // Core's projection helper, now split across two layers: Core
        // builds the plain RH/ZO matrix (no flip - see
        // TestPerspectiveRH_ZO in tests/core_tests.cpp), and this
        // Vulkan-layer flip is what makes a world-space "up" point
        // land at negative NDC y, as Vulkan's framebuffer convention
        // requires.
        const Mat4 proj = ApplyVulkanYFlip(PerspectiveRH_ZO(std::numbers::pi_v<float> / 2.0f, 1.0f, 1.0f, 10.0f));
        const Vec4 upPoint = proj * Vec4(0.0f, 1.0f, -1.0f, 1.0f);
        Check(upPoint.y / upPoint.w < 0.0f,
              "ApplyVulkanYFlip composed with PerspectiveRH_ZO gives a world-space 'up' point negative NDC y");
    }
}

int main()
{
    TestRankPhysicalDeviceType();
    TestFindGraphicsQueueFamilyEmpty();
    TestFindGraphicsQueueFamilyNoneQualify();
    TestFindGraphicsQueueFamilyReturnsFirstMatch();
    TestDecodeVulkanVersion();
    TestFormatVulkanVersion();
    TestVkResultToString();

    TestHasSeparatePresentQueue();
    TestGetUniqueQueueFamiliesSameFamily();
    TestGetUniqueQueueFamiliesDifferentFamilies();
    TestIsSwapchainSupportAdequate();
    TestChooseSurfaceFormatPrefersSrgbBgra();
    TestChooseSurfaceFormatFallsBackToFirst();
    TestChoosePresentModePrefersFifo();
    TestChooseSwapchainImageCountClampsToMax();
    TestChooseSwapchainImageCountNoMaxLimit();
    TestChooseSwapchainExtentUsesCurrentExtentWhenFixed();
    TestChooseSwapchainExtentClampsWindowSize();

    TestFindMemoryTypeMatchesHostVisible();
    TestFindMemoryTypeMatchesDeviceLocal();
    TestFindMemoryTypeRespectsTypeFilterBitmask();
    TestVertexBindingDescription();
    TestVertexAttributeDescriptions();

    TestCheckerboardByteSize();
    TestCheckerboardAlternatesTiles();
    TestCheckerboardFullyOpaque();

    TestSelectDepthFormatPrefersFirstSupported();
    TestSelectDepthFormatSkipsUnsupported();
    TestApplyVulkanYFlipNegatesOnlyYScale();
    TestApplyVulkanYFlipComposesWithProjection();

    if (g_failureCount == 0)
    {
        std::printf("All Vulkan (pure-logic) M8A checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
