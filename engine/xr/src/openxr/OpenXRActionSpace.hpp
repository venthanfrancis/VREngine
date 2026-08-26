#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: an XrSpace created from a pose
// action is a pure OpenXR/tracking concept, independent of which
// graphics API backs the session - same reasoning as
// OpenXRReferenceSpace.hpp, whose IdentityPose() this file reuses
// rather than duplicating. See docs/ARCHITECTURE.md, "Action Spaces
// (M10)".

#include "OpenXRReferenceSpace.hpp" // IdentityPose()

#include <openxr/openxr.h>

namespace AREngine::XR::OpenXR
{
    // Owns one XrSpace created via xrCreateActionSpace, for one pose
    // action + one specific subaction path (a shared left/right pose
    // action needs TWO of these - one per hand - since
    // XrActionSpaceCreateInfo binds a single subactionPath, not a set;
    // see docs/ARCHITECTURE.md). `poseInActionSpace` defaults to
    // identity - no evidence yet requires a real grip/aim offset beyond
    // what the interaction profile's own aim/grip pose component
    // already provides.
    //
    // Not copyable or movable: exactly one XrSpace per
    // OpenXRActionSpace, destroyed exactly once (xrDestroySpace), by
    // this object alone - must be destroyed before the XrSession (and
    // OpenXRSession) it was created from, same discipline as
    // OpenXRReferenceSpace.
    class OpenXRActionSpace
    {
    public:
        OpenXRActionSpace(XrInstance instance, XrSession session, XrAction action, XrPath subactionPath,
                           XrPosef poseInActionSpace = IdentityPose());
        ~OpenXRActionSpace();

        OpenXRActionSpace(const OpenXRActionSpace&) = delete;
        OpenXRActionSpace& operator=(const OpenXRActionSpace&) = delete;
        OpenXRActionSpace(OpenXRActionSpace&&) = delete;
        OpenXRActionSpace& operator=(OpenXRActionSpace&&) = delete;

        [[nodiscard]] XrSpace Get() const { return m_space; }

    private:
        XrSpace m_space = XR_NULL_HANDLE;
    };
}
