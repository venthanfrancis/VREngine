#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// Deliberately no Vulkan dependency: an XrActionSet is a pure OpenXR
// concept, independent of which graphics API backs the session. See
// docs/ARCHITECTURE.md, "Action Set (M10)".

#include <openxr/openxr.h>

#include <cstdint>

namespace AREngine::XR::OpenXR
{
    // Owns one XrActionSet. One primary "gameplay" action set for M10 -
    // no evidence yet justifies more than one (see
    // docs/ARCHITECTURE.md, "Action Set (M10)"). `name` must already be
    // a lowercase, OpenXR-compliant identifier (letters/digits/./_/-
    // only, per the spec's actionSetName rules) - not validated here,
    // same trust-the-caller discipline as every other *CreateInfo
    // wrapper in this codebase.
    //
    // Not copyable or movable: exactly one XrActionSet per
    // OpenXRActionSet, destroyed exactly once (xrDestroyActionSet), by
    // this object alone. Destroying an XrActionSet implicitly destroys
    // every XrAction created from it (per the OpenXR spec's object
    // model) - callers must still destroy their own OpenXRAction
    // wrappers first (each explicitly calls xrDestroyAction, spec-legal
    // to do before the owning set is destroyed), and every
    // OpenXRActionSpace built from one of this set's actions, before
    // this object is destroyed - see docs/ARCHITECTURE.md, "Action
    // Lifetime / Destruction Order (M10)".
    class OpenXRActionSet
    {
    public:
        OpenXRActionSet(XrInstance instance, const char* name, const char* localizedName, std::uint32_t priority = 0);
        ~OpenXRActionSet();

        OpenXRActionSet(const OpenXRActionSet&) = delete;
        OpenXRActionSet& operator=(const OpenXRActionSet&) = delete;
        OpenXRActionSet(OpenXRActionSet&&) = delete;
        OpenXRActionSet& operator=(OpenXRActionSet&&) = delete;

        [[nodiscard]] XrActionSet Get() const { return m_actionSet; }

        // Calls xrAttachSessionActionSets with just this one action
        // set. Must be called exactly once per session, after every
        // action belonging to this set has been created and every
        // interaction-profile binding has been suggested
        // (xrSuggestInteractionProfileBindings), and before any
        // xrSyncActions/xrGetActionState* call against `session` - see
        // docs/ARCHITECTURE.md, "Attach Action Set (M10)". Calling this
        // more than once, or after any action state has already been
        // queried, is a spec violation this class does not guard
        // against (same "trust the caller to follow documented call
        // order" discipline as XRFrameDriver's own Begin/End contract).
        void Attach(XrInstance instance, XrSession session) const;

    private:
        XrActionSet m_actionSet = XR_NULL_HANDLE;
    };
}
