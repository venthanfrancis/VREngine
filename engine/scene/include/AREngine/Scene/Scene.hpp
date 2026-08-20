#pragma once

// AREngine::Scene
//
// Depends only on Core (and later, optionally, Assets). Holds the game
// world's data: entities, transforms (position/rotation/scale), and
// parent/child hierarchy. Knows nothing about graphics APIs or XR
// devices — it hands renderable data to Rendering, it does not call it.
//
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - Entity/transform representation
//   - Scene hierarchy
//
// All positions/transforms in this module follow the world conventions
// documented in docs/WORLD_CONVENTIONS.md (meters, right-handed, +Y up,
// -Z forward, +X right).

namespace AREngine::Scene
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once real Scene systems land.
    [[nodiscard]] const char* ModuleName();
}
