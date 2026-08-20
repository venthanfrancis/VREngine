#pragma once

// AREngine::Rendering
//
// The RHI (Render Hardware Interface): an abstraction over GPU rendering
// operations, kept intentionally small. No graphics API is implemented
// yet (Vulkan lands at M8 — see docs/ROADMAP.md).
//
// Intended eventual contents, scoped to GPU operations only:
//   - Create buffer / texture / pipeline
//   - Submit draw calls
//
// Explicitly NOT this module's job: frame lifecycle, presentation, or
// frame submission timing. Desktop Vulkan presentation and OpenXR frame
// submission have different lifecycle requirements, so that concern is
// kept separate (owned by Frame + Runtime) until real requirements from
// both backends are known. See docs/ARCHITECTURE.md, "RHI Presentation".

namespace AREngine::Rendering
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once the real RHI lands.
    [[nodiscard]] const char* ModuleName();
}
