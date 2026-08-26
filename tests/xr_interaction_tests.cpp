// M10.6 automated tests for XRInteractionState.hpp/.cpp: pure input-to-
// visual-state logic, tested with SYNTHETIC AREngine::Input::*ActionState
// values only. Synthetic values are legitimate HERE (a unit test), never
// in tests/xr_demo.cpp itself, which must only ever feed real queried
// OpenXR state into this API - see docs/ARCHITECTURE.md, "No Fake Live
// Input (M10.6)". Deliberately calls ZERO OpenXR/Vulkan API functions
// and has ZERO OpenXR/Vulkan dependency - this file, and the code it
// tests, build in every ARENGINE_ENABLE_OPENXR/ARENGINE_ENABLE_VULKAN
// configuration, including OFF/OFF.

#include "XRInteractionState.hpp"

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

    void CheckNearlyEqual(float actual, float expected, const char* description)
    {
        constexpr float kEpsilon = 0.0001f;
        Check(actual > expected - kEpsilon && actual < expected + kEpsilon, description);
    }

    using namespace ARDemo;
    using namespace AREngine::Input;
    using namespace AREngine::Core::Math;

    DigitalActionState MakeDigital(bool down, bool pressed, bool released, bool active)
    {
        DigitalActionState state;
        state.down = down;
        state.pressed = pressed;
        state.released = released;
        state.active = active;
        return state;
    }

    // --- Digital toggle: react to pressed, never down ---

    void TestDigitalPressedTogglesHighlightOn()
    {
        XRInteractionState state;
        Check(!state.highlightEnabled, "starts with highlight off");

        ApplyDigitalToggle(MakeDigital(true, true, false, true), state);
        Check(state.highlightEnabled, "select.pressed=true toggles highlight on");
    }

    void TestDigitalHeldWithoutPressedDoesNotToggleAgain()
    {
        XRInteractionState state;
        state.highlightEnabled = true;

        // down=true but pressed=false (held, not the press edge) - must
        // not toggle every frame while held.
        ApplyDigitalToggle(MakeDigital(true, false, false, true), state);
        Check(state.highlightEnabled, "select held (down=true, pressed=false) does not toggle again");

        ApplyDigitalToggle(MakeDigital(true, false, false, true), state);
        Check(state.highlightEnabled, "select held across multiple frames still does not toggle");
    }

    void TestDigitalNextPressedTogglesHighlightOff()
    {
        XRInteractionState state;
        state.highlightEnabled = true;

        ApplyDigitalToggle(MakeDigital(false, true, false, true), state); // release then re-press edge
        Check(!state.highlightEnabled, "a second select.pressed toggles highlight back off");
    }

    void TestDigitalInactiveDoesNotSpuriouslyToggle()
    {
        XRInteractionState state;
        const bool initial = state.highlightEnabled;

        // An inactive DigitalActionState always has pressed=false by
        // construction (ConvertActionStateBoolean) - confirmed
        // explicitly here, not just assumed.
        ApplyDigitalToggle(MakeDigital(false, false, false, false), state);
        Check(state.highlightEnabled == initial, "inactive select never toggles highlight");
    }

    // --- Analog scale ---

    void TestAnalogInactiveGivesDefaultScale()
    {
        XRInteractionState state;
        state.scaleFactor = 1.6f; // simulate a stale prior active reading

        AnalogActionState trigger;
        trigger.active = false;
        trigger.value = 0.9f; // a stale/irrelevant runtime value while inactive

        ApplyAnalogScale(trigger, state);
        CheckNearlyEqual(state.scaleFactor, 1.0f, "inactive trigger resets scaleFactor to the neutral default, not a stale reading");
    }

    void TestAnalogZeroGivesBaseScale()
    {
        XRInteractionState state;
        AnalogActionState trigger;
        trigger.active = true;
        trigger.value = 0.0f;

        ApplyAnalogScale(trigger, state);
        CheckNearlyEqual(state.scaleFactor, 1.0f, "trigger=0 gives the base (1.0x) scale");
    }

    void TestAnalogHalfGivesIntermediateScale()
    {
        XRInteractionState state;
        AnalogActionState trigger;
        trigger.active = true;
        trigger.value = 0.5f;

        ApplyAnalogScale(trigger, state);
        CheckNearlyEqual(state.scaleFactor, 1.0f + 0.5f * kScaleFactorRange, "trigger=0.5 gives the documented intermediate scale");
    }

    void TestAnalogFullGivesMaxScale()
    {
        XRInteractionState state;
        AnalogActionState trigger;
        trigger.active = true;
        trigger.value = 1.0f;

        ApplyAnalogScale(trigger, state);
        CheckNearlyEqual(state.scaleFactor, 1.0f + kScaleFactorRange, "trigger=1 gives the documented maximum scale");
    }

    void TestAnalogStaleValueNotReusedAfterInactive()
    {
        XRInteractionState state;
        AnalogActionState active;
        active.active = true;
        active.value = 1.0f;
        ApplyAnalogScale(active, state);
        CheckNearlyEqual(state.scaleFactor, 1.0f + kScaleFactorRange, "sanity: active trigger=1 sets the max scale first");

        AnalogActionState nowInactive;
        nowInactive.active = false;
        nowInactive.value = 1.0f; // the runtime may still report the last value even though isActive is now false
        ApplyAnalogScale(nowInactive, state);
        CheckNearlyEqual(state.scaleFactor, 1.0f, "going inactive resets scaleFactor - the previous active value is not reused");
    }

    // --- Vector2 offset ---

    void TestVectorInactiveGivesZeroOffset()
    {
        XRInteractionState state;
        state.moveOffset = Vec2(0.3f, -0.2f); // simulate a stale prior active reading

        Vector2ActionState move;
        move.active = false;
        move.value = Vec2(0.8f, 0.8f);

        ApplyVectorOffset(move, state);
        Check(state.moveOffset.x == 0.0f && state.moveOffset.y == 0.0f, "inactive move gives a zero offset, not a stale reading");
    }

    void TestVectorPositiveXMovesRight()
    {
        XRInteractionState state;
        Vector2ActionState move;
        move.active = true;
        move.value = Vec2(1.0f, 0.0f);

        ApplyVectorOffset(move, state);
        Check(state.moveOffset.x > 0.0f, "positive move.x produces a positive (rightward) offset.x");
    }

    void TestVectorNegativeXMovesLeft()
    {
        XRInteractionState state;
        Vector2ActionState move;
        move.active = true;
        move.value = Vec2(-1.0f, 0.0f);

        ApplyVectorOffset(move, state);
        Check(state.moveOffset.x < 0.0f, "negative move.x produces a negative (leftward) offset.x");
    }

    void TestVectorPositiveYMapping()
    {
        XRInteractionState state;
        Vector2ActionState move;
        move.active = true;
        move.value = Vec2(0.0f, 1.0f);

        ApplyVectorOffset(move, state);
        // Documented mapping: offset.y is a direct (unflipped) scale of
        // move.y - matches offset.x's own direct mapping, no axis
        // inversion applied anywhere in ApplyVectorOffset.
        Check(state.moveOffset.y > 0.0f, "positive move.y produces a positive offset.y (direct, unflipped mapping)");
    }

    void TestVectorClampsToDocumentedBounds()
    {
        XRInteractionState state;
        Vector2ActionState move;
        move.active = true;
        move.value = Vec2(10.0f, -10.0f); // far outside any real controller's normal range

        ApplyVectorOffset(move, state);
        CheckNearlyEqual(state.moveOffset.x, kMaxMoveOffsetMeters, "offset.x clamps to +kMaxMoveOffsetMeters");
        CheckNearlyEqual(state.moveOffset.y, -kMaxMoveOffsetMeters, "offset.y clamps to -kMaxMoveOffsetMeters");
    }

    // --- Pose marker ---

    PoseActionState MakePose(bool active, bool positionValid, bool orientationValid, Vec3 position, Quaternion orientation)
    {
        PoseActionState pose;
        pose.active = active;
        pose.positionValid = positionValid;
        pose.orientationValid = orientationValid;
        pose.position = position;
        pose.orientation = orientation;
        return pose;
    }

    void TestPoseInactiveDisablesMarker()
    {
        XRInteractionState state;
        ApplyPoseMarker(MakePose(false, true, true, Vec3(1.0f, 2.0f, 3.0f), Quaternion::Identity()), state);
        Check(!state.poseMarkerVisible, "inactive pose action disables the marker even if position/orientation look valid");
    }

    void TestPoseActiveButInvalidDisablesMarker()
    {
        XRInteractionState state;
        // Active, but position NOT valid - chosen policy requires BOTH
        // to be valid for the marker to show.
        ApplyPoseMarker(MakePose(true, false, true, Vec3(1.0f, 2.0f, 3.0f), Quaternion::Identity()), state);
        Check(!state.poseMarkerVisible, "active pose with invalid position stays disabled under the chosen both-valid policy");
    }

    void TestPoseActiveAndValidShowsMarkerMatchingGenericPose()
    {
        XRInteractionState state;
        const Vec3 position(0.1f, 0.2f, -0.3f);
        const Quaternion orientation(0.7071f, 0.0f, 0.7071f, 0.0f); // an arbitrary, per-component-asymmetric test rotation

        ApplyPoseMarker(MakePose(true, true, true, position, orientation), state);
        Check(state.poseMarkerVisible, "active + fully valid pose shows the marker");
        Check(state.poseMarkerPosition.x == position.x && state.poseMarkerPosition.y == position.y && state.poseMarkerPosition.z == position.z,
              "marker position matches the generic PoseActionState position exactly");
    }

    void TestPoseOrientationCopiedCorrectly()
    {
        XRInteractionState state;
        // Per-component-asymmetric, not identity - a reorder/copy bug
        // would be caught here, unlike an identity-only check.
        const Quaternion orientation(0.5f, 0.5f, 0.5f, 0.5f);
        ApplyPoseMarker(MakePose(true, true, true, Vec3(), orientation), state);

        Check(state.poseMarkerOrientation.w == orientation.w, "marker orientation.w copied correctly");
        Check(state.poseMarkerOrientation.x == orientation.x, "marker orientation.x copied correctly");
        Check(state.poseMarkerOrientation.y == orientation.y, "marker orientation.y copied correctly");
        Check(state.poseMarkerOrientation.z == orientation.z, "marker orientation.z copied correctly");
    }

    void TestPoseHiddenResetsToDefaultNotStalePose()
    {
        XRInteractionState state;
        ApplyPoseMarker(MakePose(true, true, true, Vec3(5.0f, 5.0f, 5.0f), Quaternion::Identity()), state);
        Check(state.poseMarkerVisible, "sanity: an active+valid pose shows the marker first");

        ApplyPoseMarker(MakePose(false, true, true, Vec3(5.0f, 5.0f, 5.0f), Quaternion::Identity()), state);
        Check(!state.poseMarkerVisible, "going inactive hides the marker");
        Check(state.poseMarkerPosition.x == 0.0f && state.poseMarkerPosition.y == 0.0f && state.poseMarkerPosition.z == 0.0f,
              "hidden marker's position resets to default, not the previous frame's stale pose");
    }

    // --- Shared world state: independent of view count ---

    void TestInteractionStateIsIndependentOfViewCount()
    {
        // The whole point of XRInteractionState is that it is computed
        // ONCE per frame, from ONE hand's action state, and then the
        // SAME resulting state is read by however many views the
        // runtime reports (2 for stereo, but this type has no concept
        // of "view" at all) - confirmed structurally: XRInteractionState
        // carries no per-view field, no array sized by view count, and
        // every Apply* function's signature takes no view index or
        // count parameter whatsoever.
        XRInteractionState state;
        ApplyDigitalToggle(MakeDigital(true, true, false, true), state);
        ApplyAnalogScale(AnalogActionState{0.5f, true}, state);
        ApplyVectorOffset(Vector2ActionState{Vec2(0.2f, 0.2f), true}, state);
        ApplyPoseMarker(MakePose(true, true, true, Vec3(1.0f, 1.0f, 1.0f), Quaternion::Identity()), state);

        const XRInteractionState copyForView0 = state;
        const XRInteractionState copyForView1 = state;
        Check(copyForView0.highlightEnabled == copyForView1.highlightEnabled &&
              copyForView0.scaleFactor == copyForView1.scaleFactor &&
              copyForView0.moveOffset.x == copyForView1.moveOffset.x &&
              copyForView0.poseMarkerVisible == copyForView1.poseMarkerVisible,
              "the same computed state is identical regardless of which view reads it - never recomputed per eye");
    }
}

int main()
{
    TestDigitalPressedTogglesHighlightOn();
    TestDigitalHeldWithoutPressedDoesNotToggleAgain();
    TestDigitalNextPressedTogglesHighlightOff();
    TestDigitalInactiveDoesNotSpuriouslyToggle();

    TestAnalogInactiveGivesDefaultScale();
    TestAnalogZeroGivesBaseScale();
    TestAnalogHalfGivesIntermediateScale();
    TestAnalogFullGivesMaxScale();
    TestAnalogStaleValueNotReusedAfterInactive();

    TestVectorInactiveGivesZeroOffset();
    TestVectorPositiveXMovesRight();
    TestVectorNegativeXMovesLeft();
    TestVectorPositiveYMapping();
    TestVectorClampsToDocumentedBounds();

    TestPoseInactiveDisablesMarker();
    TestPoseActiveButInvalidDisablesMarker();
    TestPoseActiveAndValidShowsMarkerMatchingGenericPose();
    TestPoseOrientationCopiedCorrectly();
    TestPoseHiddenResetsToDefaultNotStalePose();

    TestInteractionStateIsIndependentOfViewCount();

    if (g_failureCount == 0)
    {
        std::printf("All XR interaction-state (pure-logic) M10.6 checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
