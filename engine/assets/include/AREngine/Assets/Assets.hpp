#pragma once

// AREngine::Assets
//
// Depends on Core and Platform (for file I/O). Loads and caches resources
// (meshes, textures, audio clips, scene files) from disk into runtime
// representations, exposed via reference-counted handles.
//
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - File-based resource loading
//   - Resource handles / caching

namespace AREngine::Assets
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once real Assets systems land.
    [[nodiscard]] const char* ModuleName();
}
