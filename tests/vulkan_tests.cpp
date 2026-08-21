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
#include "vulkan/VulkanResult.hpp"
#include "vulkan/VulkanVersion.hpp"

#include <cstdio>

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

    if (g_failureCount == 0)
    {
        std::printf("All Vulkan (pure-logic) M8A checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
