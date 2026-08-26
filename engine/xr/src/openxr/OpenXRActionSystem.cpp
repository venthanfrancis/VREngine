#include "OpenXRActionSystem.hpp"

#include "OpenXRActionStateConversion.hpp"
#include "OpenXRResult.hpp"
#include "OpenXRSimpleControllerBindings.hpp"

namespace AREngine::XR::OpenXR
{
    OpenXRActionSystem::OpenXRActionSystem(XrInstance instance, XrSession session)
        : m_handPaths(ResolveHandSubactionPaths(instance))
        , m_actionSet(instance, "gameplay", "Gameplay")
        , m_aimPoseAction(instance, m_actionSet.Get(), "aim_pose", "Aim Pose",
                           XR_ACTION_TYPE_POSE_INPUT, {m_handPaths.left, m_handPaths.right})
        , m_selectAction(instance, m_actionSet.Get(), "select", "Select",
                          XR_ACTION_TYPE_BOOLEAN_INPUT, {m_handPaths.left, m_handPaths.right})
        , m_triggerAction(instance, m_actionSet.Get(), "trigger", "Trigger",
                           XR_ACTION_TYPE_FLOAT_INPUT, {m_handPaths.left, m_handPaths.right})
        , m_moveAction(instance, m_actionSet.Get(), "move", "Move",
                        XR_ACTION_TYPE_VECTOR2F_INPUT, {m_handPaths.left, m_handPaths.right})
    {
        // Bindings must be suggested before the action set is attached
        // (OpenXR spec) - attach must happen before any action space is
        // created (established best practice) or any state is queried.
        SuggestSimpleControllerBindings(instance, m_selectAction.Get(), m_aimPoseAction.Get());
        m_actionSet.Attach(instance, session);

        m_leftAimSpace = std::make_unique<OpenXRActionSpace>(instance, session, m_aimPoseAction.Get(), m_handPaths.left);
        m_rightAimSpace = std::make_unique<OpenXRActionSpace>(instance, session, m_aimPoseAction.Get(), m_handPaths.right);
    }

    void OpenXRActionSystem::SyncActions(XrInstance instance, XrSession session)
    {
        XrActiveActionSet activeActionSet{};
        activeActionSet.actionSet = m_actionSet.Get();
        activeActionSet.subactionPath = XR_NULL_PATH; // both hands active

        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CheckXrResult(instance, xrSyncActions(session, &syncInfo), "xrSyncActions");
    }

    Input::DigitalActionState OpenXRActionSystem::GetSelectState(XrInstance instance, XrSession session, Hand hand)
    {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = m_selectAction.Get();
        getInfo.subactionPath = SubactionPath(hand);

        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        CheckXrResult(instance, xrGetActionStateBoolean(session, &getInfo, &state), "xrGetActionStateBoolean (select)");

        bool& previousDown = m_previousSelectDown[hand == Hand::Left ? 0 : 1];
        const Input::DigitalActionState result = ConvertActionStateBoolean(state, previousDown);
        previousDown = result.down;
        return result;
    }

    Input::AnalogActionState OpenXRActionSystem::GetTriggerState(XrInstance instance, XrSession session, Hand hand)
    {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = m_triggerAction.Get();
        getInfo.subactionPath = SubactionPath(hand);

        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        CheckXrResult(instance, xrGetActionStateFloat(session, &getInfo, &state), "xrGetActionStateFloat (trigger)");
        return ConvertActionStateFloat(state);
    }

    Input::Vector2ActionState OpenXRActionSystem::GetMoveState(XrInstance instance, XrSession session, Hand hand)
    {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = m_moveAction.Get();
        getInfo.subactionPath = SubactionPath(hand);

        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        CheckXrResult(instance, xrGetActionStateVector2f(session, &getInfo, &state), "xrGetActionStateVector2f (move)");
        return ConvertActionStateVector2f(state);
    }

    Input::PoseActionState OpenXRActionSystem::GetAimPoseState(
        XrInstance instance, XrSession session, XrSpace baseSpace, XrTime time, Hand hand)
    {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = m_aimPoseAction.Get();
        getInfo.subactionPath = SubactionPath(hand);

        XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
        CheckXrResult(instance, xrGetActionStatePose(session, &getInfo, &poseState), "xrGetActionStatePose (aim_pose)");

        // Only locate the action's space if the action itself is
        // active - an inactive pose action's space must never be
        // located/trusted (see docs/ARCHITECTURE.md, "Pose State
        // (M10)"). Input::PoseActionState{} default-constructs with
        // active=false and both *Valid flags false - honest, not
        // fabricated.
        if (poseState.isActive == XR_FALSE)
        {
            return Input::PoseActionState{};
        }

        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        CheckXrResult(instance, xrLocateSpace(AimSpace(hand).Get(), baseSpace, time, &location), "xrLocateSpace (aim_pose)");
        return ConvertActionStatePose(true, location);
    }
}
