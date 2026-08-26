#pragma once

// M10.6: connects generic AREngine::Input action state to a small,
// demo-owned visual interaction state - the missing link M10/M10.5
// left deliberately unbuilt ("input still does not affect anything").
// See docs/ARCHITECTURE.md, "M10.6 Implementation Notes".
//
// Deliberately generic-types-only: this header includes ONLY
// AREngine::Input/AREngine::Core::Math headers - no <openxr/openxr.h>,
// no XrAction/XrPath/XrPosef/XrVector2f/XrSpace anywhere, confirmed by
// this file's own include list. The OpenXR -> Input::*ActionState
// conversion already happened one layer up (M10's
// OpenXRActionStateConversion.hpp); this file only ever sees the
// generic result, matching the milestone's required dependency
// direction: OpenXRActionSystem -> Input::*ActionState -> (this file)
// -> Transform/tint -> renderer. Rendering never queries OpenXR
// directly, and this file never queries OpenXR either.
//
// Kept at the tests/ leaf, NOT promoted into engine/input: the FIELDS
// of XRInteractionState (a highlight bool, a scale factor, a move
// offset, a pose-marker transform) are tailored to this specific
// demo's own visualization choices (a reference cube's tint/scale, one
// diagnostic cube's position, one debug marker), not a generic shape
// any application would reuse verbatim - promoting it would be
// "engine API to make the milestone look larger," which the milestone
// brief explicitly warns against. The underlying MECHANISMS (toggle-
// on-press, linear analog-to-range mapping, bounded vector offset,
// valid-pose gating) are each only a few lines - not substantial enough
// to justify a reusable "interaction primitives" library with exactly
// one consumer. See docs/ARCHITECTURE.md, "Where Interaction Logic
// Lives (M10.6)" for the full reasoning.
//
// Pure logic throughout - no I/O, no OpenXR, no Vulkan, no rendering -
// fully unit-testable with synthetic AREngine::Input::*ActionState
// values (see tests/xr_interaction_tests.cpp). Synthetic values are
// legitimate FOR TESTS ONLY - tests/xr_demo.cpp itself must never feed
// synthetic/fabricated input into this API in place of real queried
// OpenXR state (see docs/ARCHITECTURE.md, "No Fake Live Input (M10.6)").

#include "AREngine/Input/ActionState.hpp"

#include "AREngine/Core/Math/Quaternion.hpp"
#include "AREngine/Core/Math/Vec2.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

namespace ARDemo
{
    // One shared world-visualization state, updated once per frame from
    // ONE hand's queried action state (not per-eye, not per-view) - the
    // same updated state is read by every XR view's render pass. See
    // docs/ARCHITECTURE.md, "Shared World State (M10.6)".
    struct XRInteractionState
    {
        // select.pressed toggles this - never re-toggled while held.
        bool highlightEnabled = false;

        // Multiplies the reference cube's own base scale. 1.0 (neutral)
        // whenever the trigger action is inactive - never a stale prior
        // value.
        float scaleFactor = 1.0f;

        // Direct (non-delta-time) offset applied to one diagnostic
        // cube's LOCAL-space position, bounded to
        // +/-kMaxMoveOffsetMeters on each axis. (0,0) whenever the move
        // action is inactive.
        AREngine::Core::Math::Vec2 moveOffset;

        // True only when the aim-pose action is active AND the located
        // space reports BOTH position and orientation valid - the
        // conservative policy chosen here (see
        // docs/ARCHITECTURE.md, "Pose Marker Visibility Policy (M10.6)").
        // When false, position/orientation below are reset to their
        // defaults (never a stale or fabricated pose) and the marker
        // must not be drawn.
        bool poseMarkerVisible = false;
        AREngine::Core::Math::Vec3 poseMarkerPosition;
        AREngine::Core::Math::Quaternion poseMarkerOrientation = AREngine::Core::Math::Quaternion::Identity();
    };

    // Bounds for the move-offset mapping - purely a visualization
    // choice (keeps the diagnostic cube from wandering off past the
    // reference scene), not a gameplay balance decision.
    inline constexpr float kMaxMoveOffsetMeters = 0.5f;
    inline constexpr float kMoveOffsetMetersPerUnit = 0.5f;

    // Scale-factor range for the analog mapping: trigger 0.0 -> 1.0x
    // (neutral, same as ApplyAnalogScale's own inactive default),
    // trigger 1.0 -> 1.0 + kScaleFactorRange.
    inline constexpr float kScaleFactorRange = 0.8f;

    // Toggles state.highlightEnabled on select.pressed - never on
    // select.down alone (a held action must not re-toggle every
    // frame). An inactive DigitalActionState always has pressed=false
    // by construction (OpenXRActionStateConversion.hpp's
    // ConvertActionStateBoolean already guarantees this), so no
    // separate "is active" check is needed here - honored
    // automatically, not a coincidence this function relies on
    // silently (see the test that confirms it explicitly).
    void ApplyDigitalToggle(const AREngine::Input::DigitalActionState& select, XRInteractionState& state);

    // Sets state.scaleFactor = 1.0 + trigger.value * kScaleFactorRange
    // while active, or exactly 1.0 (neutral) while inactive - never a
    // stale prior reading.
    void ApplyAnalogScale(const AREngine::Input::AnalogActionState& trigger, XRInteractionState& state);

    // Sets state.moveOffset = clamp(move.value * kMoveOffsetMetersPerUnit,
    // +/-kMaxMoveOffsetMeters) while active, or (0,0) while inactive.
    // Deliberately a DIRECT function of the current value, never
    // integrated over delta time - this is a visualization of the
    // current input state, not a player-controller movement system.
    void ApplyVectorOffset(const AREngine::Input::Vector2ActionState& move, XRInteractionState& state);

    // Sets state.poseMarkerVisible/Position/Orientation from `pose` -
    // see XRInteractionState::poseMarkerVisible's own doc comment for
    // the exact visibility policy. Never fabricates a pose: when not
    // visible, position/orientation are reset to their type defaults,
    // not left holding a stale previous pose.
    void ApplyPoseMarker(const AREngine::Input::PoseActionState& pose, XRInteractionState& state);
}
