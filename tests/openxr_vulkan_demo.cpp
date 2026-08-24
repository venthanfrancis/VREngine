// Manual M9C validation demo — NOT part of the automated CTest suite,
// since it requires a real OpenXR loader/runtime with XR_KHR_vulkan_enable2
// support (and ideally a real or simulated HMD) that CI/headless systems
// may lack. Built by CMake but deliberately not registered with
// add_test. Run it manually.
//
// Proves OpenXR/Vulkan integration far enough to construct every value
// a future graphics XrSession's XrGraphicsBindingVulkan2KHR will need:
// enable XR_KHR_vulkan_enable2 -> obtain XrSystemId -> query Vulkan
// graphics requirements -> create an XR-compatible VkInstance (via
// xrCreateVulkanInstanceKHR) -> ask OpenXR which VkPhysicalDevice to
// use (via xrGetVulkanGraphicsDevice2KHR) -> create an XR-compatible
// VkDevice (via xrCreateVulkanDeviceKHR) -> retrieve a graphics queue
// -> log the resulting graphics-binding data -> clean shutdown. Does
// NOT create an XrSession, does NOT create any Windows presentation
// surface/swapchain, does NOT render anything — M9C is graphics-binding
// bring-up only, see docs/ROADMAP.md.
//
// This demo reaches directly into XR's private src/openxr/
// implementation (not through any public XR API), same reasoning as
// M9A's arengine_openxr_demo.
//
// Same three-outcome distinction M9A established (see
// docs/ARCHITECTURE.md, "Headset Absent Case (M9A)"), plus a fourth
// M9C-specific stop condition:
//   A. No OpenXR runtime available at all.
//   B. A runtime exists, but no HMD-class system is available.
//   C. A runtime and an HMD-class system are both available, but
//      XR_KHR_vulkan_enable2 is NOT supported - reported clearly, then
//      this demo stops. See docs/ARCHITECTURE.md, "XR_KHR_vulkan_enable2
//      Selection (M9C)" for why this does not silently fall back to
//      XR_KHR_vulkan_enable (the older, non-"2" extension).
//   D. Runtime + HMD system + XR_KHR_vulkan_enable2 all available - the
//      full M9C graphics-binding path runs.

#include "AREngine/Core/Core.hpp"

#include "openxr/OpenXRInstance.hpp"
#include "openxr/OpenXRResult.hpp"
#include "openxr/OpenXRSystem.hpp"
#include "openxr/OpenXRVersion.hpp"
#include "openxr/OpenXRVulkanGraphicsBinding.hpp"

#include <array>
#include <format>

namespace
{
    // Demo-only presentation logging, not part of the reusable
    // OpenXRVulkanGraphicsBinding class - see docs/ARCHITECTURE.md,
    // "XR-Vulkan Integration Placement (M9C)": M9C's brief explicitly
    // prefers a small integration object/layer used by the dedicated
    // demo, not a bigger reusable string-formatting surface the library
    // itself doesn't need.
    std::string VkPhysicalDeviceTypeToString(VkPhysicalDeviceType type)
    {
        switch (type)
        {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete GPU";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated GPU";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual GPU";
            case VK_PHYSICAL_DEVICE_TYPE_CPU:             return "CPU (software)";
            default:                                      return "other";
        }
    }
}

int main()
{
    using namespace AREngine::XR::OpenXR;

    AR_LOG_INFO(std::format("AREngine OpenXR/Vulkan demo - header version {}, requesting API version {}",
                             FormatXrVersion(XR_CURRENT_API_VERSION), FormatXrVersion(kTargetApiVersion)));

    // --- API layers (unchanged from M9A - logged for diagnostics only) ---
    const std::vector<XrApiLayerProperties> layers = EnumerateApiLayers();
    AR_LOG_INFO(std::format("Available OpenXR API layers: {}", layers.size()));
    for (const XrApiLayerProperties& layer : layers)
    {
        AR_LOG_INFO(std::format("  {} (spec {}, layer version {}) - {}",
                                 layer.layerName, FormatXrVersion(layer.specVersion), layer.layerVersion, layer.description));
    }

    // --- Instance extensions: explicitly verify XR_KHR_vulkan_enable2
    // is supported before requesting it - see docs/ARCHITECTURE.md,
    // "XR_KHR_vulkan_enable2 Selection (M9C)". If it is not supported,
    // this reports that clearly and stops (Case C) rather than falling
    // back to the older XR_KHR_vulkan_enable or inventing another path,
    // per the M9C brief. ---
    const std::vector<XrExtensionProperties> extensions = EnumerateInstanceExtensions();
    AR_LOG_INFO(std::format("Available OpenXR instance extensions: {}", extensions.size()));
    const bool vulkanEnable2Supported = IsExtensionSupported(extensions, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);

    if (!vulkanEnable2Supported)
    {
        AR_LOG_WARNING(std::format(
            "{} is NOT supported by the active OpenXR runtime (or no runtime is active yet - instance "
            "extensions cannot be enumerated meaningfully without one). M9C requires this extension and "
            "deliberately does not fall back to the older XR_KHR_vulkan_enable or invent another path - "
            "stopping here.", XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME));
        AR_LOG_INFO("OpenXR/Vulkan demo exiting cleanly (XR_KHR_vulkan_enable2 unavailable)");
        return 0;
    }
    AR_LOG_INFO(std::format("{} is supported - will be enabled on instance creation", XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME));

    // --- Instance creation, with XR_KHR_vulkan_enable2 enabled ---
    const std::array<const char*, 1> requestedExtensions{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    OpenXRInstance instance(requestedExtensions);
    if (!instance.IsValid())
    {
        // Case A vs. a genuinely unexpected failure - identical
        // handling to M9A's demo.
        if (instance.CreationResult() == XR_ERROR_RUNTIME_UNAVAILABLE)
        {
            AR_LOG_WARNING("No active OpenXR runtime found (XR_ERROR_RUNTIME_UNAVAILABLE) - this is a normal, "
                            "expected outcome on a desktop dev machine without XR hardware set up.");
        }
        else
        {
            AR_LOG_WARNING(std::format("xrCreateInstance failed unexpectedly: {} - cannot continue",
                                        XrResultToReadableString(XR_NULL_HANDLE, instance.CreationResult())));
        }
        AR_LOG_INFO("OpenXR/Vulkan demo exiting cleanly (no instance available)");
        return 0;
    }

    XrInstanceProperties instanceProperties{};
    instanceProperties.type = XR_TYPE_INSTANCE_PROPERTIES;
    CheckXrResult(instance.Get(), xrGetInstanceProperties(instance.Get(), &instanceProperties), "xrGetInstanceProperties");
    AR_LOG_INFO(std::format("Active OpenXR runtime: {} (version {})",
                             instanceProperties.runtimeName, FormatXrVersion(instanceProperties.runtimeVersion)));

    // --- System selection: request an HMD-class system (unchanged from M9A) ---
    const SystemRequestResult systemResult = TryGetHmdSystem(instance.Get());
    if (!systemResult.found)
    {
        if (IsFormFactorUnavailable(systemResult.rawResult))
        {
            AR_LOG_WARNING("OpenXR runtime is active, but no head-mounted-display system is currently "
                            "available - the runtime is installed and responding, but no headset appears "
                            "to be connected/active right now.");
        }
        else
        {
            AR_LOG_WARNING(std::format("xrGetSystem failed unexpectedly: {}",
                                        XrResultToReadableString(instance.Get(), systemResult.rawResult)));
        }
        AR_LOG_INFO("OpenXR/Vulkan demo exiting cleanly (runtime present, no HMD system)");
        return 0;
    }

    AR_LOG_INFO(std::format("HMD system acquired: XrSystemId {}", systemResult.systemId));
    XrSystemProperties systemProperties{};
    systemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
    CheckXrResult(instance.Get(),
        xrGetSystemProperties(instance.Get(), systemResult.systemId, &systemProperties), "xrGetSystemProperties");
    AR_LOG_INFO(std::format("System name: {}", systemProperties.systemName));

    // --- M9C: the OpenXR/Vulkan graphics-binding integration itself ---
    //
    // `instance` (OpenXRInstance, declared above) is constructed BEFORE
    // `binding` below, so C++'s reverse-local-destruction-order rule
    // destroys `binding` first and `instance` last when main() returns -
    // exactly satisfying OpenXRVulkanGraphicsBinding's documented
    // requirement that the XrInstance it borrows outlive it. This
    // ordering is explicit and load-bearing, not incidental - see
    // docs/ARCHITECTURE.md, "Ownership / Destruction Order (M9C)".
    AR_LOG_INFO("Creating OpenXR-compatible Vulkan instance/device via XR_KHR_vulkan_enable2...");
    OpenXRVulkanGraphicsBinding binding(instance.Get(), systemResult.systemId);

    AR_LOG_INFO(std::format("Vulkan API version range supported by runtime: {} - {}",
                             FormatVkApiVersion(binding.GetSupportedVersionRange().minVkApiVersion),
                             FormatVkApiVersion(binding.GetSupportedVersionRange().maxVkApiVersion)));
    AR_LOG_INFO(std::format("Vulkan API version selected: {}", FormatVkApiVersion(binding.GetSelectedVulkanApiVersion())));
    AR_LOG_INFO(binding.IsValidationEnabled()
                    ? "Vulkan validation layer enabled for the OpenXR/Vulkan instance (VK_LAYER_KHRONOS_validation)"
                    : "Vulkan validation layer NOT enabled (unavailable, or a Release build)");

    const VkPhysicalDeviceProperties& deviceProperties = binding.GetPhysicalDeviceProperties();
    AR_LOG_INFO(std::format("OpenXR-selected GPU: {} ({})",
                             deviceProperties.deviceName, VkPhysicalDeviceTypeToString(deviceProperties.deviceType)));
    AR_LOG_INFO(std::format("OpenXR-selected GPU Vulkan API version: {}", FormatVkApiVersion(deviceProperties.apiVersion)));

    const VulkanGraphicsBindingData& bindingData = binding.GetBindingData();
    AR_LOG_INFO(std::format("Graphics queue family index: {}, queue index: {}",
                             bindingData.queueFamilyIndex, bindingData.queueIndex));
    AR_LOG_INFO(std::format("Graphics queue handle: {}", static_cast<const void*>(binding.GetQueue())));
    AR_LOG_INFO(std::format("Graphics-binding data valid (instance/physicalDevice/device all non-null): {}",
                             bindingData.IsValid()));

    AR_LOG_INFO("No XrSession created (M9C is graphics-binding bring-up only - see docs/ROADMAP.md, M9D)");
    AR_LOG_INFO("OpenXR/Vulkan bring-up complete - shutting down");
    return 0;

    // `binding` is destroyed here first (its own destructor: debug
    // messenger, then VkDevice, then VkInstance, in that order), then
    // `instance` (xrDestroyInstance) - see the comment above `binding`'s
    // declaration for why this order is required and how it is
    // guaranteed. No XrSession, XR swapchain, or reference space ever
    // existed to destroy first.
}
