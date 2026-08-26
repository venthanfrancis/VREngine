#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: XrPath resolution is a pure
// OpenXR concept, independent of which graphics API backs the session.
// See docs/ARCHITECTURE.md, "Subaction Paths (M10)".

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // The two hand subaction paths M10 uses - /user/hand/left and
    // /user/hand/right, resolved once via xrStringToPath (never a
    // hard-coded numeric XrPath value - the OpenXR spec does not
    // guarantee any particular numeric value across runtimes). See
    // ResolveHandSubactionPaths below.
    struct OpenXRHandPaths
    {
        XrPath left = XR_NULL_PATH;
        XrPath right = XR_NULL_PATH;
    };

    // Makes two real xrStringToPath calls - not unit-tested (a real
    // OpenXR API call, same category as every other *_TypeToPath-style
    // helper in this codebase), only exercised by the manual input
    // demo.
    [[nodiscard]] OpenXRHandPaths ResolveHandSubactionPaths(XrInstance instance);
}
