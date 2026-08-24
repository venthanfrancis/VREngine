#pragma once

// Private OpenXR bring-up implementation — see OpenXRVersion.hpp for
// the API version this targets.

#include <openxr/openxr.h>

#include <span>
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

    // True if `name` appears among `extensions` (as returned by
    // EnumerateInstanceExtensions above). Pure logic over already-
    // queried data, no OpenXR calls - directly unit-testable with
    // synthetic XrExtensionProperties. Exists so a caller can
    // explicitly verify a specific extension (e.g. M9C's
    // XR_KHR_vulkan_enable2) is genuinely supported before requesting
    // it on instance creation, rather than requesting it optimistically
    // and only discovering failure from xrCreateInstance's result. See
    // docs/ARCHITECTURE.md, "XR_KHR_vulkan_enable2 Selection (M9C)".
    [[nodiscard]] bool IsExtensionSupported(const std::vector<XrExtensionProperties>& extensions, const char* name);

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
    // Requests zero API layers always - M9A's brief is explicit that
    // arbitrary layers must not be enabled, and nothing since has given
    // a reason to revisit that.
    //
    // `requestedExtensions` defaults to empty, preserving M9A's exact
    // original zero-extension behavior for every existing call site
    // (the M9A demo still default-constructs this class unchanged).
    // M9C extends this to accept specific extensions (XR_KHR_vulkan_enable2)
    // - callers are responsible for having already confirmed each one
    // is actually supported (e.g. via EnumerateInstanceExtensions()
    // above) before passing it here; this constructor does not
    // silently drop unsupported names or fall back to anything - if an
    // unsupported extension is requested, xrCreateInstance simply fails
    // (XR_ERROR_EXTENSION_NOT_PRESENT) and IsValid() reports it like any
    // other creation failure. See docs/ARCHITECTURE.md,
    // "XR_KHR_vulkan_enable2 Selection (M9C)" for why the support check
    // itself belongs in the caller, not silently inside this class.
    //
    // Not copyable or movable: exactly one XrInstance per
    // OpenXRInstance, destroyed exactly once (xrDestroyInstance), by
    // this object alone - same discipline as every other owned handle
    // in this engine's bring-up code.
    class OpenXRInstance
    {
    public:
        explicit OpenXRInstance(std::span<const char* const> requestedExtensions = {});
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
