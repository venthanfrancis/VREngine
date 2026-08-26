#include "OpenXRActionPaths.hpp"

#include "OpenXRResult.hpp"

namespace AREngine::XR::OpenXR
{
    OpenXRHandPaths ResolveHandSubactionPaths(XrInstance instance)
    {
        OpenXRHandPaths paths;
        CheckXrResult(instance, xrStringToPath(instance, "/user/hand/left", &paths.left), "xrStringToPath (/user/hand/left)");
        CheckXrResult(instance, xrStringToPath(instance, "/user/hand/right", &paths.right), "xrStringToPath (/user/hand/right)");
        return paths;
    }
}
