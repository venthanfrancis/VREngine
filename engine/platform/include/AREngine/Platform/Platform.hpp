#pragma once

// AREngine::Platform
//
// Depends only on Core. The only module allowed to call OS-specific APIs.
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - Window creation
//   - Raw input polling (keyboard/mouse now; more later)
//   - Timing / clock
//   - File I/O
//   - Dynamic library loading
//
// Windows is the only backend for now; Android/Linux backends will live
// alongside it under src/ without changing this public interface.

namespace AREngine::Platform
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once real Platform systems land.
    [[nodiscard]] const char* ModuleName();
}
