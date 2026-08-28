#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: the OpenXR action model (action
// set, actions, action spaces, sync, state queries) is a pure OpenXR
// concept, independent of which graphics API backs the session. See
// docs/ARCHITECTURE.md, "M10 Implementation Notes".
//
// M10's small "OpenXRActionSystem" the milestone brief itself suggests
// as an alternative to growing XRFrameDriver into frame lifecycle +
// views + input + rendering + actions. Owns exactly the OpenXR action
// object model (one action set, four actions, two pose action spaces)
// and the bookkeeping needed to convert raw OpenXR action state into
// AREngine's generic Input::*ActionState types (AREngine/Input/
// ActionState.hpp) - nothing about frame pacing, view location, or
// rendering. Does NOT poll OpenXR session-state events, does NOT
// create the XrSession, does NOT know about Scene or gameplay - see
// docs/ARCHITECTURE.md for the full audit this shape was derived from.

#include "OpenXRAction.hpp"
#include "OpenXRActionPaths.hpp"
#include "OpenXRActionSet.hpp"
#include "OpenXRActionSpace.hpp"

#include "AREngine/Input/ActionState.hpp"

#include <openxr/openxr.h>

#include <array>
#include <memory>
#include <string>

namespace AREngine::XR::OpenXR
{
    enum class Hand
    {
        Left,
        Right,
    };

    // Not copyable or movable: exactly one action-object graph per
    // OpenXRActionSystem, destroyed exactly once, by this object alone
    // (in reverse declaration order - see the member list below and
    // docs/ARCHITECTURE.md, "Action Lifetime / Destruction Order
    // (M10)").
    class OpenXRActionSystem
    {
    public:
        // Creates the "gameplay" action set, the four M10 actions
        // (aim_pose/select/trigger/move, each spanning both hands via
        // shared subaction paths), suggests KHR simple_controller
        // bindings for aim_pose/select, attaches the action set to
        // `session`, and creates the left/right aim_pose action spaces
        // - all exactly once, in this order, in this constructor. See
        // docs/ARCHITECTURE.md, "Action Set / Action / Binding / Attach
        // / Action-Space Ordering (M10)" for why this exact order is
        // required (bindings before attach, per the OpenXR spec;
        // spaces after attach, matching established best practice).
        OpenXRActionSystem(XrInstance instance, XrSession session);
        ~OpenXRActionSystem() = default;

        OpenXRActionSystem(const OpenXRActionSystem&) = delete;
        OpenXRActionSystem& operator=(const OpenXRActionSystem&) = delete;
        OpenXRActionSystem(OpenXRActionSystem&&) = delete;
        OpenXRActionSystem& operator=(OpenXRActionSystem&&) = delete;

        // Calls xrSyncActions for this system's one action set. Must
        // only be called once the session is RUNNING (after
        // xrBeginSession) - see docs/ARCHITECTURE.md, "xrSyncActions
        // Placement (M10)". Call once per application tick, before any
        // GetXState() call for that tick.
        void SyncActions(XrInstance instance, XrSession session);

        [[nodiscard]] Input::DigitalActionState GetSelectState(XrInstance instance, XrSession session, Hand hand);
        [[nodiscard]] Input::AnalogActionState GetTriggerState(XrInstance instance, XrSession session, Hand hand);
        [[nodiscard]] Input::Vector2ActionState GetMoveState(XrInstance instance, XrSession session, Hand hand);

        // `baseSpace`/`time` are the caller's own existing LOCAL
        // reference space and current predicted display time (the same
        // ones XRFrameDriver's own view location uses) - never a space
        // or time this class invents itself. See docs/ARCHITECTURE.md,
        // "Predicted Display Time For Poses (M10)".
        [[nodiscard]] Input::PoseActionState GetAimPoseState(
            XrInstance instance, XrSession session, XrSpace baseSpace, XrTime time, Hand hand);

        // M11.1B diagnostic: queries xrGetCurrentInteractionProfile for
        // the given hand's top-level user path and returns the runtime-
        // reported interaction profile as a human-readable path string
        // (e.g. "/interaction_profiles/khr/simple_controller"), or an
        // empty string if the runtime reports XR_NULL_PATH (no profile
        // bound yet for that hand). Never guessed/hard-coded - this
        // reflects exactly what the runtime returns. See
        // docs/ARCHITECTURE.md, "M11.1B Implementation Notes".
        [[nodiscard]] std::string GetCurrentInteractionProfile(XrInstance instance, XrSession session, Hand hand) const;

    private:
        [[nodiscard]] XrPath SubactionPath(Hand hand) const { return hand == Hand::Left ? m_handPaths.left : m_handPaths.right; }
        [[nodiscard]] const OpenXRActionSpace& AimSpace(Hand hand) const { return hand == Hand::Left ? *m_leftAimSpace : *m_rightAimSpace; }

        OpenXRHandPaths m_handPaths;
        OpenXRActionSet m_actionSet;
        OpenXRAction m_aimPoseAction;
        OpenXRAction m_selectAction;
        OpenXRAction m_triggerAction;
        OpenXRAction m_moveAction;

        // Deferred (constructed in the constructor BODY, after Attach())
        // rather than via the member-initializer list - see the .cpp.
        std::unique_ptr<OpenXRActionSpace> m_leftAimSpace;
        std::unique_ptr<OpenXRActionSpace> m_rightAimSpace;

        // Previous-call select-down state, per hand - see
        // ConvertActionStateBoolean's own documentation for why this is
        // tracked here rather than derived from changedSinceLastSync.
        std::array<bool, 2> m_previousSelectDown{false, false};
    };
}
