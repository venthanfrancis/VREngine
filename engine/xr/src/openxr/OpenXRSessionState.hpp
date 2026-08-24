#pragma once

// Private OpenXR bring-up implementation — see OpenXRSession.hpp.
//
// DetermineFrameStatus/DetermineSessionLifecycleActions (M9E.5) give
// this file its first dependency on AREngine::Frame - deliberately
// added here rather than co-located with XRFrameDriver, since this
// file (unlike XRFrameDriver.hpp, which needs OpenXRSession and is
// therefore Vulkan-gated) is Vulkan-independent and can keep these two
// pure functions - and their tests - buildable/testable in the
// narrowest config (OPENXR=ON alone). See docs/ARCHITECTURE.md,
// "Where DetermineFrameStatus Lives (M9E.5)".

#include "AREngine/Frame/FrameStatus.hpp"

#include <openxr/openxr.h>

#include <string>
#include <vector>

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

    // Maps a session's current state/running-ness onto the generic
    // Frame::FrameStatus a FrameDriver::PrepareFrame() should return -
    // added in M9E.5 as the one piece of XR-specific lifecycle policy
    // XRFrameDriver needs that is genuinely separable as pure logic
    // (not a real API call). `sessionRunning` is intentionally a
    // separate parameter, same discipline as ShouldEndSession above.
    // Pure logic, directly unit-testable. See docs/ARCHITECTURE.md,
    // "New Lifecycle API (M9E.5)".
    [[nodiscard]] constexpr Frame::FrameStatus DetermineFrameStatus(XrSessionState currentState, bool sessionRunning)
    {
        if (ShouldStopMainLoop(currentState))
        {
            return Frame::FrameStatus::Stop;
        }
        if (!sessionRunning)
        {
            return Frame::FrameStatus::Idle;
        }
        return Frame::FrameStatus::Continue;
    }

    // What a caller must do in response to one observed XrSessionState
    // transition: nothing, call xrBeginSession, or call xrEndSession.
    enum class SessionLifecycleAction
    {
        None,
        Begin,
        End,
    };

    // Given every XrEventDataSessionStateChanged observed in one
    // xrPollEvent draining cycle, IN ORDER (see
    // SessionEventPollResult::sessionStateSequence in OpenXRSession.hpp
    // for why order matters), and whether the session was running
    // before this cycle began, returns the sequence of
    // BeginSession/EndSession calls a caller must make, in the same
    // order, to react correctly to every transition - not just the
    // last one.
    //
    // This is a real correctness fix, not defensive padding: a runtime
    // can legitimately deliver more than one session-state-changed
    // event within a single draining cycle (e.g. READY immediately
    // followed by STOPPING, if the application loses focus or the
    // window closes in the same instant the runtime was about to begin
    // a session). Reacting only to the last state observed that cycle
    // would silently skip the required xrBeginSession (or
    // xrEndSession) call for the intermediate one - leaving the
    // session's running flag and XrSessionState permanently
    // inconsistent with each other, with no way to recover without an
    // explicit fix like this one. Pure logic (each decision reuses
    // ShouldBeginSession/ShouldEndSession above and tracks the running
    // flag exactly as OpenXRSession itself would after each real
    // call), directly unit-testable without any real OpenXR call. See
    // docs/ARCHITECTURE.md, "Ordered Session-State Processing (M9E.5)".
    [[nodiscard]] inline std::vector<SessionLifecycleAction> DetermineSessionLifecycleActions(
        const std::vector<XrSessionState>& sessionStateSequence, bool initiallyRunning)
    {
        std::vector<SessionLifecycleAction> actions;
        actions.reserve(sessionStateSequence.size());

        bool running = initiallyRunning;
        for (const XrSessionState state : sessionStateSequence)
        {
            if (ShouldBeginSession(state))
            {
                actions.push_back(SessionLifecycleAction::Begin);
                running = true;
            }
            else if (ShouldEndSession(state, running))
            {
                actions.push_back(SessionLifecycleAction::End);
                running = false;
            }
            else
            {
                actions.push_back(SessionLifecycleAction::None);
            }
        }
        return actions;
    }
}
