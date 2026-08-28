#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: interaction-profile binding
// suggestion is a pure OpenXR concept, independent of which graphics
// API backs the session. See docs/ARCHITECTURE.md, "M11.1B
// Interaction-Profile Binding".
//
// M10's khr/simple_controller binding (OpenXRSimpleControllerBindings.hpp)
// left `trigger`/`move` permanently unbound - it has no trigger or
// thumbstick component at all. M11.1B found Meta XR Simulator (system
// name "Meta Quest 3") resolves to exactly khr/simple_controller when
// that is the only profile AREngine suggests bindings for - OpenXR's own
// binding-resolution only ever selects among the profiles the
// application actually suggested, so a richer profile must be suggested
// for the runtime to ever report it. This file adds
// /interaction_profiles/oculus/touch_controller - the standard, core-OpenXR
// (present since 1.0, no vendor extension) profile for Quest/Touch-style
// controllers - ALONGSIDE simple_controller, never replacing it: an
// application may suggest bindings for multiple profiles simultaneously,
// and the runtime picks whichever one actually matches the connected
// (or, here, simulated) device.

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // The literal OpenXR path strings this binding suggests, kept as
    // pure data separate from the real xrStringToPath/
    // xrSuggestInteractionProfileBindings calls below - this struct
    // (and GetTouchControllerBindingPaths()) has no OpenXR API
    // dependency at all and is directly unit-testable, unlike the real
    // API calls in SuggestTouchControllerBindings() itself (same
    // "real API call -> not unit tested" precedent as
    // SuggestSimpleControllerBindings/LoadOpenXRVulkanFunctions
    // elsewhere in this codebase).
    struct TouchControllerBindingPaths
    {
        const char* profile;
        const char* leftSelect;
        const char* rightSelect;
        const char* leftTrigger;
        const char* rightTrigger;
        const char* leftMove;
        const char* rightMove;
        const char* leftAimPose;
        const char* rightAimPose;
    };

    [[nodiscard]] constexpr TouchControllerBindingPaths GetTouchControllerBindingPaths()
    {
        return TouchControllerBindingPaths{
            "/interaction_profiles/oculus/touch_controller",
            "/user/hand/left/input/x/click",
            "/user/hand/right/input/a/click",
            "/user/hand/left/input/trigger/value",
            "/user/hand/right/input/trigger/value",
            "/user/hand/left/input/thumbstick",
            "/user/hand/right/input/thumbstick",
            "/user/hand/left/input/aim/pose",
            "/user/hand/right/input/aim/pose",
        };
    }

    // Suggests bindings for /interaction_profiles/oculus/touch_controller.
    // Verified against the OpenXR 1.1 specification's standard
    // interaction profile table for this exact profile, not guessed:
    //
    //   Left:  /input/x/click, /input/x/touch, /input/y/click, /input/y/touch,
    //          /input/menu/click, /input/squeeze/value, /input/trigger/value,
    //          /input/trigger/touch, /input/thumbstick (2D), /input/thumbstick/click,
    //          /input/thumbstick/touch, /input/thumbrest/touch, /input/grip/pose,
    //          /input/aim/pose, /output/haptic
    //   Right: /input/a/click, /input/a/touch, /input/b/click, /input/b/touch,
    //          /input/system/click, /input/squeeze/value, /input/trigger/value,
    //          /input/trigger/touch, /input/thumbstick (2D), /input/thumbstick/click,
    //          /input/thumbstick/touch, /input/thumbrest/touch, /input/grip/pose,
    //          /input/aim/pose, /output/haptic
    //
    // Component mapping (documented, not arbitrary):
    //   - `selectAction` (boolean) -> /input/x/click (left) / /input/a/click
    //     (right) - this profile has no literal "select/click" component
    //     (per the milestone's own explicit warning about Touch-style
    //     profiles), so the primary face button is used instead -
    //     distinct from `triggerAction` below, so the two remain
    //     independently testable inputs, matching AREngine's own
    //     select-vs-trigger action semantics.
    //   - `triggerAction` (float) -> /input/trigger/value - a genuine
    //     float-capable component, never a boolean click bound to a
    //     float action.
    //   - `moveAction` (Vector2f) -> /input/thumbstick (the AGGREGATE 2D
    //     path, never the scalar /input/thumbstick/x or .../y alone -
    //     binding a Vector2f action to the aggregate path is what the
    //     spec defines for 2D thumbstick input).
    //   - `poseAction` (pose) -> /input/aim/pose, unchanged in shape
    //     from the simple_controller binding - never grip/pose.
    //
    // For both hands (resolves the full left/right component paths
    // itself via xrStringToPath - callers do not need to pass hand
    // paths in).
    void SuggestTouchControllerBindings(
        XrInstance instance, XrAction selectAction, XrAction triggerAction, XrAction moveAction, XrAction poseAction);
}
