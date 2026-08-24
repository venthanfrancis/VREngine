// Manual M9A validation demo — NOT part of the automated CTest suite,
// since it requires a real OpenXR loader/runtime (and ideally a real
// headset) that CI/headless systems may lack. Built by CMake but
// deliberately not registered with add_test. Run it manually.
//
// Proves OpenXR bring-up end to end: enumerate API layers -> enumerate
// instance extensions -> create XrInstance -> query runtime info ->
// request an XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY system -> log system
// properties -> clean shutdown. Does NOT create an XrSession, does NOT
// touch Vulkan, does NOT render anything — M9A is bring-up only, see
// docs/ROADMAP.md.
//
// This demo reaches directly into XR's private src/openxr/
// implementation (not through any public XR API) because no public
// API exposes OpenXR bring-up yet, by design — mirrors M8A's
// arengine_vulkan_demo reaching into Rendering's private src/vulkan/.
//
// Deliberately distinguishes three outcomes rather than treating every
// failure the same way (see docs/ARCHITECTURE.md, "Headset Absent Case
// (M9A)"):
//   A. No OpenXR runtime available at all (xrCreateInstance fails,
//      typically XR_ERROR_RUNTIME_UNAVAILABLE).
//   B. A runtime exists, but no HMD-class system is available
//      (xrGetSystem fails with XR_ERROR_FORM_FACTOR_UNAVAILABLE/
//      _UNSUPPORTED).
//   C. A runtime and an HMD-class system are both available.
// None of these is treated as a crash — A and B are ordinary machine
// states for a desktop dev machine, not bugs.

#include "AREngine/Core/Core.hpp"

#include "openxr/OpenXRInstance.hpp"
#include "openxr/OpenXRResult.hpp"
#include "openxr/OpenXRSystem.hpp"
#include "openxr/OpenXRVersion.hpp"

#include <format>

int main()
{
    using namespace AREngine::XR::OpenXR;

    AR_LOG_INFO(std::format("AREngine OpenXR bring-up demo - header version {}, requesting API version {}",
                             FormatXrVersion(XR_CURRENT_API_VERSION), FormatXrVersion(kTargetApiVersion)));

    // --- API layers (loader-level query, no instance needed) ---
    const std::vector<XrApiLayerProperties> layers = EnumerateApiLayers();
    AR_LOG_INFO(std::format("Available OpenXR API layers: {}", layers.size()));
    bool coreValidationLayerAvailable = false;
    for (const XrApiLayerProperties& layer : layers)
    {
        AR_LOG_INFO(std::format("  {} (spec {}, layer version {}) - {}",
                                 layer.layerName, FormatXrVersion(layer.specVersion),
                                 layer.layerVersion, layer.description));
        if (std::string_view(layer.layerName) == "XR_APILAYER_LUNARG_core_validation")
        {
            coreValidationLayerAvailable = true;
        }
    }
    // M9A deliberately does not enable any API layer, including
    // validation, even when available - see docs/ARCHITECTURE.md,
    // "API Layer Enumeration (M9A)": there is little for a validation
    // layer to usefully catch yet (no session, no swapchain, no frame
    // loop), and the brief is explicit that arbitrary layers must not
    // be enabled. Normal execution never requires this layer to be
    // installed.
    AR_LOG_INFO(coreValidationLayerAvailable
                    ? "OpenXR core validation layer is available, but NOT enabled (see source comment)"
                    : "OpenXR core validation layer is not available (not required for normal execution)");

    // --- Instance extensions (also a loader-level query) ---
    const std::vector<XrExtensionProperties> extensions = EnumerateInstanceExtensions();
    AR_LOG_INFO(std::format("Available OpenXR instance extensions: {}", extensions.size()));
    for (const XrExtensionProperties& extension : extensions)
    {
        AR_LOG_INFO(std::format("  {} (version {})", extension.extensionName, extension.extensionVersion));
    }
    AR_LOG_INFO("M9A enables zero extensions (no graphics binding, hand/eye tracking, "
                "passthrough, or spatial anchors yet)");

    // --- Instance creation ---
    OpenXRInstance instance;
    if (!instance.IsValid())
    {
        // Case A vs. a genuinely unexpected failure - see the file
        // header comment and docs/ARCHITECTURE.md, "Instance Creation
        // Failure Handling (M9A)". Either way: log clearly, exit
        // cleanly, do not crash.
        if (instance.CreationResult() == XR_ERROR_RUNTIME_UNAVAILABLE)
        {
            AR_LOG_WARNING(
                "No active OpenXR runtime found (XR_ERROR_RUNTIME_UNAVAILABLE) - this machine has the "
                "OpenXR loader available but no runtime installed/active (no headset software running). "
                "This is a normal, expected outcome on a desktop dev machine without XR hardware set up.");
        }
        else
        {
            AR_LOG_WARNING(std::format(
                "xrCreateInstance failed unexpectedly: {} - cannot continue OpenXR bring-up",
                XrResultToReadableString(XR_NULL_HANDLE, instance.CreationResult())));
        }
        AR_LOG_INFO("OpenXR bring-up demo exiting cleanly (no instance available)");
        return 0;
    }

    // --- Runtime information (instance now valid; failure here would
    // be genuinely unexpected, so this is allowed to assert) ---
    XrInstanceProperties instanceProperties{};
    instanceProperties.type = XR_TYPE_INSTANCE_PROPERTIES;
    CheckXrResult(instance.Get(), xrGetInstanceProperties(instance.Get(), &instanceProperties), "xrGetInstanceProperties");
    AR_LOG_INFO(std::format("Active OpenXR runtime: {} (version {})",
                             instanceProperties.runtimeName, FormatXrVersion(instanceProperties.runtimeVersion)));

    // --- System selection: request an HMD-class system ---
    const SystemRequestResult systemResult = TryGetHmdSystem(instance.Get());
    if (!systemResult.found)
    {
        // Case B vs. a genuinely unexpected xrGetSystem failure.
        if (IsFormFactorUnavailable(systemResult.rawResult))
        {
            AR_LOG_WARNING(
                "OpenXR runtime is active, but no head-mounted-display system is currently available "
                "(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY unavailable) - the runtime is installed and "
                "responding, but no headset appears to be connected/active right now.");
        }
        else
        {
            AR_LOG_WARNING(std::format("xrGetSystem failed unexpectedly: {}",
                                        XrResultToReadableString(instance.Get(), systemResult.rawResult)));
        }
        AR_LOG_INFO("OpenXR bring-up demo exiting cleanly (runtime present, no HMD system)");
        return 0;
    }

    AR_LOG_INFO(std::format("HMD system acquired: XrSystemId {}", systemResult.systemId));

    // --- System properties (systemId now valid; failure here would
    // also be genuinely unexpected) ---
    XrSystemProperties systemProperties{};
    systemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
    CheckXrResult(instance.Get(),
        xrGetSystemProperties(instance.Get(), systemResult.systemId, &systemProperties), "xrGetSystemProperties");

    AR_LOG_INFO(std::format("System name: {}", systemProperties.systemName));
    AR_LOG_INFO(std::format("Vendor ID: {}", systemProperties.vendorId));
    AR_LOG_INFO(std::format("Max swapchain image size: {}x{}",
                             systemProperties.graphicsProperties.maxSwapchainImageWidth,
                             systemProperties.graphicsProperties.maxSwapchainImageHeight));
    AR_LOG_INFO(std::format("Max composition layer count: {}", systemProperties.graphicsProperties.maxLayerCount));
    AR_LOG_INFO(std::format("Orientation tracking: {}, Position tracking: {}",
                             systemProperties.trackingProperties.orientationTracking != XR_FALSE,
                             systemProperties.trackingProperties.positionTracking != XR_FALSE));

    AR_LOG_INFO("OpenXR bring-up complete - shutting down");
    return 0;

    // `instance` is destroyed here, automatically (xrDestroyInstance,
    // via OpenXRInstance's destructor) — no XrSession, swapchain, or
    // reference space ever existed to destroy first. systemResult's
    // XrSystemId is never separately destroyed - it is not a resource
    // OpenXR owns, just an opaque identifier valid for this instance's
    // lifetime. See docs/ARCHITECTURE.md, "RAII / Destruction (M9A)".
}
