#include "OpenXRSimpleControllerBindings.hpp"

#include "OpenXRResult.hpp"

#include <array>

namespace AREngine::XR::OpenXR
{
    void SuggestSimpleControllerBindings(XrInstance instance, XrAction selectAction, XrAction poseAction)
    {
        XrPath leftSelect = XR_NULL_PATH;
        XrPath rightSelect = XR_NULL_PATH;
        XrPath leftAimPose = XR_NULL_PATH;
        XrPath rightAimPose = XR_NULL_PATH;
        XrPath profile = XR_NULL_PATH;
        CheckXrResult(instance, xrStringToPath(instance, "/user/hand/left/input/select/click", &leftSelect),
            "xrStringToPath (select/click, left)");
        CheckXrResult(instance, xrStringToPath(instance, "/user/hand/right/input/select/click", &rightSelect),
            "xrStringToPath (select/click, right)");
        CheckXrResult(instance, xrStringToPath(instance, "/user/hand/left/input/aim/pose", &leftAimPose),
            "xrStringToPath (aim/pose, left)");
        CheckXrResult(instance, xrStringToPath(instance, "/user/hand/right/input/aim/pose", &rightAimPose),
            "xrStringToPath (aim/pose, right)");
        CheckXrResult(instance, xrStringToPath(instance, "/interaction_profiles/khr/simple_controller", &profile),
            "xrStringToPath (khr/simple_controller)");

        const std::array<XrActionSuggestedBinding, 4> bindings{{
            {selectAction, leftSelect},
            {selectAction, rightSelect},
            {poseAction, leftAimPose},
            {poseAction, rightAimPose},
        }};

        XrInteractionProfileSuggestedBinding suggestedBinding{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestedBinding.interactionProfile = profile;
        suggestedBinding.countSuggestedBindings = static_cast<std::uint32_t>(bindings.size());
        suggestedBinding.suggestedBindings = bindings.data();
        CheckXrResult(instance, xrSuggestInteractionProfileBindings(instance, &suggestedBinding),
            "xrSuggestInteractionProfileBindings (khr/simple_controller)");
    }
}
