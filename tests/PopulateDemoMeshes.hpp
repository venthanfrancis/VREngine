#pragma once

// M15/M16: the one shared asset-backed mesh-setup helper used by BOTH
// the desktop scene-render demo and the integrated XR demo, mirroring
// PopulateDemoMaterials's own "one definition, two consumers" shape.
// Loads one real OBJ file through AssetManager and uploads/caches its
// GPU mesh through the engine-owned VulkanRenderResourceContext (M16 -
// previously a demo-local MeshCache + MeshRegistry pair). See
// docs/ARCHITECTURE.md, "M16 - Render Resource Context Foundation".
//
// The demo's OTHER mesh (the floor quad) stays fully procedural,
// created directly in each demo's own main() via
// context.CreateProceduralMesh(...) - this function deliberately does
// not also own that call, so "procedural and asset-backed meshes
// coexist in one context" stays visible at each demo's own call site,
// not hidden behind one helper that does everything.
//
// Parameters are kept flat (not bundled into a new "render context"
// struct) - no such bundling precedent exists anywhere in this
// codebase, matching PopulateDemoMaterials's own established shape.

#include "AREngine/Assets/Assets.hpp"
#include "AREngine/Scene/MeshId.hpp"

#include "vulkan/VulkanRenderResourceContext.hpp"

namespace ARDemo
{
    // Loads meshes/pyramid.obj through `assetManager` (relative to
    // whatever asset root it was constructed with - both demos point
    // this at tests/data/assets/), uploads/caches its GPU mesh through
    // `context`, and returns the resulting Scene::MeshId (converted
    // from the context's own backend-neutral Rendering::MeshHandle -
    // see docs/ARCHITECTURE.md for why these are kept distinct). No
    // file I/O, parsing, or GPU upload happens here more than once -
    // this is a setup-time call only, never called from a frame loop.
    [[nodiscard]] AREngine::Scene::MeshId PopulateDemoMeshes(
        AREngine::Assets::AssetManager& assetManager,
        AREngine::Rendering::Vulkan::VulkanRenderResourceContext& context);
}
