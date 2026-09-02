#pragma once

// M15: the one shared asset-backed mesh-setup helper used by BOTH the
// desktop scene-render demo and the integrated XR demo, mirroring
// PopulateDemoMaterials's own "one definition, two consumers" shape.
// Loads one real OBJ file through AssetManager, uploads/caches its GPU
// mesh through a MeshCache, and registers it into a MeshRegistry under
// a newly minted MeshId - the demo-local mesh-minting convention
// DemoMeshIds already established (M12), now backed by a real asset
// instead of purely-procedural geometry for this one mesh. See
// docs/ARCHITECTURE.md, "M15 - Asset-Backed Mesh Loading Foundation".
//
// The demo's OTHER mesh (the floor quad) stays fully procedural,
// created and registered directly in each demo's own main() exactly as
// before - this function deliberately does not also own that call, so
// "procedural and asset-backed meshes coexist in one MeshRegistry" stays
// visible at each demo's own call site, not hidden behind one helper
// that does everything.
//
// Parameters are kept flat (not bundled into a new "render context"
// struct) - no such bundling precedent exists anywhere in this
// codebase, matching PopulateDemoMaterials's own established shape.

#include "AREngine/Assets/Assets.hpp"
#include "AREngine/Scene/MeshId.hpp"

#include "MeshCache.hpp"
#include "MeshRegistry.hpp"

namespace ARDemo
{
    // Loads meshes/pyramid.obj through `assetManager` (relative to
    // whatever asset root it was constructed with - both demos point
    // this at tests/data/assets/), resolves/caches its GPU mesh through
    // `meshCache`, mints a new MeshId, and registers it into
    // `meshRegistry`. No file I/O, parsing, or GPU upload happens here
    // more than once - this is a setup-time call only, never called
    // from a frame loop.
    [[nodiscard]] AREngine::Scene::MeshId PopulateDemoMeshes(
        AREngine::Assets::AssetManager& assetManager,
        MeshCache& meshCache,
        MeshRegistry& meshRegistry,
        VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue);
}
