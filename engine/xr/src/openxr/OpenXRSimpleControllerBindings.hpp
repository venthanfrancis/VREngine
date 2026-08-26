#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: interaction-profile binding
// suggestion is a pure OpenXR concept, independent of which graphics
// API backs the session. See docs/ARCHITECTURE.md, "Interaction
// Profile / Suggested Bindings (M10)".

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // Suggests bindings for the KHR simple_controller interaction
    // profile (/interaction_profiles/khr/simple_controller) - the only
    // profile M10 targets, per the milestone's own explicit priority.
    // Verified against the OpenXR 1.0 specification's standard
    // interaction profile table, not guessed: simple_controller defines
    // exactly /input/select/click, /input/menu/click, /input/grip/pose,
    // /input/aim/pose, and /output/haptic per top-level user path - no
    // trigger and no thumbstick/2D component exists on this profile.
    //
    // `selectAction` (boolean) is bound to select/click and
    // `poseAction` (pose) to aim/pose, for both hands (this function
    // resolves the full left/right component paths itself via
    // xrStringToPath - callers do not need to pass hand paths in).
    // `triggerAction`/`moveAction` are deliberately NOT passed here and
    // are not bound to anything under this profile - they remain
    // valid, created actions that will correctly report isActive=false
    // while simple_controller is the active profile (see
    // docs/ARCHITECTURE.md for why a second, richer profile was not
    // also targeted in M10).
    void SuggestSimpleControllerBindings(XrInstance instance, XrAction selectAction, XrAction poseAction);
}
