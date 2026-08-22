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

#include "vulkan/VulkanPhysicalDevice.hpp"
#include "vulkan/VulkanQueueFamilies.hpp"
#include "vulkan/VulkanResult.hpp"
#include "vulkan/VulkanSwapchainSupport.hpp"
#include "vulkan/VulkanVersion.hpp"

#include <cstdio>
#include <limits>

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

    if (g_failureCount == 0)
    {
        std::printf("All Vulkan (pure-logic) M8A checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
