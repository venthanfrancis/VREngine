#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.

#include <openxr/openxr.h>

#include <string>

namespace AREngine::XR::OpenXR
{
    // Human-readable name for an XrSessionState (e.g.
    // "XR_SESSION_STATE_READY"). Pure logic (a plain switch), no
    // OpenXR calls - directly unit-testable. Falls back to a numeric
    // representation for any value not in the known set (mirrors
    // XrResultToReadableString's numeric-fallback discipline from
    // OpenXRResult.hpp).
    [[nodiscard]] std::string FormatSessionState(XrSessionState state);

    // True only when `state` is XR_SESSION_STATE_READY - the one state
    // in which xrBeginSession should be called. Session state is
    // deliberately never treated as a simple boolean anywhere in this
    // engine - see docs/ARCHITECTURE.md, "Session States (M9D)".
    [[nodiscard]] constexpr bool ShouldBeginSession(XrSessionState state)
    {
        return state == XR_SESSION_STATE_READY;
    }

    // True only when `state` is XR_SESSION_STATE_STOPPING AND the
    // session is currently running. `sessionRunning` is intentionally
    // a separate parameter, not inferred from `state` - whether
    // xrBeginSession has actually succeeded is tracked independently
    // of XrSessionState (see docs/ARCHITECTURE.md, "Session Running
    // Flag (M9D)"), since a runtime could in principle reach STOPPING
    // without a session ever having begun.
    [[nodiscard]] constexpr bool ShouldEndSession(XrSessionState state, bool sessionRunning)
    {
        return state == XR_SESSION_STATE_STOPPING && sessionRunning;
    }

    // True when a demo's main loop should stop:
    // XR_SESSION_STATE_EXITING (a normal, requested exit - not an
    // error) or XR_SESSION_STATE_LOSS_PENDING (the runtime/system is
    // being lost; M9D does not attempt session recreation, just a
    // clean stop). See docs/ARCHITECTURE.md, "EXITING (M9D)" and
    // "LOSS_PENDING (M9D)".
    [[nodiscard]] constexpr bool ShouldStopMainLoop(XrSessionState state)
    {
        return state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING;
    }
}
