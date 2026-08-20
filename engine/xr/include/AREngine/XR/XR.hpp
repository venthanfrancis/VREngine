#pragma once

// AREngine::XR
//
// Depends on Core, Platform, and Frame. Wraps XR runtime concepts (head
// pose, hand/controller tracking, spatial anchors, passthrough) behind
// generic engine types, so gameplay code never calls an XR API directly.
//
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - OpenXR integration (M9)
//   - XRFrameDriver (implements the Frame module's FrameDriver interface)

namespace AREngine::XR
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once real XR systems land.
    [[nodiscard]] const char* ModuleName();
}
