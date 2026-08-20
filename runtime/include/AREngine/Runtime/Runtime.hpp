#pragma once

// AREngine::Runtime
//
// The glue layer: owns the main loop and module init/update/shutdown
// order. Depends on every engine module. This is what an Editor or a
// game (Sandbox, or eventually a third-party developer's project) links
// against.
//
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - Application lifecycle (init / update / shutdown)
//   - Main loop, written against Frame's FrameDriver interface — backed
//     by a DesktopFrameDriver today, an XRFrameDriver later, with no
//     change to the loop itself. See docs/ARCHITECTURE.md.

namespace AREngine::Runtime
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by Editor/Sandbox. Remove once the real Runtime lands.
    [[nodiscard]] const char* ModuleName();
}
