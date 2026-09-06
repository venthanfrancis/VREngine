#pragma once

// M17: the Scene <-> Rendering integration/conversion layer, replacing
// the now-deleted tests/RenderDrawPlanning.hpp/.cpp (whose PlannedDraw/
// BuildDrawPlan became obsolete once SubmitRenderItems took over per-
// view draw execution - nothing else in the codebase consumed
// PlannedDraw, confirmed by repo-wide search before deleting it).
//
// Pure logic only - no Vulkan, no OpenXR, no Rendering::Vulkan type
// anywhere in this file - so it compiles and is fully testable with
// ARENGINE_ENABLE_VULKAN OFF, and is shared unchanged between the
// desktop scene demo (1 view) and the integrated XR demo (N eye
// views). See docs/ARCHITECTURE.md, "M17 - Scene Render Submission
// Foundation".
//
// This is the ONLY place Scene::MeshId/MaterialId and
// Rendering::MeshHandle/MaterialHandle are ever both mentioned - a
// deliberate architectural seam (see AGENTS.md): Rendering must never
// depend on Scene, so this conversion lives here, at the tests/-leaf
// integration layer that already depends on both modules, not inside
// engine/rendering itself.

#include "AREngine/Rendering/RenderItem.hpp"
#include "AREngine/Scene/RenderableInstance.hpp"

#include <span>
#include <vector>

namespace ARDemo
{
    // Converts each Scene::RenderableInstance into a backend-neutral
    // Rendering::RenderItem: Scene::MeshId/MaterialId are rewrapped
    // into Rendering::MeshHandle/MaterialHandle via the same trivial
    // one-line `.id` reinterpretation already used (in the opposite
    // direction) by PopulateDemoMeshes.cpp/PopulateDemoMaterials.cpp -
    // both are structurally identical {uint64_t} wrappers, so this is
    // a value reinterpretation, not a lookup. `worldTransform`/`tint`
    // carry straight through unchanged. Does not require a view/
    // projection of any kind - RenderItem is view-independent by
    // design (see RenderItem.hpp).
    [[nodiscard]] std::vector<AREngine::Rendering::RenderItem> BuildRenderItems(
        std::span<const AREngine::Scene::RenderableInstance> renderables);
}
