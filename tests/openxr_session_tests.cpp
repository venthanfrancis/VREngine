// M9D automated tests for AREngine::XR::OpenXR's session-lifecycle
// pure-logic helpers: session-state formatting/decisions, view-
// configuration selection, and reference-space selection.
// Deliberately calls ZERO real OpenXR API functions (no xrCreateSession,
// no xrPollEvent, ...) and has ZERO Vulkan dependency (session *state*,
// view configuration, and reference spaces are all pure OpenXR
// concepts, independent of which graphics API eventually backs the
// session - see docs/ARCHITECTURE.md, "M9D Implementation Notes"), so
// this runs on any machine with the OpenXR headers available at
// compile time, without needing a real OpenXR runtime, headset, GPU, or
// even Vulkan enabled.
//
// M9E.5 adds DetermineFrameStatus/DetermineSessionLifecycleActions
// coverage - still zero real OpenXR/Vulkan calls, but this file (like
// OpenXRSessionState.hpp/.cpp itself) now depends on AREngine::Frame
// for Frame::FrameStatus - see docs/ARCHITECTURE.md, "Where
// DetermineFrameStatus Lives (M9E.5)".
//
// Real OpenXR session bring-up (session/space creation and event
// polling against a real loader/runtime) is exercised only by the
// separate, manual arengine_openxr_session_demo — not part of this
// suite, since CTest must not depend on an XR runtime or headset being
// present.

#include "openxr/OpenXREnvironmentBlendMode.hpp"
#include "openxr/OpenXRFrameTiming.hpp"
#include "openxr/OpenXRReferenceSpace.hpp"
#include "openxr/OpenXRSessionState.hpp"
#include "openxr/OpenXRViewConfiguration.hpp"

#include "AREngine/Frame/FrameStatus.hpp"

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

    // --- Session state ---

    void TestFormatSessionStateKnownValues()
    {
        Check(FormatSessionState(XR_SESSION_STATE_IDLE) == "XR_SESSION_STATE_IDLE", "Formats IDLE correctly");
        Check(FormatSessionState(XR_SESSION_STATE_READY) == "XR_SESSION_STATE_READY", "Formats READY correctly");
        Check(FormatSessionState(XR_SESSION_STATE_SYNCHRONIZED) == "XR_SESSION_STATE_SYNCHRONIZED", "Formats SYNCHRONIZED correctly");
        Check(FormatSessionState(XR_SESSION_STATE_VISIBLE) == "XR_SESSION_STATE_VISIBLE", "Formats VISIBLE correctly");
        Check(FormatSessionState(XR_SESSION_STATE_FOCUSED) == "XR_SESSION_STATE_FOCUSED", "Formats FOCUSED correctly");
        Check(FormatSessionState(XR_SESSION_STATE_STOPPING) == "XR_SESSION_STATE_STOPPING", "Formats STOPPING correctly");
        Check(FormatSessionState(XR_SESSION_STATE_LOSS_PENDING) == "XR_SESSION_STATE_LOSS_PENDING", "Formats LOSS_PENDING correctly");
        Check(FormatSessionState(XR_SESSION_STATE_EXITING) == "XR_SESSION_STATE_EXITING", "Formats EXITING correctly");
    }

    void TestFormatSessionStateUnknownFallback()
    {
        const std::string formatted = FormatSessionState(static_cast<XrSessionState>(12345));
        Check(formatted == "XR_SESSION_STATE_UNKNOWN(12345)",
              "An unrecognized numeric state falls back to a readable-but-numeric representation, not a crash");
    }

    void TestShouldBeginSession()
    {
        Check(ShouldBeginSession(XR_SESSION_STATE_READY), "READY should begin the session");
        Check(!ShouldBeginSession(XR_SESSION_STATE_IDLE), "IDLE should not begin the session");
        Check(!ShouldBeginSession(XR_SESSION_STATE_SYNCHRONIZED), "SYNCHRONIZED should not begin the session");
        Check(!ShouldBeginSession(XR_SESSION_STATE_STOPPING), "STOPPING should not begin the session");
    }

    void TestShouldEndSession()
    {
        Check(ShouldEndSession(XR_SESSION_STATE_STOPPING, /*sessionRunning=*/true),
              "STOPPING while running should end the session");
        Check(!ShouldEndSession(XR_SESSION_STATE_STOPPING, /*sessionRunning=*/false),
              "STOPPING while NOT running should not call xrEndSession again");
        Check(!ShouldEndSession(XR_SESSION_STATE_FOCUSED, /*sessionRunning=*/true),
              "FOCUSED (even while running) should not end the session");
    }

    void TestShouldStopMainLoop()
    {
        Check(ShouldStopMainLoop(XR_SESSION_STATE_EXITING), "EXITING should stop the main loop");
        Check(ShouldStopMainLoop(XR_SESSION_STATE_LOSS_PENDING), "LOSS_PENDING should stop the main loop");
        Check(!ShouldStopMainLoop(XR_SESSION_STATE_FOCUSED), "FOCUSED should not stop the main loop");
        Check(!ShouldStopMainLoop(XR_SESSION_STATE_STOPPING), "STOPPING alone (before EXITING) should not yet stop the main loop");
    }

    // --- DetermineFrameStatus (M9E.5) ---

    void TestDetermineFrameStatusStop()
    {
        Check(DetermineFrameStatus(XR_SESSION_STATE_EXITING, /*sessionRunning=*/false) == AREngine::Frame::FrameStatus::Stop,
              "EXITING maps to FrameStatus::Stop regardless of sessionRunning");
        Check(DetermineFrameStatus(XR_SESSION_STATE_LOSS_PENDING, /*sessionRunning=*/true) == AREngine::Frame::FrameStatus::Stop,
              "LOSS_PENDING maps to FrameStatus::Stop regardless of sessionRunning");
    }

    void TestDetermineFrameStatusIdle()
    {
        Check(DetermineFrameStatus(XR_SESSION_STATE_READY, /*sessionRunning=*/false) == AREngine::Frame::FrameStatus::Idle,
              "A non-terminal state with sessionRunning=false maps to FrameStatus::Idle (e.g. not yet begun)");
        Check(DetermineFrameStatus(XR_SESSION_STATE_STOPPING, /*sessionRunning=*/false) == AREngine::Frame::FrameStatus::Idle,
              "STOPPING with sessionRunning=false (already ended) maps to FrameStatus::Idle, not Stop - "
              "EXITING has not arrived yet");
    }

    void TestDetermineFrameStatusContinue()
    {
        Check(DetermineFrameStatus(XR_SESSION_STATE_SYNCHRONIZED, /*sessionRunning=*/true) == AREngine::Frame::FrameStatus::Continue,
              "A non-terminal state with sessionRunning=true maps to FrameStatus::Continue");
        Check(DetermineFrameStatus(XR_SESSION_STATE_FOCUSED, /*sessionRunning=*/true) == AREngine::Frame::FrameStatus::Continue,
              "FOCUSED with sessionRunning=true maps to FrameStatus::Continue");
    }

    // --- DetermineSessionLifecycleActions (M9E.5) ---
    //
    // Directly tests the ordered-processing fix: a runtime can legitimately
    // deliver more than one XrEventDataSessionStateChanged within a single
    // poll cycle, and reacting only to the last one would silently skip a
    // required xrBeginSession/xrEndSession call.

    void TestDetermineSessionLifecycleActionsSingleReady()
    {
        const std::vector<XrSessionState> sequence{XR_SESSION_STATE_READY};
        const auto actions = DetermineSessionLifecycleActions(sequence, /*initiallyRunning=*/false);
        Check(actions.size() == 1 && actions[0] == SessionLifecycleAction::Begin,
              "A single READY (not running) produces exactly one Begin action");
    }

    void TestDetermineSessionLifecycleActionsSingleStopping()
    {
        const std::vector<XrSessionState> sequence{XR_SESSION_STATE_STOPPING};
        const auto actions = DetermineSessionLifecycleActions(sequence, /*initiallyRunning=*/true);
        Check(actions.size() == 1 && actions[0] == SessionLifecycleAction::End,
              "A single STOPPING (running) produces exactly one End action");
    }

    void TestDetermineSessionLifecycleActionsReadyThenStoppingSameCycle()
    {
        // The exact scenario this function exists to fix: READY and
        // STOPPING both arrive within one poll cycle, before the
        // session was ever running. Collapsing to "only the last state"
        // would see only STOPPING (with sessionRunning still false) and
        // produce no action at all - the session would never begin,
        // never end, and the demo would be stuck. Processing in order
        // must produce Begin, then End.
        const std::vector<XrSessionState> sequence{XR_SESSION_STATE_READY, XR_SESSION_STATE_STOPPING};
        const auto actions = DetermineSessionLifecycleActions(sequence, /*initiallyRunning=*/false);
        Check(actions.size() == 2, "Two observed transitions produce two actions, not one");
        Check(actions.size() == 2 && actions[0] == SessionLifecycleAction::Begin,
              "READY (first) produces Begin");
        Check(actions.size() == 2 && actions[1] == SessionLifecycleAction::End,
              "STOPPING (second, after Begin already ran) produces End - only possible because the running "
              "flag updated in between, not by looking at the original initiallyRunning=false alone");
    }

    void TestDetermineSessionLifecycleActionsNonActionableStates()
    {
        const std::vector<XrSessionState> sequence{XR_SESSION_STATE_IDLE, XR_SESSION_STATE_SYNCHRONIZED};
        const auto actions = DetermineSessionLifecycleActions(sequence, /*initiallyRunning=*/true);
        Check(actions.size() == 2 && actions[0] == SessionLifecycleAction::None && actions[1] == SessionLifecycleAction::None,
              "States that are neither READY nor a running-session's STOPPING produce None actions");
    }

    void TestDetermineSessionLifecycleActionsEmpty()
    {
        const auto actions = DetermineSessionLifecycleActions({}, /*initiallyRunning=*/false);
        Check(actions.empty(), "No observed transitions produces no actions");
    }

    // --- View configuration ---

    void TestIsViewConfigurationTypeSupported()
    {
        const std::vector<XrViewConfigurationType> supported{
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
        Check(IsViewConfigurationTypeSupported(supported, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO),
              "Finds stereo when present");
        Check(!IsViewConfigurationTypeSupported(supported, XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT),
              "Does not find a type that isn't present");
    }

    void TestSelectPrimaryViewConfigurationTypePrefersStereo()
    {
        const std::vector<XrViewConfigurationType> supported{
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO};
        const auto selected = SelectPrimaryViewConfigurationType(supported);
        Check(selected.has_value() && *selected == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
              "Selects PRIMARY_STEREO when it is supported");
    }

    void TestSelectPrimaryViewConfigurationTypeReturnsNulloptWithoutStereo()
    {
        // Deliberately does NOT fall back to MONO - see
        // OpenXRViewConfiguration.hpp's documented reasoning.
        const std::vector<XrViewConfigurationType> supported{XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
        const auto selected = SelectPrimaryViewConfigurationType(supported);
        Check(!selected.has_value(), "Returns std::nullopt (not a silent MONO fallback) when stereo is unsupported");
    }

    // --- Reference spaces ---

    void TestIsReferenceSpaceTypeSupported()
    {
        const std::vector<XrReferenceSpaceType> supported{XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL};
        Check(IsReferenceSpaceTypeSupported(supported, XR_REFERENCE_SPACE_TYPE_VIEW), "Finds VIEW when present");
        Check(!IsReferenceSpaceTypeSupported(supported, XR_REFERENCE_SPACE_TYPE_STAGE), "Does not find STAGE when absent");
    }

    void TestSelectReferenceSpacesToCreateAllPresent()
    {
        const std::vector<XrReferenceSpaceType> supported{
            XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL, XR_REFERENCE_SPACE_TYPE_STAGE};
        const auto selected = SelectReferenceSpacesToCreate(supported);
        Check(selected.size() == 3, "All three (VIEW, LOCAL, STAGE) are selected when all are supported");
        Check(IsReferenceSpaceTypeSupported(selected, XR_REFERENCE_SPACE_TYPE_VIEW), "VIEW is included");
        Check(IsReferenceSpaceTypeSupported(selected, XR_REFERENCE_SPACE_TYPE_LOCAL), "LOCAL is included");
        Check(IsReferenceSpaceTypeSupported(selected, XR_REFERENCE_SPACE_TYPE_STAGE), "STAGE is included");
    }

    void TestSelectReferenceSpacesToCreateWithoutStage()
    {
        // The exact scenario the brief warns about: "do not assume
        // STAGE exists" - many runtimes (including SteamVR's null
        // driver) do not support it.
        const std::vector<XrReferenceSpaceType> supported{XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_LOCAL};
        const auto selected = SelectReferenceSpacesToCreate(supported);
        Check(selected.size() == 2, "Only VIEW and LOCAL are selected when STAGE is unsupported - AREngine does not require STAGE");
        Check(!IsReferenceSpaceTypeSupported(selected, XR_REFERENCE_SPACE_TYPE_STAGE), "STAGE is correctly absent from the selection");
    }

    void TestIdentityPose()
    {
        const XrPosef pose = IdentityPose();
        Check(pose.orientation.x == 0.0f && pose.orientation.y == 0.0f && pose.orientation.z == 0.0f && pose.orientation.w == 1.0f,
              "IdentityPose's orientation is the identity quaternion (0,0,0,1)");
        Check(pose.position.x == 0.0f && pose.position.y == 0.0f && pose.position.z == 0.0f,
              "IdentityPose's position is the origin (0,0,0)");
    }

    // --- Environment blend mode (M9E) ---

    void TestIsEnvironmentBlendModeSupported()
    {
        const std::vector<XrEnvironmentBlendMode> supported{XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND};
        Check(IsEnvironmentBlendModeSupported(supported, XR_ENVIRONMENT_BLEND_MODE_OPAQUE), "Finds OPAQUE when present");
        Check(!IsEnvironmentBlendModeSupported(supported, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE), "Does not find ADDITIVE when absent");
    }

    void TestSelectEnvironmentBlendModePrefersOpaque()
    {
        const std::vector<XrEnvironmentBlendMode> supported{
            XR_ENVIRONMENT_BLEND_MODE_ADDITIVE, XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND, XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
        const auto selected = SelectEnvironmentBlendMode(supported);
        Check(selected.has_value() && *selected == XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
              "Selects OPAQUE when it is supported, regardless of enumeration order");
    }

    void TestSelectEnvironmentBlendModeFallsBackToAlphaBlend()
    {
        const std::vector<XrEnvironmentBlendMode> supported{XR_ENVIRONMENT_BLEND_MODE_ADDITIVE, XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND};
        const auto selected = SelectEnvironmentBlendMode(supported);
        Check(selected.has_value() && *selected == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND,
              "Falls back to ALPHA_BLEND when OPAQUE is unsupported");
    }

    void TestSelectEnvironmentBlendModeFallsBackToAdditive()
    {
        const std::vector<XrEnvironmentBlendMode> supported{XR_ENVIRONMENT_BLEND_MODE_ADDITIVE};
        const auto selected = SelectEnvironmentBlendMode(supported);
        Check(selected.has_value() && *selected == XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
              "Falls back to ADDITIVE when neither OPAQUE nor ALPHA_BLEND is supported");
    }

    void TestSelectEnvironmentBlendModeReturnsNulloptWhenEmpty()
    {
        const auto selected = SelectEnvironmentBlendMode({});
        Check(!selected.has_value(), "Returns std::nullopt (not a silent fallback) when the runtime reports no supported modes");
    }

    // --- XrTime conversion (M9E) ---

    void TestXrTimeToSecondsConvertsNanosecondsCorrectly()
    {
        Check(XrTimeToSeconds(1'000'000'000) == 1.0, "One billion nanoseconds is exactly one second");
        Check(XrTimeToSeconds(0) == 0.0, "Zero nanoseconds is zero seconds");
        Check(XrTimeToSeconds(500'000'000) == 0.5, "Half a billion nanoseconds is half a second");
    }
}

int main()
{
    TestFormatSessionStateKnownValues();
    TestFormatSessionStateUnknownFallback();
    TestShouldBeginSession();
    TestShouldEndSession();
    TestShouldStopMainLoop();

    TestDetermineFrameStatusStop();
    TestDetermineFrameStatusIdle();
    TestDetermineFrameStatusContinue();

    TestDetermineSessionLifecycleActionsSingleReady();
    TestDetermineSessionLifecycleActionsSingleStopping();
    TestDetermineSessionLifecycleActionsReadyThenStoppingSameCycle();
    TestDetermineSessionLifecycleActionsNonActionableStates();
    TestDetermineSessionLifecycleActionsEmpty();

    TestIsViewConfigurationTypeSupported();
    TestSelectPrimaryViewConfigurationTypePrefersStereo();
    TestSelectPrimaryViewConfigurationTypeReturnsNulloptWithoutStereo();

    TestIsReferenceSpaceTypeSupported();
    TestSelectReferenceSpacesToCreateAllPresent();
    TestSelectReferenceSpacesToCreateWithoutStage();
    TestIdentityPose();

    TestIsEnvironmentBlendModeSupported();
    TestSelectEnvironmentBlendModePrefersOpaque();
    TestSelectEnvironmentBlendModeFallsBackToAlphaBlend();
    TestSelectEnvironmentBlendModeFallsBackToAdditive();
    TestSelectEnvironmentBlendModeReturnsNulloptWhenEmpty();
    TestXrTimeToSecondsConvertsNanosecondsCorrectly();

    if (g_failureCount == 0)
    {
        std::printf("All OpenXR session (pure-logic) M9D/M9E/M9E.5 checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
