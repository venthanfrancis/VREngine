#pragma once

#include "AREngine/Frame/FrameStatus.hpp"
#include "AREngine/Frame/FrameTiming.hpp"

namespace AREngine::Frame
{
    // What FrameDriver::PrepareFrame() returns: this frame's timing
    // (including whether it should be rendered) plus whether a frame
    // lifecycle should happen at all this tick. Bundled together
    // (rather than two separate return values) because they are always
    // produced and consumed together — this mirrors how OpenXR's own
    // XrFrameState bundles predictedDisplayTime/predictedDisplayPeriod/
    // shouldRender into one struct. Added in M9E.5 — see
    // docs/ARCHITECTURE.md, "New Lifecycle API (M9E.5)".
    struct FrameContext
    {
        FrameTiming timing;
        FrameStatus status = FrameStatus::Continue;
    };
}
