#pragma once

#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec2.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

namespace AREngine::Input
{
    // Generic action-state vocabulary for input sources richer than
    // keyboard/mouse's simple digital-key model (M7's InputSystem) -
    // added in M10 specifically because real OpenXR controller input
    // introduces concepts keyboard/mouse never had: float trigger
    // values, 2D thumbstick values, tracked poses, and an explicit
    // "active" concept (an OpenXR action can be bound but temporarily
    // inactive - no interaction profile currently supplies it - which
    // has no keyboard/mouse equivalent; a physical key is always either
    // present or absent). These are plain data - this header knows
    // nothing about OpenXR, Win32, or any other specific input source,
    // and InputSystem/KeyCode/MouseButton are deliberately untouched by
    // it (no tracked pose, trigger, or thumbstick is modeled as a fake
    // KeyCode - see docs/ARCHITECTURE.md, "Generic Input Extension
    // (M10)" for the audit that justified adding these rather than
    // extending the existing keyboard/mouse types).
    //
    // "down"/"pressed"/"released" mirror InputSystem's own ButtonState
    // semantics exactly (M7): down = held this frame (including the
    // frame it was pressed and the frame before release), pressed/
    // released = true for exactly the one frame the transition
    // happened, never re-triggered while held. "active" has no
    // keyboard/mouse equivalent: when false, the action is not
    // currently bound to anything the runtime can read (e.g. the
    // active interaction profile has no matching component) - the
    // producer of one of these structs must never leave "down"/"value"
    // stale from a previous active state when active becomes false
    // (mirrors M7's WindowFocusLostEvent handling, which similarly
    // never leaves a key/button stuck Down after the signal that would
    // have released it stops arriving).
    struct DigitalActionState
    {
        bool down = false;
        bool pressed = false;
        bool released = false;
        bool active = false;
    };

    // `value`'s range is whatever the binding/runtime reports (commonly
    // 0..1 for a trigger, but never blindly clamped here - see
    // docs/ARCHITECTURE.md). Zero when `active` is false.
    struct AnalogActionState
    {
        float value = 0.0f;
        bool active = false;
    };

    // `value` is zero (not a stale previous reading) when `active` is
    // false. Not every interaction profile has a 2D input (e.g.
    // thumbstick) - callers must check `active`, never assume presence.
    struct Vector2ActionState
    {
        Core::Math::Vec2 value;
        bool active = false;
    };

    // `positionValid`/`orientationValid` reflect the OpenXR spec's own
    // distinction: pose data must not be used at all when the
    // corresponding VALID bit is unset - `position`/`orientation` are
    // only meaningful when their own valid flag is true, never
    // fabricated otherwise. `active` reflects the pose ACTION's own
    // activity (independent of, and checked before, space location) -
    // an inactive action's position/orientation are always default/
    // identity, never a stale previous pose.
    struct PoseActionState
    {
        Core::Math::Vec3 position;
        Core::Math::Quaternion orientation = Core::Math::Quaternion::Identity();
        bool positionValid = false;
        bool orientationValid = false;
        bool active = false;
    };
}
