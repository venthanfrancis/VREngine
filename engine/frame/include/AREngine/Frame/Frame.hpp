#pragma once

// AREngine::Frame
//
// Depends only on Core. Kept as its own module — separate from Core —
// specifically so Core stays a minimal foundation with no rendering/XR
// concepts, while Runtime, DesktopFrameDriver (later), and XRFrameDriver
// (later) can all depend on Frame without Core needing to know about any
// of them. See docs/ARCHITECTURE.md, "FrameDriver Abstraction".
//
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - FrameDriver interface (wait for next frame / get view(s) / submit)
//   - FrameTiming (predicted display time, delta time)
//   - ViewInfo (pose + projection; 1 view desktop, 2 views stereo XR)

namespace AREngine::Frame
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once the real Frame types land.
    [[nodiscard]] const char* ModuleName();
}
