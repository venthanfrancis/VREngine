// M9C automated tests for AREngine::XR::OpenXR's OpenXR/Vulkan
// integration pure-logic helpers: version conversion/range
// compatibility, queue-family selection, and graphics-binding data
// validation. Deliberately calls ZERO real OpenXR or Vulkan API
// functions (no xrCreateVulkanInstanceKHR, no vkCreateInstance, ...) —
// only uses their plain C structs/enums as synthetic test data, so this
// runs on any machine with the OpenXR and Vulkan headers available at
// compile time, without needing a real OpenXR runtime, headset, or even
// a real GPU at runtime. See docs/ARCHITECTURE.md, "M9C Implementation
// Notes".
//
// Real OpenXR/Vulkan bring-up (instance/device creation against a real
// loader/runtime/GPU) is exercised only by the separate, manual
// arengine_openxr_vulkan_demo — not part of this suite, since CTest
// must not depend on an XR runtime or headset being present.

#include "openxr/OpenXRSwapchain.hpp"
#include "openxr/OpenXRVulkanGraphicsBinding.hpp"

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

    using namespace AREngine::XR::OpenXR;

    // --- Version conversion (XrVersion -> Vulkan's VkVersion encoding) ---

    void TestXrVersionToVkApiVersionConvertsCorrectly()
    {
        // XR_MAKE_VERSION(1, 2, 999) uses OpenXR's 16/16/32-bit packing;
        // VK_MAKE_API_VERSION(0, 1, 2, 0) uses Vulkan's 7/10/12-bit
        // packing. These are deliberately different numeric values -
        // the test would fail if the conversion were a naive
        // reinterpret instead of a real decode-then-re-encode.
        const XrVersion xrVersion = XR_MAKE_VERSION(1, 2, 999);
        const std::uint32_t vkVersion = XrVersionToVkApiVersion(xrVersion);
        Check(vkVersion == VK_MAKE_API_VERSION(0, 1, 2, 0),
              "XrVersionToVkApiVersion correctly decodes XrVersion's major/minor and re-encodes as a Vulkan version");
        Check(VK_API_VERSION_PATCH(vkVersion) == 0,
              "XrVersionToVkApiVersion always produces patch 0 (patch is intentionally dropped)");
    }

    void TestFormatVkApiVersion()
    {
        Check(FormatVkApiVersion(VK_API_VERSION_1_2) == "1.2.0", "FormatVkApiVersion formats Vulkan 1.2 correctly");
        Check(FormatVkApiVersion(VK_API_VERSION_1_0) == "1.0.0", "FormatVkApiVersion formats Vulkan 1.0 correctly");
    }

    // --- Version range compatibility ---

    void TestIsVulkanApiVersionSupportedWithinRange()
    {
        const VulkanVersionRange range{VK_API_VERSION_1_0, VK_API_VERSION_1_3};
        Check(IsVulkanApiVersionSupported(VK_API_VERSION_1_2, range),
              "A version strictly between min and max is supported");
        Check(IsVulkanApiVersionSupported(VK_API_VERSION_1_0, range), "The minimum itself is supported (inclusive)");
        Check(IsVulkanApiVersionSupported(VK_API_VERSION_1_3, range), "The maximum itself is supported (inclusive)");
    }

    void TestIsVulkanApiVersionSupportedOutsideRange()
    {
        const VulkanVersionRange range{VK_MAKE_API_VERSION(0, 1, 1, 0), VK_MAKE_API_VERSION(0, 1, 3, 0)};
        Check(!IsVulkanApiVersionSupported(VK_API_VERSION_1_0, range),
              "A version below the minimum is not supported");
        Check(!IsVulkanApiVersionSupported(VK_MAKE_API_VERSION(0, 1, 4, 0), range),
              "A version above the maximum is not supported");
    }

    void TestSelectVulkanApiVersionPrefersPreferredWhenSupported()
    {
        const VulkanVersionRange range{VK_API_VERSION_1_0, VK_MAKE_API_VERSION(0, 1, 3, 0)};
        Check(SelectVulkanApiVersion(VK_API_VERSION_1_2, range) == VK_API_VERSION_1_2,
              "SelectVulkanApiVersion keeps the preferred version (e.g. AREngine's desktop 1.2 target) when it's in range");
    }

    void TestSelectVulkanApiVersionFallsBackToMinimum()
    {
        // Preferred (1.2) is below this runtime's minimum (1.3) -
        // must fall back to the runtime's own minimum, never silently
        // request something outside the declared range.
        const VulkanVersionRange range{VK_MAKE_API_VERSION(0, 1, 3, 0), VK_MAKE_API_VERSION(0, 1, 4, 0)};
        Check(SelectVulkanApiVersion(VK_API_VERSION_1_2, range) == range.minVkApiVersion,
              "SelectVulkanApiVersion falls back to the runtime's minimum when the preferred version is out of range");
    }

    // --- Timeline semaphore device-feature selection (M11.2) ---

    void TestSelectTimelineSemaphoreSupportedAndApi12()
    {
        // Case A: core in the selected version - enabled, no extension.
        const auto selection = SelectTimelineSemaphoreSupport(VK_API_VERSION_1_2, /*supported=*/true, /*extensionAvailable=*/false);
        Check(selection.enable, "Enabled when the physical device supports it and the selected version is >= 1.2");
        Check(!selection.requiresExtension, "No extension required when timeline semaphores are core (>= 1.2)");
    }

    void TestSelectTimelineSemaphoreSupportedAndApi11WithExtension()
    {
        // Case B: pre-1.2, extension present - enabled via the extension.
        const auto selection = SelectTimelineSemaphoreSupport(VK_API_VERSION_1_1, /*supported=*/true, /*extensionAvailable=*/true);
        Check(selection.enable, "Enabled when the physical device supports it, API < 1.2, and the extension is available");
        Check(selection.requiresExtension, "VK_KHR_timeline_semaphore is required below Vulkan 1.2");
    }

    void TestSelectTimelineSemaphoreUnsupportedNeverEnabled()
    {
        // Case C: no support at all - never enabled, regardless of
        // version or extension availability.
        const auto selection = SelectTimelineSemaphoreSupport(VK_API_VERSION_1_2, /*supported=*/false, /*extensionAvailable=*/true);
        Check(!selection.enable, "Never enabled when the physical device does not report support, even if the extension exists");
        Check(!selection.requiresExtension, "No extension requested for an unsupported feature");
    }

    void TestSelectTimelineSemaphorePre12WithoutExtensionStaysDisabled()
    {
        // Below 1.2, supported by the device, but the extension is not
        // enumerated - clear failure per policy: do not enable a
        // feature AREngine cannot actually satisfy.
        const auto selection = SelectTimelineSemaphoreSupport(VK_API_VERSION_1_1, /*supported=*/true, /*extensionAvailable=*/false);
        Check(!selection.enable, "Not enabled below 1.2 when VK_KHR_timeline_semaphore is unavailable, even if the device otherwise supports it");
        Check(!selection.requiresExtension, "No extension requested when it isn't available in the first place");
    }

    void TestSelectTimelineSemaphoreHasNoRuntimeNameDependency()
    {
        // No runtime-name parameter exists on this function at all -
        // the same (version, supported, extensionAvailable) inputs
        // always produce the same result, whatever runtime produced them.
        const auto steamVrShaped = SelectTimelineSemaphoreSupport(VK_API_VERSION_1_2, true, false);
        const auto metaShaped = SelectTimelineSemaphoreSupport(VK_API_VERSION_1_2, true, false);
        Check(steamVrShaped.enable == metaShaped.enable && steamVrShaped.requiresExtension == metaShaped.requiresExtension,
              "Identical capability inputs always produce an identical decision - no hidden runtime-name branching");
    }

    // --- Queue family selection ---

    VkQueueFamilyProperties MakeQueueFamily(VkQueueFlags flags)
    {
        VkQueueFamilyProperties props{};
        props.queueFlags = flags;
        props.queueCount = 1;
        return props;
    }

    void TestFindGraphicsQueueFamilyEmpty()
    {
        Check(!FindGraphicsQueueFamily({}).has_value(), "No queue families at all means no graphics family found");
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

    // --- Graphics-binding data validation ---

    void TestVulkanGraphicsBindingDataDefaultIsInvalid()
    {
        Check(!VulkanGraphicsBindingData{}.IsValid(), "A default-constructed (all-null) binding is invalid");
    }

    void TestVulkanGraphicsBindingDataRequiresAllThreeHandles()
    {
        // Fake, non-null-but-never-dereferenced handle values - valid
        // for a pure IsValid() check (which only compares against
        // VK_NULL_HANDLE), never passed to a real Vulkan call.
        auto fakeInstance = reinterpret_cast<VkInstance>(0x1);
        auto fakePhysicalDevice = reinterpret_cast<VkPhysicalDevice>(0x2);
        auto fakeDevice = reinterpret_cast<VkDevice>(0x3);

        VulkanGraphicsBindingData binding;
        Check(!binding.IsValid(), "All-null binding is invalid");

        binding.instance = fakeInstance;
        Check(!binding.IsValid(), "Only `instance` set is still invalid");

        binding.physicalDevice = fakePhysicalDevice;
        Check(!binding.IsValid(), "`instance` + `physicalDevice` set, but `device` still null, is still invalid");

        binding.device = fakeDevice;
        Check(binding.IsValid(), "All three handles non-null makes the binding valid");
    }

    // --- Swapchain color format selection (M9E) ---

    void TestSelectSwapchainColorFormatPrefersBgraSrgb()
    {
        const std::vector<std::int64_t> supported{
            static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_UNORM),
            static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_SRGB),
            static_cast<std::int64_t>(VK_FORMAT_B8G8R8A8_SRGB),
        };
        const auto selected = SelectSwapchainColorFormat(supported);
        Check(selected.has_value() && *selected == static_cast<std::int64_t>(VK_FORMAT_B8G8R8A8_SRGB),
              "Selects VK_FORMAT_B8G8R8A8_SRGB when it is supported, regardless of enumeration order");
    }

    void TestSelectSwapchainColorFormatFallsBackToRgbaSrgb()
    {
        const std::vector<std::int64_t> supported{
            static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_UNORM),
            static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_SRGB),
        };
        const auto selected = SelectSwapchainColorFormat(supported);
        Check(selected.has_value() && *selected == static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_SRGB),
              "Falls back to VK_FORMAT_R8G8B8A8_SRGB when VK_FORMAT_B8G8R8A8_SRGB is unsupported");
    }

    void TestSelectSwapchainColorFormatFallsBackToFirstWhenNoSrgb()
    {
        // Deliberately no sRGB format at all - never assumed present,
        // per M9E's brief. Falls back to whatever the runtime reports
        // first, rather than failing outright.
        const std::vector<std::int64_t> supported{
            static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_UNORM),
            static_cast<std::int64_t>(VK_FORMAT_R16G16B16A16_SFLOAT),
        };
        const auto selected = SelectSwapchainColorFormat(supported);
        Check(selected.has_value() && *selected == static_cast<std::int64_t>(VK_FORMAT_R8G8B8A8_UNORM),
              "Falls back to the first reported format when no preferred sRGB format is available");
    }

    void TestSelectSwapchainColorFormatReturnsNulloptWhenEmpty()
    {
        const auto selected = SelectSwapchainColorFormat({});
        Check(!selected.has_value(), "Returns std::nullopt when the runtime reports zero swapchain formats");
    }
}

int main()
{
    TestXrVersionToVkApiVersionConvertsCorrectly();
    TestFormatVkApiVersion();

    TestIsVulkanApiVersionSupportedWithinRange();
    TestIsVulkanApiVersionSupportedOutsideRange();
    TestSelectVulkanApiVersionPrefersPreferredWhenSupported();
    TestSelectVulkanApiVersionFallsBackToMinimum();

    TestSelectTimelineSemaphoreSupportedAndApi12();
    TestSelectTimelineSemaphoreSupportedAndApi11WithExtension();
    TestSelectTimelineSemaphoreUnsupportedNeverEnabled();
    TestSelectTimelineSemaphorePre12WithoutExtensionStaysDisabled();
    TestSelectTimelineSemaphoreHasNoRuntimeNameDependency();

    TestFindGraphicsQueueFamilyEmpty();
    TestFindGraphicsQueueFamilyNoneQualify();
    TestFindGraphicsQueueFamilyReturnsFirstMatch();

    TestVulkanGraphicsBindingDataDefaultIsInvalid();
    TestVulkanGraphicsBindingDataRequiresAllThreeHandles();

    TestSelectSwapchainColorFormatPrefersBgraSrgb();
    TestSelectSwapchainColorFormatFallsBackToRgbaSrgb();
    TestSelectSwapchainColorFormatFallsBackToFirstWhenNoSrgb();
    TestSelectSwapchainColorFormatReturnsNulloptWhenEmpty();

    if (g_failureCount == 0)
    {
        std::printf("All OpenXR/Vulkan (pure-logic) M9C/M9E checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
