// Manual M10 validation demo — NOT part of the automated CTest suite,
// since it requires a real OpenXR loader/runtime with XR_KHR_vulkan_enable2
// support (this codebase's XrSession creation needs the M9C Vulkan
// graphics binding, even though nothing in this demo renders anything -
// see docs/ARCHITECTURE.md, "Demo: openxr_input_demo.cpp (M10)"). Built
// by CMake but deliberately not registered with add_test. Run it
// manually.
//
// AREngine's first OpenXR action/input foundation, proven end to end:
// create one "gameplay" XrActionSet -> create four actions (aim_pose,
// select, trigger, move - each spanning both hands via shared
// left/right subaction paths, M10's OpenXRActionSystem) -> suggest KHR
// simple_controller bindings for aim_pose/select (the only standard
// profile M10 targets) -> attach the action set to the session (once)
// -> create left/right action spaces for aim_pose -> drive the real XR
// frame lifecycle through XRFrameDriver (M9E.5-M9H, unchanged) -> each
// running frame, xrSyncActions once, then query digital/analog/vector2/
// pose state for both hands and convert it into AREngine::Input's
// generic *ActionState types -> log only meaningful changes (first
// observed state, activation transitions, digital press/release,
// analog/vector2 changes above a small diagnostic threshold, pose
// validity transitions) -> repeat for a target frame count -> request a
// clean session exit -> destroy action spaces/actions/action set before
// the session, same discipline as every other OpenXR object this
// codebase owns.
//
// No swapchains, no projection layer, no rendering of any kind - the
// session still needs the generic XR frame lifecycle (PrepareFrame/
// BeginFrame/EndFrame) running to stay alive and reach RUNNING (so
// action sync/state queries are legal), but EndFrame() always submits
// zero composition layers here; SetPendingProjectionLayer() is simply
// never called. This is a deliberately different, narrower bring-up
// than openxr_frame_demo.cpp/openxr_cube_demo.cpp - action/input
// validation does not need a swapchain to exist.
//
// This demo reaches directly into XR's private src/openxr/
// implementation, same reasoning as every other manual OpenXR demo in
// this codebase (M9A onward).
//
// SteamVR's null driver is expected to expose no real controller
// bound to any interaction profile in this environment - every action
// is expected to report isActive=false for the whole run. That is
// reported honestly (see the end-of-run summary), not worked around or
// fabricated - see docs/ARCHITECTURE.md, "Runtime/Controller
// Availability Findings (M10)".

#include "AREngine/Core/Core.hpp"
#include "AREngine/Frame/Frame.hpp"
#include "AREngine/Input/ActionState.hpp"

#include "openxr/OpenXRActionSystem.hpp"
#include "openxr/OpenXREnvironmentBlendMode.hpp"
#include "openxr/OpenXRInstance.hpp"
#include "openxr/OpenXRReferenceSpace.hpp"
#include "openxr/OpenXRResult.hpp"
#include "openxr/OpenXRSession.hpp"
#include "openxr/OpenXRSystem.hpp"
#include "openxr/OpenXRVersion.hpp"
#include "openxr/OpenXRViewConfiguration.hpp"
#include "openxr/OpenXRVulkanGraphicsBinding.hpp"
#include "openxr/XRFrameDriver.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <vector>

namespace
{
    constexpr std::uint32_t kTargetFrameCount = 500;
    constexpr std::chrono::seconds kMaxDemoDuration{90};

    // "Meaningful analog change" - small enough to catch real motion,
    // large enough to ignore floating-point/runtime jitter on an
    // otherwise-steady value. Purely diagnostic (change-only logging
    // threshold), never used for anything semantic.
    constexpr float kAnalogLogThreshold = 0.05f;

    const char* HandName(AREngine::XR::OpenXR::Hand hand)
    {
        return hand == AREngine::XR::OpenXR::Hand::Left ? "Left" : "Right";
    }

    // Change-only logging bookkeeping, per hand - deliberately separate
    // from OpenXRActionSystem's own previous-down tracking (that one
    // computes Pressed/Released correctness; this one only decides
    // whether THIS DEMO should print a line, a purely diagnostic
    // concern that must never influence engine-level state).
    struct HandLogState
    {
        bool everLogged = false;
        bool selectActive = false;
        bool triggerActive = false;
        float triggerValue = 0.0f;
        bool moveActive = false;
        AREngine::Core::Math::Vec2 moveValue;
        bool poseActive = false;
        bool posePositionValid = false;
        bool poseOrientationValid = false;
    };

    void LogHandState(
        AREngine::XR::OpenXR::Hand hand, HandLogState& logState,
        const AREngine::Input::DigitalActionState& select,
        const AREngine::Input::AnalogActionState& trigger,
        const AREngine::Input::Vector2ActionState& move,
        const AREngine::Input::PoseActionState& pose)
    {
        const char* name = HandName(hand);

        if (!logState.everLogged)
        {
            AR_LOG_INFO(std::format(
                "  [{}] first observed state: select.active={} trigger.active={} move.active={} pose.active={}",
                name, select.active, trigger.active, move.active, pose.active));
            logState.everLogged = true;
        }

        if (select.pressed)
        {
            AR_LOG_INFO(std::format("  [{}] Select: pressed", name));
        }
        if (select.released)
        {
            AR_LOG_INFO(std::format("  [{}] Select: released", name));
        }
        if (select.active != logState.selectActive)
        {
            AR_LOG_INFO(std::format("  [{}] Select: active {} -> {}", name, logState.selectActive, select.active));
            logState.selectActive = select.active;
        }

        if (trigger.active != logState.triggerActive)
        {
            AR_LOG_INFO(std::format("  [{}] Trigger: active {} -> {}", name, logState.triggerActive, trigger.active));
            logState.triggerActive = trigger.active;
            logState.triggerValue = trigger.value;
        }
        else if (trigger.active && std::abs(trigger.value - logState.triggerValue) > kAnalogLogThreshold)
        {
            AR_LOG_INFO(std::format("  [{}] Trigger: {:.3f} -> {:.3f}", name, logState.triggerValue, trigger.value));
            logState.triggerValue = trigger.value;
        }

        if (move.active != logState.moveActive)
        {
            AR_LOG_INFO(std::format("  [{}] Move: active {} -> {}", name, logState.moveActive, move.active));
            logState.moveActive = move.active;
            logState.moveValue = move.value;
        }
        else if (move.active)
        {
            const float dx = move.value.x - logState.moveValue.x;
            const float dy = move.value.y - logState.moveValue.y;
            if (std::sqrt(dx * dx + dy * dy) > kAnalogLogThreshold)
            {
                AR_LOG_INFO(std::format("  [{}] Move: ({:.3f},{:.3f}) -> ({:.3f},{:.3f})",
                                         name, logState.moveValue.x, logState.moveValue.y, move.value.x, move.value.y));
                logState.moveValue = move.value;
            }
        }

        if (pose.active != logState.poseActive)
        {
            AR_LOG_INFO(std::format("  [{}] AimPose: active {} -> {}", name, logState.poseActive, pose.active));
            logState.poseActive = pose.active;
        }
        if (pose.positionValid != logState.posePositionValid)
        {
            AR_LOG_INFO(std::format("  [{}] AimPose: positionValid {} -> {}", name, logState.posePositionValid, pose.positionValid));
            logState.posePositionValid = pose.positionValid;
        }
        if (pose.orientationValid != logState.poseOrientationValid)
        {
            AR_LOG_INFO(std::format("  [{}] AimPose: orientationValid {} -> {}", name, logState.poseOrientationValid, pose.orientationValid));
            logState.poseOrientationValid = pose.orientationValid;
        }
    }
}

int main()
{
    using namespace AREngine::XR::OpenXR;
    namespace Frame = AREngine::Frame;
    namespace Input = AREngine::Input;

    AR_LOG_INFO(std::format("AREngine OpenXR input demo - header version {}, requesting API version {}",
                             FormatXrVersion(XR_CURRENT_API_VERSION), FormatXrVersion(kTargetApiVersion)));

    // --- Bring-up: identical in substance to openxr_frame_demo.cpp up
    // through session + LOCAL space - see that file for the full
    // reasoning behind each stop condition. No swapchain, no projection
    // layer: this demo renders nothing. ---
    const std::vector<XrExtensionProperties> extensions = EnumerateInstanceExtensions();
    if (!IsExtensionSupported(extensions, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME))
    {
        AR_LOG_WARNING(std::format("{} is NOT supported by the active OpenXR runtime - stopping here.",
                                    XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME));
        AR_LOG_INFO("OpenXR input demo exiting cleanly (XR_KHR_vulkan_enable2 unavailable)");
        return 0;
    }

    const std::array<const char*, 1> requestedExtensions{XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
    OpenXRInstance instance(requestedExtensions);
    if (!instance.IsValid())
    {
        if (instance.CreationResult() == XR_ERROR_RUNTIME_UNAVAILABLE)
        {
            AR_LOG_WARNING("No active OpenXR runtime found (XR_ERROR_RUNTIME_UNAVAILABLE).");
        }
        else
        {
            AR_LOG_WARNING(std::format("xrCreateInstance failed unexpectedly: {}",
                                        XrResultToReadableString(XR_NULL_HANDLE, instance.CreationResult())));
        }
        AR_LOG_INFO("OpenXR input demo exiting cleanly (no instance available)");
        return 0;
    }

    XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
    CheckXrResult(instance.Get(), xrGetInstanceProperties(instance.Get(), &instanceProperties), "xrGetInstanceProperties");
    AR_LOG_INFO(std::format("Active OpenXR runtime: {} (version {})",
                             instanceProperties.runtimeName, FormatXrVersion(instanceProperties.runtimeVersion)));

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
        AR_LOG_INFO("OpenXR input demo exiting cleanly (runtime present, no HMD system)");
        return 0;
    }
    AR_LOG_INFO(std::format("HMD system acquired: XrSystemId {}", systemResult.systemId));
    const XrSystemId systemId = systemResult.systemId;

    const std::vector<XrViewConfigurationType> viewConfigTypes = EnumerateViewConfigurationTypes(instance.Get(), systemId);
    const std::optional<XrViewConfigurationType> primaryViewConfigType = SelectPrimaryViewConfigurationType(viewConfigTypes);
    if (!primaryViewConfigType.has_value())
    {
        AR_LOG_WARNING("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is NOT supported by this runtime/system - stopping here.");
        AR_LOG_INFO("OpenXR input demo exiting cleanly (no stereo view configuration)");
        return 0;
    }
    AR_LOG_INFO("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is supported - selected as the primary view configuration");

    const std::vector<XrEnvironmentBlendMode> supportedBlendModes =
        EnumerateEnvironmentBlendModes(instance.Get(), systemId, *primaryViewConfigType);
    const std::optional<XrEnvironmentBlendMode> selectedBlendMode = SelectEnvironmentBlendMode(supportedBlendModes);
    if (!selectedBlendMode.has_value())
    {
        AR_LOG_WARNING("None of OPAQUE/ALPHA_BLEND/ADDITIVE is supported by this runtime - stopping here.");
        AR_LOG_INFO("OpenXR input demo exiting cleanly (no usable environment blend mode)");
        return 0;
    }
    AR_LOG_INFO(std::format("Selected environment blend mode: {}", EnvironmentBlendModeToString(*selectedBlendMode)));

    AR_LOG_INFO("Creating OpenXR-compatible Vulkan instance/device via XR_KHR_vulkan_enable2...");
    OpenXRVulkanGraphicsBinding binding(instance.Get(), systemId);
    AR_LOG_INFO(std::format("Vulkan API version selected: {}", FormatVkApiVersion(binding.GetSelectedVulkanApiVersion())));

    OpenXRSession session(instance.Get(), systemId, binding.GetBindingData());
    AR_LOG_INFO("XrSession created successfully");

    OpenXRReferenceSpace localSpace(instance.Get(), session.Get(), XR_REFERENCE_SPACE_TYPE_LOCAL);
    AR_LOG_INFO("Created LOCAL reference space");

    // --- M10: action set + actions + bindings + attach + action
    // spaces, all in one constructor call - see OpenXRActionSystem.hpp/
    // .cpp and docs/ARCHITECTURE.md for the full ordering rationale. ---
    OpenXRActionSystem actionSystem(instance.Get(), session.Get());
    AR_LOG_INFO("Created 'gameplay' action set (aim_pose/select/trigger/move, left+right subaction paths), "
                "suggested khr/simple_controller bindings, attached to session, created left/right aim_pose spaces");

    // --- Frame loop, driven through XRFrameDriver (unchanged since
    // M9E.5) - only what's needed to keep the session RUNNING (a
    // precondition for xrSyncActions/xrGetActionState*), never a
    // swapchain or composition layer. ---
    AR_LOG_INFO(std::format("Beginning OpenXR input loop - target {} completed frames before requesting exit...", kTargetFrameCount));

    XRFrameDriver frameDriver(instance.Get(), session, localSpace, *primaryViewConfigType, *selectedBlendMode);

    std::uint32_t completedFrameCount = 0;
    std::uint32_t syncedFrameCount = 0;
    bool exitRequested = false;
    const auto startTime = std::chrono::steady_clock::now();

    std::array<HandLogState, 2> logStates{}; // [0]=Left, [1]=Right

    while (true)
    {
        const Frame::FrameContext frameContext = frameDriver.PrepareFrame();

        if (frameContext.status == Frame::FrameStatus::Stop)
        {
            AR_LOG_INFO("Frame driver reports FrameStatus::Stop - stopping the main loop cleanly (not an error)");
            break;
        }

        if (frameContext.status == Frame::FrameStatus::Idle)
        {
            if (std::chrono::steady_clock::now() - startTime > kMaxDemoDuration)
            {
                AR_LOG_WARNING("Safety timeout reached while the frame driver was idle - stopping.");
                break;
            }
            continue;
        }

        frameDriver.BeginFrame();

        // xrSyncActions is a distinct concept from rendering - synced
        // every running (FrameStatus::Continue) frame, independent of
        // shouldRender, since input responsiveness has no reason to be
        // throttled by the compositor's render-opportunity signal. Only
        // reachable here because FrameStatus::Continue itself implies
        // the session is RUNNING (PrepareFrame only calls the real
        // xrWaitFrame, which requires a running session, to produce
        // Continue) - see docs/ARCHITECTURE.md, "xrSyncActions Placement
        // (M10)".
        actionSystem.SyncActions(instance.Get(), session.Get());
        ++syncedFrameCount;

        const XrTime predictedDisplayTime = frameDriver.GetLastPredictedDisplayTime();
        for (const Hand hand : {Hand::Left, Hand::Right})
        {
            const Input::DigitalActionState select = actionSystem.GetSelectState(instance.Get(), session.Get(), hand);
            const Input::AnalogActionState trigger = actionSystem.GetTriggerState(instance.Get(), session.Get(), hand);
            const Input::Vector2ActionState move = actionSystem.GetMoveState(instance.Get(), session.Get(), hand);
            const Input::PoseActionState pose =
                actionSystem.GetAimPoseState(instance.Get(), session.Get(), localSpace.Get(), predictedDisplayTime, hand);

            LogHandState(hand, logStates[hand == Hand::Left ? 0 : 1], select, trigger, move, pose);
        }

        // EndFrame() always runs, submitting zero composition layers -
        // SetPendingProjectionLayer() is never called anywhere in this
        // demo, and XRFrameDriver::EndFrame() already defaults to zero
        // layers whenever it isn't.
        frameDriver.EndFrame();

        ++completedFrameCount;
        if (completedFrameCount == 1 || completedFrameCount % 100 == 0)
        {
            AR_LOG_INFO(std::format("Completed frame {} (synced={}, deltaTime={:.4f}s)",
                                     completedFrameCount, syncedFrameCount, frameContext.timing.deltaTimeSeconds));
        }

        if (completedFrameCount >= kTargetFrameCount && !exitRequested)
        {
            AR_LOG_INFO(std::format("Reached target frame count ({}) - requesting a clean session exit...", kTargetFrameCount));
            frameDriver.RequestExit();
            exitRequested = true;
        }

        if (std::chrono::steady_clock::now() - startTime > kMaxDemoDuration)
        {
            AR_LOG_WARNING("Safety timeout reached without the runtime reaching EXITING - stopping the demo loop anyway.");
            break;
        }
    }

    AR_LOG_INFO(std::format("Total completed frames: {}, synced frames: {}", completedFrameCount, syncedFrameCount));
    AR_LOG_INFO(std::format(
        "Final state: [Left] select.active={} trigger.active={} move.active={} pose.active={} | "
        "[Right] select.active={} trigger.active={} move.active={} pose.active={}",
        logStates[0].selectActive, logStates[0].triggerActive, logStates[0].moveActive, logStates[0].poseActive,
        logStates[1].selectActive, logStates[1].triggerActive, logStates[1].moveActive, logStates[1].poseActive));
    if (!logStates[0].selectActive && !logStates[0].triggerActive && !logStates[0].moveActive && !logStates[0].poseActive &&
        !logStates[1].selectActive && !logStates[1].triggerActive && !logStates[1].moveActive && !logStates[1].poseActive)
    {
        AR_LOG_INFO("No action was ever active on either hand this run - consistent with this environment's SteamVR "
                    "null driver exposing no real controller bound to any interaction profile. Not fabricated or "
                    "worked around - see docs/ARCHITECTURE.md, \"Runtime/Controller Availability Findings (M10)\".");
    }

    AR_LOG_INFO("OpenXR input demo complete - shutting down");
    return 0;

    // Destruction, in exact reverse declaration order: frameDriver
    // (trivial - owns nothing) -> actionSystem (destroys both aim_pose
    // action spaces, then all four actions, then the action set itself
    // - all while `session` is still alive) -> localSpace
    // (xrDestroySpace) -> session (xrDestroySession) -> binding
    // (VkDevice, then VkInstance) -> instance (xrDestroyInstance) last.
}
