#pragma once

// Private OpenXR bring-up implementation — see OpenXRVersion.hpp for
// the API version this targets.

#include <openxr/openxr.h>

#include <vector>

namespace AREngine::XR::OpenXR
{
    // Enumerates OpenXR API layers visible to the loader (no instance
    // required - this is a loader-level query). Logged for diagnostic
    // purposes only; M9A deliberately enables none of them (no
    // arbitrary API layers - see docs/ARCHITECTURE.md, "API Layer
    // Enumeration (M9A)"). Makes a real OpenXR call - not unit-tested,
    // only exercised by the manual bring-up demo. Returns an empty
    // vector (with a logged warning, not an assert) if the call itself
    // fails - even this early, loader-only query can fail with no
    // runtime installed.
    [[nodiscard]] std::vector<XrApiLayerProperties> EnumerateApiLayers();

    // Enumerates OpenXR instance extensions the active runtime (plus
    // any loader-implicit ones) supports (no instance required for
    // this query either). Logged for diagnostic purposes only; M9A
    // enables none of them - no graphics-binding, hand-tracking,
    // eye-tracking, passthrough, or spatial-anchor extensions belong in
    // this milestone. See docs/ARCHITECTURE.md, "Instance Extension
    // Enumeration (M9A)". Same graceful-empty-on-failure behavior as
    // EnumerateApiLayers.
    [[nodiscard]] std::vector<XrExtensionProperties> EnumerateInstanceExtensions();

    // Owns an XrInstance. Deliberately does NOT assert or abort if
    // xrCreateInstance fails - unlike every other RAII bring-up wrapper
    // in this engine (VulkanInstance, etc.), "no OpenXR runtime
    // installed/active" is a completely ordinary machine state (most
    // desktop/dev machines have none), not a programmer error to crash
    // on. Callers must check IsValid() before calling Get(), and may
    // inspect CreationResult() to distinguish *why* creation failed.
    // See docs/ARCHITECTURE.md, "Instance Creation Failure Handling
    // (M9A)".
    //
    // Requests zero API layers and zero extensions - M9A's brief is
    // explicit that arbitrary layers must not be enabled, and that
    // graphics-binding/hand-tracking/eye-tracking/passthrough/spatial-
    // anchor extensions are all out of scope until later milestones.
    //
    // Not copyable or movable: exactly one XrInstance per
    // OpenXRInstance, destroyed exactly once (xrDestroyInstance), by
    // this object alone - same discipline as every other owned handle
    // in this engine's bring-up code.
    class OpenXRInstance
    {
    public:
        OpenXRInstance();
        ~OpenXRInstance();

        OpenXRInstance(const OpenXRInstance&) = delete;
        OpenXRInstance& operator=(const OpenXRInstance&) = delete;
        OpenXRInstance(OpenXRInstance&&) = delete;
        OpenXRInstance& operator=(OpenXRInstance&&) = delete;

        [[nodiscard]] bool IsValid() const { return m_instance != XR_NULL_HANDLE; }
        [[nodiscard]] XrInstance Get() const { return m_instance; }

        // The raw xrCreateInstance result, so a caller can distinguish
        // why creation failed (XR_ERROR_RUNTIME_UNAVAILABLE - no
        // runtime installed/active - from anything else) without this
        // class deciding what that distinction means for them. Only
        // meaningful when IsValid() is false.
        [[nodiscard]] XrResult CreationResult() const { return m_creationResult; }

    private:
        XrInstance m_instance = XR_NULL_HANDLE;
        XrResult m_creationResult = XR_SUCCESS;
    };
}
