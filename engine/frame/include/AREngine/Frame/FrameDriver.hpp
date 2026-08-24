#pragma once

#include "AREngine/Frame/FrameContext.hpp"
#include "AREngine/Frame/ViewInfo.hpp"

#include <vector>

namespace AREngine::Frame
{
    // High-level frame lifecycle, independent of where the frame
    // actually comes from. Runtime drives its main loop against this
    // interface; DesktopFrameDriver and XRFrameDriver both implement
    // it. See docs/ARCHITECTURE.md, "New Lifecycle API (M9E.5)".
    //
    // Redesigned in M9E.5 from real OpenXR evidence gathered in M9E
    // (the original WaitForNextFrame/GetViews/SubmitFrame shape,
    // designed in M1 before any real backend existed, did not fit):
    // no representation of shouldRender, no explicit begin-frame seam,
    // and a single SubmitFrame() too coarse for OpenXR's real
    // acquire/wait/render/release/end sequence. See
    // docs/ARCHITECTURE.md, "Why The Old FrameDriver Was Insufficient
    // (M9E.5)" for the full evidence.
    //
    // Deliberately excluded from this interface: desktop presentation
    // (e.g. swapchain present) and GPU render-target acquisition (e.g.
    // OpenXR's xrAcquireSwapchainImage/xrWaitSwapchainImage/
    // xrReleaseSwapchainImage) — both are resource-lifecycle concerns
    // that differ fundamentally between backends and belong to
    // whichever renderer/XR-integration layer actually owns the GPU
    // resources, not to this generic timing/pacing interface. See
    // docs/ARCHITECTURE.md, "Render-Target Acquisition Ownership
    // (M9E.5)".
    //
    // Contract: BeginFrame()/GetViews()/EndFrame() must only be called
    // after PrepareFrame() returns FrameStatus::Continue — a
    // FrameStatus::Idle result means no frame lifecycle call should
    // happen at all this tick, and FrameStatus::Stop means the caller
    // should end its own loop. GetViews() is only valid after
    // BeginFrame(). EndFrame() must be called exactly once per
    // BeginFrame(), regardless of FrameTiming::shouldRender — some
    // backends require a matching Begin/End pair either way (OpenXR's
    // xrBeginFrame/xrEndFrame).
    class FrameDriver
    {
    public:
        virtual ~FrameDriver() = default;

        // Blocks (if needed) until it is time to prepare the next
        // frame, and returns that frame's timing/shouldRender plus
        // whether a frame lifecycle should happen at all this tick.
        virtual FrameContext PrepareFrame() = 0;

        // Marks the start of this frame's render work. Only valid after
        // PrepareFrame() returns FrameStatus::Continue. Pairs with
        // EndFrame().
        virtual void BeginFrame() = 0;

        // Returns the view(s) to render for the frame most recently
        // begun. May be empty, one, or many.
        virtual std::vector<ViewInfo> GetViews() = 0;

        // Marks the frame as complete. What this actually does depends
        // on the implementation (present a swapchain image, submit to
        // an XR runtime, ...) — not this interface's concern.
        virtual void EndFrame() = 0;
    };
}
