// Manual M9D validation demo — NOT part of the automated CTest suite,
// since it requires a real OpenXR loader/runtime with XR_KHR_vulkan_enable2
// support (and ideally a real or simulated HMD) that CI/headless systems
// may lack. Built by CMake but deliberately not registered with
// add_test. Run it manually.
//
// Proves AREngine's first real XrSession end to end: create the M9C
// Vulkan graphics binding -> create XrSession from it -> enumerate view
// configurations and select PRIMARY_STEREO -> enumerate + create
// reference spaces -> poll session-state events -> call xrBeginSession
// on READY -> request a clean exit once the session reaches a stable
// running state -> observe the runtime drive STOPPING/EXITING -> clean
// shutdown. Does NOT create any XR swapchain, does NOT call
// xrWaitFrame/xrBeginFrame/xrEndFrame, does NOT call xrLocateViews or
// xrLocateSpace, does NOT render anything — M9D is session lifecycle
// bring-up only, see docs/ROADMAP.md.
//
// This demo reaches directly into XR's private src/openxr/
// implementation, same reasoning as M9A/M9C's demos.
//
// Same outcome-distinguishing discipline M9A/M9C established (see
// docs/ARCHITECTURE.md, "Headset Absent Case (M9A)" and
// "XR_KHR_vulkan_enable2 Selection (M9C)"): no runtime, no HMD system,
// and (new this milestone) no stereo view configuration are all
// reported clearly and stopped on cleanly, never crashed on.

#include "AREngine/Core/Core.hpp"

#include "openxr/OpenXRInstance.hpp"
#include "openxr/OpenXRReferenceSpace.hpp"
#include "openxr/OpenXRResult.hpp"
#include "openxr/OpenXRSession.hpp"
#include "openxr/OpenXRSessionState.hpp"
#include "openxr/OpenXRSystem.hpp"
#include "openxr/OpenXRVersion.hpp"
#include "openxr/OpenXRViewConfiguration.hpp"
#include "openxr/OpenXRVulkanGraphicsBinding.hpp"

#include <array>
#include <chrono>
#include <format>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace
{
    // M11.3 diagnostic-only teardown tracer - see tests/xr_demo.cpp's
    // own copy for the full reasoning (std::cerr, unit-buffered plus
    // explicit flush, survives a crash that would lose std::cout's
    // buffered content). Temporary, removed once the Meta clean-exit
    // crash is root-caused.
    struct TeardownMarker
    {
        const char* label;
        explicit TeardownMarker(const char* l) : label(l) {}
        ~TeardownMarker()
        {
            std::cerr << "[TEARDOWN] about to destroy: " << label << std::endl;
            std::cerr.flush();
        }
    };

    // Demo-only formatting for reference space types, not part of the
    // reusable OpenXRReferenceSpace library surface - see M9C's
    // VkPhysicalDeviceTypeToString for the same "small integration
    // object/layer used by the dedicated demo" reasoning.
    std::string ReferenceSpaceTypeToString(XrReferenceSpaceType type)
    {
        switch (type)
        {
            case XR_REFERENCE_SPACE_TYPE_VIEW:  return "VIEW";
            case XR_REFERENCE_SPACE_TYPE_LOCAL: return "LOCAL";
            case XR_REFERENCE_SPACE_TYPE_STAGE: return "STAGE";
            default:                             return std::format("OTHER({})", static_cast<int>(type));
        }
    }

    // Safety ceiling so this demo can never hang indefinitely if a
    // runtime produces an unexpected event sequence - not part of
    // OpenXR's own lifecycle, purely a manual-test convenience. Chosen
    // generously (SteamVR's own state machine settles in well under a
    // second in practice).
    constexpr std::chrono::seconds kMaxDemoDuration{30};
}

int main()
{
    using namespace AREngine::XR::OpenXR;

    AR_LOG_INFO(std::format("AREngine OpenXR session demo - header version {}, requesting API version {}",
                             FormatXrVersion(XR_CURRENT_API_VERSION), FormatXrVersion(kTargetApiVersion)));

    // --- Instance extensions: same XR_KHR_vulkan_enable2 check M9C
    // established - a graphics XrSession still needs a graphics
    // binding, and M9D reuses that exact requirement unchanged. ---
    const std::vector<XrExtensionProperties> extensions = EnumerateInstanceExtensions();
    if (!IsExtensionSupported(extensions, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME))
    {
        AR_LOG_WARNING(std::format("{} is NOT supported by the active OpenXR runtime - M9D's session requires "
                                    "the M9C Vulkan graphics binding, which requires this extension. Stopping here.",
                                    XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME));
        AR_LOG_INFO("OpenXR session demo exiting cleanly (XR_KHR_vulkan_enable2 unavailable)");
        return 0;
    }

    // --- Instance creation ---
    //
    // Declared first, before every other OpenXR/Vulkan/session object
    // below - see docs/ARCHITECTURE.md, "Destruction Order (M9D)".
    // C++'s reverse-local-destruction-order rule destroys everything
    // below in exactly the opposite order it is declared here, which
    // is precisely the order OpenXR/Vulkan require: reference spaces,
    // then session, then Vulkan graphics binding, then instance, last.
    const std::array<const char*, 1> requestedExtensions{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    OpenXRInstance instance(requestedExtensions);
    const TeardownMarker teardownInstance("instance (xrDestroyInstance)");
    if (!instance.IsValid())
    {
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
        AR_LOG_INFO("OpenXR session demo exiting cleanly (no instance available)");
        return 0;
    }

    XrInstanceProperties instanceProperties{};
    instanceProperties.type = XR_TYPE_INSTANCE_PROPERTIES;
    CheckXrResult(instance.Get(), xrGetInstanceProperties(instance.Get(), &instanceProperties), "xrGetInstanceProperties");
    AR_LOG_INFO(std::format("Active OpenXR runtime: {} (version {})",
                             instanceProperties.runtimeName, FormatXrVersion(instanceProperties.runtimeVersion)));

    // --- System selection ---
    const SystemRequestResult systemResult = TryGetHmdSystem(instance.Get());
    if (!systemResult.found)
    {
        if (IsFormFactorUnavailable(systemResult.rawResult))
        {
            AR_LOG_WARNING("OpenXR runtime is active, but no head-mounted-display system is currently available.");
        }
        else
        {
            AR_LOG_WARNING(std::format("xrGetSystem failed unexpectedly: {}",
                                        XrResultToReadableString(instance.Get(), systemResult.rawResult)));
        }
        AR_LOG_INFO("OpenXR session demo exiting cleanly (runtime present, no HMD system)");
        return 0;
    }
    AR_LOG_INFO(std::format("HMD system acquired: XrSystemId {}", systemResult.systemId));
    const XrSystemId systemId = systemResult.systemId;

    // --- View configuration: enumerate, then require PRIMARY_STEREO -
    // reported clearly and stopped on if unavailable, per
    // docs/ARCHITECTURE.md, "Selected Primary View Configuration (M9D)". ---
    const std::vector<XrViewConfigurationType> viewConfigTypes = EnumerateViewConfigurationTypes(instance.Get(), systemId);
    AR_LOG_INFO(std::format("Supported view configuration types: {}", viewConfigTypes.size()));
    for (const XrViewConfigurationType type : viewConfigTypes)
    {
        AR_LOG_INFO(std::format("  {}", static_cast<int>(type)));
    }

    const std::optional<XrViewConfigurationType> primaryViewConfigType = SelectPrimaryViewConfigurationType(viewConfigTypes);
    if (!primaryViewConfigType.has_value())
    {
        AR_LOG_WARNING("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is NOT supported by this runtime/system - "
                        "M9D requires it to begin a session. Stopping here.");
        AR_LOG_INFO("OpenXR session demo exiting cleanly (no stereo view configuration)");
        return 0;
    }
    AR_LOG_INFO("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is supported - selected as the primary view configuration");

    const XrViewConfigurationProperties viewConfigProperties =
        GetViewConfigurationProperties(instance.Get(), systemId, *primaryViewConfigType);
    AR_LOG_INFO(std::format("View configuration fovMutable: {}", viewConfigProperties.fovMutable != XR_FALSE));

    const std::vector<XrViewConfigurationView> viewConfigViews =
        EnumerateViewConfigurationViews(instance.Get(), systemId, *primaryViewConfigType);
    AR_LOG_INFO(std::format("View count: {}", viewConfigViews.size()));
    for (std::size_t i = 0; i < viewConfigViews.size(); ++i)
    {
        const XrViewConfigurationView& view = viewConfigViews[i];
        AR_LOG_INFO(std::format("  View {}: recommended {}x{} (max {}x{}), recommended sample count {} (max {})",
                                 i, view.recommendedImageRectWidth, view.recommendedImageRectHeight,
                                 view.maxImageRectWidth, view.maxImageRectHeight,
                                 view.recommendedSwapchainSampleCount, view.maxSwapchainSampleCount));
    }

    // --- M9C: XR-compatible Vulkan graphics binding (unchanged from
    // M9C - no second Vulkan device is created for M9D). ---
    AR_LOG_INFO("Creating OpenXR-compatible Vulkan instance/device via XR_KHR_vulkan_enable2...");
    OpenXRVulkanGraphicsBinding binding(instance.Get(), systemId);
    const TeardownMarker teardownBinding("binding (debug messenger, then vkDestroyDevice, then vkDestroyInstance)");
    AR_LOG_INFO(std::format("Vulkan API version selected: {}", FormatVkApiVersion(binding.GetSelectedVulkanApiVersion())));

    // --- M9D: XrSession, created from the M9C graphics binding. ---
    OpenXRSession session(instance.Get(), systemId, binding.GetBindingData());
    const TeardownMarker teardownSession("session (xrDestroySession)");
    AR_LOG_INFO("XrSession created successfully");

    // --- Reference spaces: enumerate supported types, then create
    // whichever of VIEW/LOCAL/STAGE the runtime actually supports -
    // see docs/ARCHITECTURE.md, "Reference Spaces (M9D)". Declared
    // AFTER `session` so they are destroyed BEFORE it (reverse local
    // destruction order) - required, not incidental; see
    // "Destruction Order (M9D)". ---
    const std::vector<XrReferenceSpaceType> supportedSpaceTypes = EnumerateReferenceSpaceTypes(instance.Get(), session.Get());
    AR_LOG_INFO(std::format("Supported reference space types: {}", supportedSpaceTypes.size()));
    for (const XrReferenceSpaceType type : supportedSpaceTypes)
    {
        AR_LOG_INFO(std::format("  {}", ReferenceSpaceTypeToString(type)));
    }

    const std::vector<XrReferenceSpaceType> spaceTypesToCreate = SelectReferenceSpacesToCreate(supportedSpaceTypes);
    std::vector<std::unique_ptr<OpenXRReferenceSpace>> referenceSpaces;
    referenceSpaces.reserve(spaceTypesToCreate.size());
    for (const XrReferenceSpaceType type : spaceTypesToCreate)
    {
        referenceSpaces.push_back(std::make_unique<OpenXRReferenceSpace>(instance.Get(), session.Get(), type));
        AR_LOG_INFO(std::format("Created {} reference space", ReferenceSpaceTypeToString(type)));
    }
    const TeardownMarker teardownReferenceSpaces("referenceSpaces (each xrDestroySpace)");
    if (!IsReferenceSpaceTypeSupported(supportedSpaceTypes, XR_REFERENCE_SPACE_TYPE_STAGE))
    {
        AR_LOG_INFO("STAGE reference space is not supported by this runtime - not created (AREngine does not require it)");
    }

    // --- Session-state event loop ---
    AR_LOG_INFO("Polling session state events (SteamVR/null HMD)...");

    XrSessionState currentState = XR_SESSION_STATE_UNKNOWN;
    std::vector<XrSessionState> observedStates;
    bool exitRequested = false;
    const auto startTime = std::chrono::steady_clock::now();

    while (true)
    {
        const SessionEventPollResult pollResult = PollSessionEvents(instance.Get());

        if (pollResult.instanceLossPending)
        {
            AR_LOG_WARNING("XrEventDataInstanceLossPending received - the OpenXR instance itself is about to "
                            "become invalid. Stopping cleanly (no recreation attempted in M9D).");
            break;
        }

        if (pollResult.sessionStateChanged)
        {
            currentState = pollResult.newSessionState;
            observedStates.push_back(currentState);
            AR_LOG_INFO(std::format("Session state changed -> {}", FormatSessionState(currentState)));

            if (ShouldBeginSession(currentState))
            {
                session.BeginSession(*primaryViewConfigType);
                AR_LOG_INFO("xrBeginSession succeeded - session is now running");

                // "Diagnostics complete, request a clean exit" fires
                // here - right after the session starts running - NOT
                // on reaching FOCUSED. Observed empirically: SYNCHRONIZED
                // (and therefore VISIBLE/FOCUSED, which both require it)
                // is only reached once the app participates in the
                // frame loop (xrWaitFrame) - which M9D explicitly must
                // not call. Without a frame loop, this session can only
                // ever reach READY -> running, and would otherwise sit
                // there until the safety timeout. See
                // docs/ARCHITECTURE.md, "How The Demo Ends (M9D)" for
                // the full reasoning and what was actually observed.
                if (!exitRequested)
                {
                    AR_LOG_INFO("Session is running - diagnostics complete, requesting a clean session exit...");
                    session.RequestExit();
                    exitRequested = true;
                }
            }
            else if (ShouldEndSession(currentState, session.IsRunning()))
            {
                session.EndSession();
                AR_LOG_INFO("xrEndSession succeeded - session is no longer running");
            }
        }

        if (ShouldStopMainLoop(currentState))
        {
            AR_LOG_INFO(std::format(
                "{} - stopping the main loop cleanly (not an error)", FormatSessionState(currentState)));
            break;
        }

        if (std::chrono::steady_clock::now() - startTime > kMaxDemoDuration)
        {
            AR_LOG_WARNING("Safety timeout reached without the runtime reaching EXITING - stopping the demo loop "
                            "anyway (this is a manual-test convenience, not part of OpenXR's own lifecycle).");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::string observedSequence;
    for (std::size_t i = 0; i < observedStates.size(); ++i)
    {
        if (i != 0)
        {
            observedSequence += " -> ";
        }
        observedSequence += FormatSessionState(observedStates[i]);
    }
    AR_LOG_INFO(std::format("Observed session state sequence: {}", observedSequence));

    if (session.IsRunning() && ShouldEndSession(currentState, session.IsRunning()))
    {
        // Only reached if the loop broke out (e.g. the safety timeout)
        // in the same event-poll cycle STOPPING arrived but before the
        // top-of-loop ShouldStopMainLoop check ran again - xrEndSession
        // is only ever called when the tracked state is genuinely
        // STOPPING, per its own precondition (calling it from any other
        // state returns XR_ERROR_SESSION_NOT_RUNNING - confirmed the
        // hard way against SteamVR's null driver during this
        // milestone's own manual validation). See
        // docs/ARCHITECTURE.md, "STOPPING (M9D)".
        AR_LOG_WARNING("Loop exited with the session still marked running and state STOPPING - calling xrEndSession");
        session.EndSession();
    }
    else if (session.IsRunning())
    {
        // Still running, but never reached STOPPING (e.g. the safety
        // timeout fired first, or xrRequestExitSession's effect never
        // arrived in this run) - xrEndSession must NOT be called from
        // here (would fail XR_ERROR_SESSION_NOT_RUNNING). Destroying a
        // still-running session directly is spec-legal; OpenXRSession's
        // destructor (xrDestroySession) below handles it.
        AR_LOG_WARNING("Loop exited with the session still marked running, but STOPPING was never observed - "
                        "destroying the session directly (xrEndSession is not valid outside STOPPING)");
    }

    AR_LOG_INFO("OpenXR session demo complete - shutting down");
    std::cerr << "[TEARDOWN] entering automatic (RAII) destructor chain now" << std::endl;
    std::cerr.flush();
    return 0;

    // Destruction, in order (see the comment above `instance`'s
    // declaration): `referenceSpaces` (each xrDestroySpace), then
    // `session` (xrDestroySession), then `binding` (its VkDevice, then
    // VkInstance - the session no longer references them once
    // destroyed), then `instance` (xrDestroyInstance) last.
}
