#pragma once

#include "AREngine/Frame/FrameTiming.hpp"
#include "AREngine/Frame/ViewInfo.hpp"

#include <vector>

namespace AREngine::Frame
{
    // High-level frame lifecycle, independent of where the frame
    // actually comes from. Runtime drives its main loop against this
    // interface; DesktopFrameDriver and, later, XRFrameDriver both
    // implement it. See docs/ARCHITECTURE.md, "The FrameDriver
    // Abstraction".
    //
    // Deliberately excluded from this interface: desktop presentation
    // (e.g. swapchain present) and OpenXR-specific submission calls —
    // those have different lifecycle requirements per backend and are
    // not designed here yet. See docs/ARCHITECTURE.md, "RHI
    // Presentation".
    class FrameDriver
    {
    public:
        virtual ~FrameDriver() = default;

        // Blocks (if needed) until it is time to start preparing the
        // next frame, and returns that frame's timing information.
        virtual FrameTiming WaitForNextFrame() = 0;

        // Returns the view(s) to render for the frame most recently
        // returned by WaitForNextFrame(). May be empty, one, or many.
        virtual std::vector<ViewInfo> GetViews() = 0;

        // Marks the frame as complete. What this actually does depends
        // on the implementation (present a swapchain image, submit to
        // an XR runtime, ...) — not this interface's concern.
        virtual void SubmitFrame() = 0;
    };
}
