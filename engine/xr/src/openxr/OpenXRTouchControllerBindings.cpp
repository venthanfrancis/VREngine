#include "OpenXRTouchControllerBindings.hpp"

#include "OpenXRResult.hpp"

#include <array>

namespace AREngine::XR::OpenXR
{
    void SuggestTouchControllerBindings(
        XrInstance instance, XrAction selectAction, XrAction triggerAction, XrAction moveAction, XrAction poseAction)
    {
        const TouchControllerBindingPaths paths = GetTouchControllerBindingPaths();

        XrPath leftSelect = XR_NULL_PATH;
        XrPath rightSelect = XR_NULL_PATH;
        XrPath leftTrigger = XR_NULL_PATH;
        XrPath rightTrigger = XR_NULL_PATH;
        XrPath leftThumbstick = XR_NULL_PATH;
        XrPath rightThumbstick = XR_NULL_PATH;
        XrPath leftAimPose = XR_NULL_PATH;
        XrPath rightAimPose = XR_NULL_PATH;
        XrPath profile = XR_NULL_PATH;

        CheckXrResult(instance, xrStringToPath(instance, paths.leftSelect, &leftSelect),
            "xrStringToPath (x/click, left)");
        CheckXrResult(instance, xrStringToPath(instance, paths.rightSelect, &rightSelect),
            "xrStringToPath (a/click, right)");
        CheckXrResult(instance, xrStringToPath(instance, paths.leftTrigger, &leftTrigger),
            "xrStringToPath (trigger/value, left)");
        CheckXrResult(instance, xrStringToPath(instance, paths.rightTrigger, &rightTrigger),
            "xrStringToPath (trigger/value, right)");
        CheckXrResult(instance, xrStringToPath(instance, paths.leftMove, &leftThumbstick),
            "xrStringToPath (thumbstick, left)");
        CheckXrResult(instance, xrStringToPath(instance, paths.rightMove, &rightThumbstick),
            "xrStringToPath (thumbstick, right)");
        CheckXrResult(instance, xrStringToPath(instance, paths.leftAimPose, &leftAimPose),
            "xrStringToPath (aim/pose, left)");
        CheckXrResult(instance, xrStringToPath(instance, paths.rightAimPose, &rightAimPose),
            "xrStringToPath (aim/pose, right)");
        CheckXrResult(instance, xrStringToPath(instance, paths.profile, &profile),
            "xrStringToPath (oculus/touch_controller)");

        const std::array<XrActionSuggestedBinding, 8> bindings{{
            {selectAction, leftSelect},
            {selectAction, rightSelect},
            {triggerAction, leftTrigger},
            {triggerAction, rightTrigger},
            {moveAction, leftThumbstick},
            {moveAction, rightThumbstick},
            {poseAction, leftAimPose},
            {poseAction, rightAimPose},
        }};

        XrInteractionProfileSuggestedBinding suggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBinding.interactionProfile = profile;
        suggestedBinding.countSuggestedBindings = static_cast<std::uint32_t>(bindings.size());
        suggestedBinding.suggestedBindings = bindings.data();
        CheckXrResult(instance, xrSuggestInteractionProfileBindings(instance, &suggestedBinding),
            "xrSuggestInteractionProfileBindings (oculus/touch_controller)");
    }
}
