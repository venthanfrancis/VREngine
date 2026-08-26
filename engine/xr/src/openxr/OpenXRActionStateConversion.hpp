#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: converting OpenXR action state
// into AREngine's generic Input::*ActionState types is a pure OpenXR +
// Input concept, independent of which graphics API backs the session.
// See docs/ARCHITECTURE.md, "Digital/Analog/Vector2/Pose State
// Semantics (M10)".

#include "OpenXRViewConversion.hpp" // ConvertXrPosition/ConvertXrOrientation

#include "AREngine/Input/ActionState.hpp"

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // Digital transition logic mirrors AREngine::Input::InputSystem's
    // own ButtonState model exactly (M7): down/pressed/released are
    // computed from an up->down / down->up EDGE against `previousDown`
    // (the caller's own tracked state from the last call), NOT from
    // OpenXR's own changedSinceLastSync - which means "this value
    // differs from what it was at the previous xrSyncActions", a
    // related but not identical concept (e.g. it says nothing about
    // whether the CALLER has already observed and reacted to that
    // change). When `state.isActive` is false, the action is treated
    // as not-held regardless of `state.currentState`, and if it WAS
    // held on the previous call, a release transition is still
    // reported - mirroring M7's WindowFocusLostEvent handling, which
    // similarly never leaves a key/button stuck Down once the signal
    // that would have released it stops arriving. Pure logic, directly
    // unit-testable.
    [[nodiscard]] constexpr Input::DigitalActionState ConvertActionStateBoolean(
        const XrActionStateBoolean& state, bool previousDown)
    {
        const bool active = state.isActive != XR_FALSE;
        const bool down = active && state.currentState != XR_FALSE;

        Input::DigitalActionState result;
        result.active = active;
        result.down = down;
        result.pressed = down && !previousDown;
        result.released = !down && previousDown;
        return result;
    }

    // `value` is zero when inactive - never a stale previous reading.
    // No blind clamping: the OpenXR spec does not guarantee every
    // float input component is normalized to 0..1 (though common
    // trigger components are), so whatever the runtime reports is
    // passed through unchanged while active. Pure logic, directly
    // unit-testable.
    [[nodiscard]] constexpr Input::AnalogActionState ConvertActionStateFloat(const XrActionStateFloat& state)
    {
        Input::AnalogActionState result;
        result.active = state.isActive != XR_FALSE;
        result.value = result.active ? state.currentState : 0.0f;
        return result;
    }

    // `value` is (0,0) when inactive - never a stale previous reading.
    // Pure logic, directly unit-testable.
    [[nodiscard]] constexpr Input::Vector2ActionState ConvertActionStateVector2f(const XrActionStateVector2f& state)
    {
        Input::Vector2ActionState result;
        result.active = state.isActive != XR_FALSE;
        result.value = result.active ? Core::Math::Vec2(state.currentState.x, state.currentState.y) : Core::Math::Vec2();
        return result;
    }

    // `actionIsActive` comes from a prior xrGetActionStatePose call
    // (the pose ACTION's own activity) and `location` from a
    // subsequent xrLocateSpace call against that action's space -
    // callers must check `actionIsActive` before ever calling
    // xrLocateSpace at all (an inactive pose action's space should
    // never be located/trusted - see docs/ARCHITECTURE.md, "Pose State
    // (M10)"). Position/orientation are only meaningful when their own
    // *_VALID bit is set - never fabricated otherwise; an active
    // action can still report invalid pose data during a tracking
    // interruption (position/orientation valid does not imply actively
    // tracked - the OpenXR spec's own *_TRACKED bits are a separate,
    // stricter condition this function does not require). Pure logic,
    // directly unit-testable.
    [[nodiscard]] inline Input::PoseActionState ConvertActionStatePose(bool actionIsActive, const XrSpaceLocation& location)
    {
        Input::PoseActionState result;
        result.active = actionIsActive;
        result.positionValid = (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0;
        result.orientationValid = (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0;
        if (result.positionValid)
        {
            result.position = ConvertXrPosition(location.pose.position);
        }
        if (result.orientationValid)
        {
            result.orientation = ConvertXrOrientation(location.pose.orientation);
        }
        return result;
    }
}
