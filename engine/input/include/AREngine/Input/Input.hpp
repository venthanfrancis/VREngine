#pragma once

// AREngine::Input
//
// Depends on Core and Platform. Turns raw device input (keyboard/mouse
// now; XR controllers/hand tracking later) into named "actions" that
// gameplay code binds to, instead of binding to specific devices.
//
// Intended eventual contents (not implemented yet — see docs/ROADMAP.md):
//   - Action-mapping over raw Platform input

namespace AREngine::Input
{
    // Placeholder only, to prove this module compiles, links, and can be
    // consumed by other modules. Remove once real Input systems land.
    [[nodiscard]] const char* ModuleName();
}
