#pragma once

// Private OpenXR/Vulkan integration boundary - see
// OpenXRVulkanGraphicsBinding.hpp for the vulkan.h/openxr_platform.h
// include-order requirement this file also depends on transitively.
//
// XrSession creation is the one part of M9D's session lifecycle that
// genuinely needs Vulkan (XrGraphicsBindingVulkan2KHR must be chained
// through XrSessionCreateInfo::next) - everything else about session
// *state* (OpenXRSessionState.hpp), view configuration
// (OpenXRViewConfiguration.hpp), and reference spaces
// (OpenXRReferenceSpace.hpp) is a pure OpenXR concept with no graphics-
// API dependency at all, which is why those three files have none. See
// docs/ARCHITECTURE.md, "XrSession Ownership (M9D)".

#include "OpenXRVulkanGraphicsBinding.hpp"

#include <cstdint>

namespace AREngine::XR::OpenXR
{
    // Result of draining every pending OpenXR event for one polling
    // cycle (xrPollEvent in a loop until XR_EVENT_UNAVAILABLE, each
    // call using a freshly zero-initialized XrEventDataBuffer as
    // OpenXR requires). Only reports the two event types M9D cares
    // about - unrelated event types are safely ignored, not routed
    // through a giant dispatch system. See docs/ARCHITECTURE.md,
    // "Event Polling (M9D)".
    //
    // If multiple XrEventDataSessionStateChanged events arrive within
    // one poll cycle, `newSessionState` reflects the LAST one - it is
    // the current, authoritative state; earlier ones in the same cycle
    // are already stale by the time polling returns.
    struct SessionEventPollResult
    {
        bool sessionStateChanged = false;
        XrSessionState newSessionState = XR_SESSION_STATE_UNKNOWN;
        bool instanceLossPending = false;
        XrTime lossTime = 0;
    };

    // Makes real xrPollEvent calls - not unit-tested, only exercised
    // by the manual session demo.
    [[nodiscard]] SessionEventPollResult PollSessionEvents(XrInstance instance);

    // Owns one XrSession, created via xrCreateSession with a
    // XrGraphicsBindingVulkan2KHR (built directly from the caller's
    // already-validated M9C `VulkanGraphicsBindingData` - no second
    // Vulkan instance/device is created here, the M9C-created objects
    // are reused as-is) chained through XrSessionCreateInfo::next. Not
    // a headless session. `createFlags` stays zero - nothing in M9D
    // needs anything else. See docs/ARCHITECTURE.md, "Graphics Binding
    // Relationship (M9D)".
    //
    // Tracks whether xrBeginSession has succeeded (IsRunning()) as a
    // piece of state kept SEPARATE from whatever XrSessionState a
    // caller is independently tracking via PollSessionEvents - see
    // docs/ARCHITECTURE.md, "Session Running Flag (M9D)". This class
    // does not track XrSessionState itself and does not decide when to
    // call BeginSession/EndSession - callers drive that using
    // OpenXRSessionState.hpp's ShouldBeginSession/ShouldEndSession
    // against their own tracked state.
    //
    // Not copyable or movable: exactly one XrSession per OpenXRSession,
    // destroyed exactly once (xrDestroySession), by this object alone.
    class OpenXRSession
    {
    public:
        // Does NOT own `instance` - the caller must keep its
        // OpenXRInstance alive for this object's entire lifetime, same
        // borrowing discipline OpenXRVulkanGraphicsBinding already
        // established in M9C.
        OpenXRSession(XrInstance instance, XrSystemId systemId, const VulkanGraphicsBindingData& bindingData);
        ~OpenXRSession();

        OpenXRSession(const OpenXRSession&) = delete;
        OpenXRSession& operator=(const OpenXRSession&) = delete;
        OpenXRSession(OpenXRSession&&) = delete;
        OpenXRSession& operator=(OpenXRSession&&) = delete;

        [[nodiscard]] XrSession Get() const { return m_session; }
        [[nodiscard]] bool IsRunning() const { return m_running; }

        // Calls xrBeginSession with `primaryViewConfigurationType`.
        // Callers should only invoke this when
        // ShouldBeginSession(currentState) is true - this method does
        // not check XrSessionState itself (it does not track one).
        void BeginSession(XrViewConfigurationType primaryViewConfigurationType);

        // Calls xrEndSession. Callers should only invoke this when
        // ShouldEndSession(currentState, IsRunning()) is true.
        void EndSession();

        // Calls xrRequestExitSession - asks the runtime to begin
        // winding the session down. Does NOT call xrEndSession itself;
        // expect the runtime to drive the rest of the lifecycle
        // (STOPPING, then EXITING) via subsequent PollSessionEvents
        // calls. See docs/ARCHITECTURE.md, "How The Demo Ends (M9D)".
        void RequestExit();

    private:
        XrInstance m_instance = XR_NULL_HANDLE; // borrowed, not owned - see the class comment above
        XrSession m_session = XR_NULL_HANDLE;
        bool m_running = false;
    };
}
