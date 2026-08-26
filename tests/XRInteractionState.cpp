#include "XRInteractionState.hpp"

#include <algorithm>

namespace ARDemo
{
    void ApplyDigitalToggle(const AREngine::Input::DigitalActionState& select, XRInteractionState& state)
    {
        if (select.pressed)
        {
            state.highlightEnabled = !state.highlightEnabled;
        }
    }

    void ApplyAnalogScale(const AREngine::Input::AnalogActionState& trigger, XRInteractionState& state)
    {
        state.scaleFactor = trigger.active ? (1.0f + trigger.value * kScaleFactorRange) : 1.0f;
    }

    void ApplyVectorOffset(const AREngine::Input::Vector2ActionState& move, XRInteractionState& state)
    {
        if (!move.active)
        {
            state.moveOffset = AREngine::Core::Math::Vec2(0.0f, 0.0f);
            return;
        }

        const float x = std::clamp(move.value.x * kMoveOffsetMetersPerUnit, -kMaxMoveOffsetMeters, kMaxMoveOffsetMeters);
        const float y = std::clamp(move.value.y * kMoveOffsetMetersPerUnit, -kMaxMoveOffsetMeters, kMaxMoveOffsetMeters);
        state.moveOffset = AREngine::Core::Math::Vec2(x, y);
    }

    void ApplyPoseMarker(const AREngine::Input::PoseActionState& pose, XRInteractionState& state)
    {
        state.poseMarkerVisible = pose.active && pose.positionValid && pose.orientationValid;
        if (state.poseMarkerVisible)
        {
            state.poseMarkerPosition = pose.position;
            state.poseMarkerOrientation = pose.orientation;
        }
        else
        {
            state.poseMarkerPosition = AREngine::Core::Math::Vec3();
            state.poseMarkerOrientation = AREngine::Core::Math::Quaternion::Identity();
        }
    }
}
