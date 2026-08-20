#pragma once

// AREngine::Core
//
// The foundational module. Depends on nothing else in the engine.
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - Math (vectors, matrices, quaternions)
//   - Logging
//   - Assertions
//   - Event system
//
// Use std:: containers (vector, unordered_map, string, ...) directly
// throughout the engine. Do not introduce custom container types without
// a measured (profiled) reason.

namespace AREngine::Core
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once real Core systems land.
    [[nodiscard]] const char* ModuleName();
}
