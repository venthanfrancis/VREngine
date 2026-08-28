// M10 automated tests for AREngine::XR::OpenXR's pure-logic action-state
// conversion helpers (OpenXRActionStateConversion.hpp): boolean/float/
// vector2f/pose XrActionState* -> AREngine::Input::*ActionState
// conversion. Deliberately calls ZERO real OpenXR API functions (no
// xrCreateActionSet, no xrSyncActions, no xrGetActionState*, ...) and
// has ZERO Vulkan dependency, so this runs on any machine with the
// OpenXR headers available at compile time, without needing a real
// OpenXR runtime, headset, GPU, or even Vulkan enabled.
//
// Real action creation/binding/sync/state-query against a real loader/
// runtime is exercised only by the separate, manual
// arengine_openxr_input_demo — not part of this suite, since CTest must
// not depend on an XR runtime or headset being present.

#include "openxr/OpenXRActionStateConversion.hpp"
#include "openxr/OpenXRTouchControllerBindings.hpp"

#include <cstdio>
#include <cstring>

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
    using namespace AREngine::Input;

    XrActionStateBoolean MakeBooleanState(bool current, bool active)
    {
        XrActionStateBoolean state{};
        state.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        state.currentState = current ? XR_TRUE : XR_FALSE;
        state.isActive = active ? XR_TRUE : XR_FALSE;
        return state;
    }

    // --- ConvertActionStateBoolean: false -> true -> false transitions ---

    void TestBooleanFalseToTrueIsPressed()
    {
        const DigitalActionState result = ConvertActionStateBoolean(MakeBooleanState(true, true), /*previousDown=*/false);
        Check(result.down, "false->true: down is true");
        Check(result.pressed, "false->true: pressed is true (this is the up->down edge)");
        Check(!result.released, "false->true: released is false");
        Check(result.active, "false->true: active is true");
    }

    void TestBooleanTrueToTrueIsHeldNotPressed()
    {
        const DigitalActionState result = ConvertActionStateBoolean(MakeBooleanState(true, true), /*previousDown=*/true);
        Check(result.down, "true->true: down is true");
        Check(!result.pressed, "true->true: pressed is false - not re-triggered while held");
        Check(!result.released, "true->true: released is false");
    }

    void TestBooleanTrueToFalseIsReleased()
    {
        const DigitalActionState result = ConvertActionStateBoolean(MakeBooleanState(false, true), /*previousDown=*/true);
        Check(!result.down, "true->false: down is false");
        Check(!result.pressed, "true->false: pressed is false");
        Check(result.released, "true->false: released is true (this is the down->up edge)");
    }

    void TestBooleanFalseToFalseIsNoTransition()
    {
        const DigitalActionState result = ConvertActionStateBoolean(MakeBooleanState(false, true), /*previousDown=*/false);
        Check(!result.down, "false->false: down stays false");
        Check(!result.pressed, "false->false: pressed is false");
        Check(!result.released, "false->false: released is false");
    }

    // --- Inactive digital state clears held state ---

    void TestBooleanInactiveClearsHeldStateAndReportsRelease()
    {
        // The runtime still reports currentState=true (e.g. a stale
        // value from before the action went inactive), but isActive is
        // false - the converted state must not report "down", and since
        // it WAS held on the previous call, a release transition must
        // still be reported (mirrors M7's WindowFocusLostEvent handling
        // - nothing is left stuck Down).
        const DigitalActionState result = ConvertActionStateBoolean(MakeBooleanState(true, false), /*previousDown=*/true);
        Check(!result.active, "inactive: active is false");
        Check(!result.down, "inactive: down is false even though currentState was true");
        Check(result.released, "inactive (was held): released is true - no stale held state left behind");
        Check(!result.pressed, "inactive (was held): pressed is false");
    }

    void TestBooleanInactiveAndNotPreviouslyHeldStaysQuiet()
    {
        const DigitalActionState result = ConvertActionStateBoolean(MakeBooleanState(true, false), /*previousDown=*/false);
        Check(!result.down, "inactive, not previously held: down is false");
        Check(!result.pressed, "inactive, not previously held: pressed is false");
        Check(!result.released, "inactive, not previously held: released is false - nothing to release");
    }

    // --- ConvertActionStateFloat: analog inactive behavior ---

    void TestFloatActiveValuePassesThrough()
    {
        XrActionStateFloat state{};
        state.type = XR_TYPE_ACTION_STATE_FLOAT;
        state.currentState = 0.73f;
        state.isActive = XR_TRUE;

        const AnalogActionState result = ConvertActionStateFloat(state);
        Check(result.active, "float active: active is true");
        Check(result.value == 0.73f, "float active: value passes through unchanged, not blindly clamped");
    }

    void TestFloatInactiveReportsZeroNotStaleValue()
    {
        XrActionStateFloat state{};
        state.type = XR_TYPE_ACTION_STATE_FLOAT;
        state.currentState = 0.9f; // a stale/irrelevant runtime value while inactive
        state.isActive = XR_FALSE;

        const AnalogActionState result = ConvertActionStateFloat(state);
        Check(!result.active, "float inactive: active is false");
        Check(result.value == 0.0f, "float inactive: value is zero, never the stale currentState");
    }

    // --- ConvertActionStateVector2f: vector2 inactive behavior ---

    void TestVector2ActiveValuePassesThrough()
    {
        XrActionStateVector2f state{};
        state.type = XR_TYPE_ACTION_STATE_VECTOR2F;
        state.currentState = XrVector2f{0.25f, -0.5f};
        state.isActive = XR_TRUE;

        const Vector2ActionState result = ConvertActionStateVector2f(state);
        Check(result.active, "vector2 active: active is true");
        Check(result.value.x == 0.25f && result.value.y == -0.5f, "vector2 active: value passes through unchanged");
    }

    void TestVector2InactiveReportsZeroNotStaleValue()
    {
        XrActionStateVector2f state{};
        state.type = XR_TYPE_ACTION_STATE_VECTOR2F;
        state.currentState = XrVector2f{1.0f, 1.0f}; // stale/irrelevant while inactive
        state.isActive = XR_FALSE;

        const Vector2ActionState result = ConvertActionStateVector2f(state);
        Check(!result.active, "vector2 inactive: active is false");
        Check(result.value.x == 0.0f && result.value.y == 0.0f, "vector2 inactive: value is (0,0), never the stale currentState");
    }

    // --- ConvertActionStatePose: XrPosef -> generic pose + validity flags ---

    XrSpaceLocation MakeLocation(XrSpaceLocationFlags flags, float px, float py, float pz, float qw, float qx, float qy, float qz)
    {
        XrSpaceLocation location{};
        location.type = XR_TYPE_SPACE_LOCATION;
        location.locationFlags = flags;
        location.pose.position = XrVector3f{px, py, pz};
        location.pose.orientation = XrQuaternionf{qx, qy, qz, qw};
        return location;
    }

    void TestPoseBothValidConvertsPositionAndOrientation()
    {
        constexpr XrSpaceLocationFlags kBothValid =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        const XrSpaceLocation location = MakeLocation(kBothValid, 1.0f, 2.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f);

        const PoseActionState result = ConvertActionStatePose(/*actionIsActive=*/true, location);
        Check(result.active, "pose: active is true");
        Check(result.positionValid, "pose: positionValid is true when the VALID bit is set");
        Check(result.orientationValid, "pose: orientationValid is true when the VALID bit is set");
        Check(result.position.x == 1.0f && result.position.y == 2.0f && result.position.z == 3.0f,
              "pose: position reuses the same XrVector3f -> Vec3 conversion as view location (correctness by reuse)");
        Check(result.orientation.w == 1.0f && result.orientation.x == 0.0f, "pose: orientation reuses the same XrQuaternionf -> Quaternion conversion");
    }

    void TestPoseInvalidPositionIsNotFabricated()
    {
        // Orientation valid, position NOT valid - position must stay at
        // its default (0,0,0), never populated from the (irrelevant)
        // XrSpaceLocation.pose.position value.
        const XrSpaceLocation location = MakeLocation(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT, 9.0f, 9.0f, 9.0f, 1.0f, 0.0f, 0.0f, 0.0f);

        const PoseActionState result = ConvertActionStatePose(/*actionIsActive=*/true, location);
        Check(!result.positionValid, "pose: positionValid is false when the bit is unset");
        Check(result.orientationValid, "pose: orientationValid is true when the bit is set");
        Check(result.position.x == 0.0f && result.position.y == 0.0f && result.position.z == 0.0f,
              "pose: position stays default (not fabricated from invalid data)");
    }

    void TestPoseInactiveActionIsReportedInactive()
    {
        constexpr XrSpaceLocationFlags kBothValid =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        const XrSpaceLocation location = MakeLocation(kBothValid, 5.0f, 5.0f, 5.0f, 1.0f, 0.0f, 0.0f, 0.0f);

        // Even with a "valid-looking" location, an inactive ACTION must
        // be reported as inactive - callers of the real API never reach
        // this branch in practice (OpenXRActionSystem checks
        // isActive before ever calling xrLocateSpace), but the
        // conversion function itself must still honor the flag it was
        // given, not silently upgrade it based on location data alone.
        const PoseActionState result = ConvertActionStatePose(/*actionIsActive=*/false, location);
        Check(!result.active, "pose: an inactive action stays inactive regardless of location validity");
    }

    // --- M11.1B: oculus/touch_controller binding paths (pure data, no
    // OpenXR API calls - see OpenXRTouchControllerBindings.hpp for why
    // SuggestTouchControllerBindings() itself, which makes real
    // xrStringToPath/xrSuggestInteractionProfileBindings calls, is not
    // unit-tested here, matching this codebase's existing precedent). ---

    bool PathEquals(const char* actual, const char* expected)
    {
        return std::strcmp(actual, expected) == 0;
    }

    void TestTouchControllerProfilePath()
    {
        const TouchControllerBindingPaths paths = GetTouchControllerBindingPaths();
        Check(PathEquals(paths.profile, "/interaction_profiles/oculus/touch_controller"),
              "touch_controller: profile path is exactly /interaction_profiles/oculus/touch_controller");
    }

    void TestTouchControllerSelectComponentPaths()
    {
        const TouchControllerBindingPaths paths = GetTouchControllerBindingPaths();
        // No literal "select/click" exists on this profile - the
        // primary face button is used instead (x/click left, a/click
        // right), distinct from the trigger component.
        Check(PathEquals(paths.leftSelect, "/user/hand/left/input/x/click"),
              "touch_controller: left select maps to x/click");
        Check(PathEquals(paths.rightSelect, "/user/hand/right/input/a/click"),
              "touch_controller: right select maps to a/click");
    }

    void TestTouchControllerTriggerComponentPaths()
    {
        const TouchControllerBindingPaths paths = GetTouchControllerBindingPaths();
        // A genuine float-capable component - never a boolean click
        // bound to the float trigger action.
        Check(PathEquals(paths.leftTrigger, "/user/hand/left/input/trigger/value"),
              "touch_controller: left trigger maps to trigger/value");
        Check(PathEquals(paths.rightTrigger, "/user/hand/right/input/trigger/value"),
              "touch_controller: right trigger maps to trigger/value");
    }

    void TestTouchControllerVector2ComponentPaths()
    {
        const TouchControllerBindingPaths paths = GetTouchControllerBindingPaths();
        // The AGGREGATE 2D thumbstick path - never a scalar .../x or
        // .../y alone, which would not be a valid Vector2f binding.
        Check(PathEquals(paths.leftMove, "/user/hand/left/input/thumbstick"),
              "touch_controller: left move maps to the aggregate thumbstick path, not a scalar axis");
        Check(PathEquals(paths.rightMove, "/user/hand/right/input/thumbstick"),
              "touch_controller: right move maps to the aggregate thumbstick path, not a scalar axis");
        Check(!PathEquals(paths.leftMove, "/user/hand/left/input/thumbstick/x"),
              "touch_controller: left move is NOT bound to the scalar /x sub-path");
    }

    void TestTouchControllerAimPoseComponentPaths()
    {
        const TouchControllerBindingPaths paths = GetTouchControllerBindingPaths();
        // aim/pose, matching the simple_controller binding's own
        // choice - never grip/pose.
        Check(PathEquals(paths.leftAimPose, "/user/hand/left/input/aim/pose"),
              "touch_controller: left aim_pose maps to aim/pose");
        Check(PathEquals(paths.rightAimPose, "/user/hand/right/input/aim/pose"),
              "touch_controller: right aim_pose maps to aim/pose");
        Check(!PathEquals(paths.leftAimPose, "/user/hand/left/input/grip/pose"),
              "touch_controller: left aim_pose is NOT bound to grip/pose");
    }

    void TestTouchControllerLeftRightPathsAreDistinct()
    {
        const TouchControllerBindingPaths paths = GetTouchControllerBindingPaths();
        // Left/right subaction bookkeeping: every left-hand path must
        // differ from its right-hand counterpart (a hand-swap bug would
        // silently pass every other check above).
        Check(!PathEquals(paths.leftSelect, paths.rightSelect), "touch_controller: left/right select paths differ");
        Check(!PathEquals(paths.leftTrigger, paths.rightTrigger), "touch_controller: left/right trigger paths differ");
        Check(!PathEquals(paths.leftMove, paths.rightMove), "touch_controller: left/right move paths differ");
        Check(!PathEquals(paths.leftAimPose, paths.rightAimPose), "touch_controller: left/right aim_pose paths differ");
    }

    void TestTouchControllerAndSimpleControllerProfilesCoexist()
    {
        // Multiple profile suggestions coexisting: touch_controller's
        // own profile path must never collide with khr/simple_controller's
        // (OpenXRSimpleControllerBindings.cpp) - both are suggested
        // unconditionally, together, in OpenXRActionSystem's constructor,
        // and the runtime resolves whichever actually matches the
        // connected/simulated device. A literal string compare here
        // (not a shared constant) deliberately mirrors what a future
        // reader of either file would see independently.
        const TouchControllerBindingPaths touchPaths = GetTouchControllerBindingPaths();
        constexpr const char* kSimpleControllerProfile = "/interaction_profiles/khr/simple_controller";
        Check(!PathEquals(touchPaths.profile, kSimpleControllerProfile),
              "touch_controller and simple_controller profile paths are distinct - both can be suggested together");
    }
}

int main()
{
    TestBooleanFalseToTrueIsPressed();
    TestBooleanTrueToTrueIsHeldNotPressed();
    TestBooleanTrueToFalseIsReleased();
    TestBooleanFalseToFalseIsNoTransition();
    TestBooleanInactiveClearsHeldStateAndReportsRelease();
    TestBooleanInactiveAndNotPreviouslyHeldStaysQuiet();

    TestFloatActiveValuePassesThrough();
    TestFloatInactiveReportsZeroNotStaleValue();

    TestVector2ActiveValuePassesThrough();
    TestVector2InactiveReportsZeroNotStaleValue();

    TestPoseBothValidConvertsPositionAndOrientation();
    TestPoseInvalidPositionIsNotFabricated();
    TestPoseInactiveActionIsReportedInactive();

    TestTouchControllerProfilePath();
    TestTouchControllerSelectComponentPaths();
    TestTouchControllerTriggerComponentPaths();
    TestTouchControllerVector2ComponentPaths();
    TestTouchControllerAimPoseComponentPaths();
    TestTouchControllerLeftRightPathsAreDistinct();
    TestTouchControllerAndSimpleControllerProfilesCoexist();

    if (g_failureCount == 0)
    {
        std::printf("All OpenXR action-state-conversion (pure-logic) M10/M11.1B checks passed\n");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
