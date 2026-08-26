#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: an XrAction is a pure OpenXR
// concept, independent of which graphics API backs the session. See
// docs/ARCHITECTURE.md, "Actions To Create (M10)".

#include <openxr/openxr.h>

#include <vector>

namespace AREngine::XR::OpenXR
{
    // Owns one XrAction, created within an already-existing
    // XrActionSet. `subactionPaths` may be empty (an action with no
    // subaction paths) or, for M10's shared left/right actions, the
    // two hand paths - one XrAction serves both hands, distinguished
    // at query/space-creation time by which subaction path is passed
    // in, rather than creating separate left_x/right_x actions (see
    // docs/ARCHITECTURE.md, "Left/Right Hand Representation (M10)" for
    // why the shared-action model was chosen).
    //
    // Not copyable or movable: exactly one XrAction per OpenXRAction,
    // destroyed exactly once (xrDestroyAction), by this object alone -
    // must be destroyed before the OpenXRActionSet it was created from
    // (see that class's own destruction-order documentation).
    class OpenXRAction
    {
    public:
        OpenXRAction(XrInstance instance, XrActionSet actionSet, const char* name, const char* localizedName,
                     XrActionType type, const std::vector<XrPath>& subactionPaths = {});
        ~OpenXRAction();

        OpenXRAction(const OpenXRAction&) = delete;
        OpenXRAction& operator=(const OpenXRAction&) = delete;
        OpenXRAction(OpenXRAction&&) = delete;
        OpenXRAction& operator=(OpenXRAction&&) = delete;

        [[nodiscard]] XrAction Get() const { return m_action; }

    private:
        XrAction m_action = XR_NULL_HANDLE;
    };
}
