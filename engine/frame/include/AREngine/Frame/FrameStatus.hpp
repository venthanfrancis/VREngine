#pragma once

namespace AREngine::Frame
{
    // Whether a frame lifecycle should happen at all this tick, and if
    // so, whether the caller should end its own loop instead. Added in
    // M9E.5, directly from real OpenXR evidence gathered in M9E — see
    // docs/ARCHITECTURE.md, "FrameStatus (M9E.5)".
    //
    // Deliberately a different axis from FrameTiming::shouldRender:
    // FrameStatus answers "is a Begin/End pair even legal to call this
    // tick," while shouldRender answers "given a legal Begin/End pair,
    // should its content actually be rendered." OpenXR's real frame
    // lifecycle needs both — xrBeginFrame/xrEndFrame require the
    // session to be running (confirmed the hard way in M9E: calling
    // xrWaitFrame while not running fails with
    // XR_ERROR_SESSION_NOT_RUNNING), which is a completely different
    // situation from a running session whose own xrWaitFrame reports
    // shouldRender=false (which M9E observed on the large majority of
    // its own frames, and where xrBeginFrame/xrEndFrame remain fully
    // legal to call).
    enum class FrameStatus
    {
        // A full frame lifecycle should happen: BeginFrame(), then
        // (only if the FrameTiming's shouldRender is true) the caller's
        // own render work, then EndFrame() — regardless of shouldRender.
        Continue,

        // No frame lifecycle call (BeginFrame/GetViews/EndFrame) should
        // happen at all this tick — not even an "empty" one. The caller
        // should simply loop again. Desktop never returns this; XR
        // returns it while the session exists but is not currently
        // running (not yet begun, or between STOPPING and EXITING) —
        // xrBeginFrame/xrEndFrame are not legal to call in that window
        // at all, not merely "skip the content."
        Idle,

        // The frame source can no longer produce frames — the caller
        // should end its own loop. Desktop never returns this; XR
        // returns it once the session has reached a terminal state
        // (EXITING/LOSS_PENDING).
        Stop,
    };
}
