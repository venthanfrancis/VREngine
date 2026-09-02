#pragma once

// M12: the one shared scene-content builder used by BOTH the desktop
// scene-render demo and the integrated XR demo, so the two presentation
// paths render genuinely the same Scene data (not two hand-duplicated
// copies of it). Pure Scene + Core - no Vulkan, no OpenXR - so it
// compiles and is usable regardless of which backend is enabled. See
// docs/ARCHITECTURE.md, "M12 - Renderable Scene Integration Foundation".
//
// Named PopulateDemoScene, not BuildDemoScene: vulkan_present_demo.cpp
// already has its own unrelated, differently-shaped file-local
// BuildDemoScene - reusing that name for a header-shared function with a
// different signature would be confusing to anyone grepping the
// codebase later.

#include "AREngine/Scene/EntityId.hpp"
#include "AREngine/Scene/MaterialId.hpp"
#include "AREngine/Scene/MeshId.hpp"
#include "AREngine/Scene/Scene.hpp"

namespace ARDemo
{
    // Minted once by each demo's own main() (the single source of truth
    // for which MeshId means "pyramid" vs. "floor"), then passed to both
    // PopulateDemoScene (to assign Renderables) and a MeshRegistry (to
    // resolve them to actual uploaded VulkanMesh objects) - the two
    // never need to independently agree on matching integers.
    //
    // M15: `pyramid` is now asset-backed (loaded from meshes/pyramid.obj
    // via AssetManager - see tests/PopulateDemoMeshes.hpp), replacing
    // the earlier procedural cube; `floor` stays fully procedural
    // (Rendering::CreateQuadMesh), proving the two coexist in one
    // MeshRegistry. See docs/ARCHITECTURE.md, "M15 - Asset-Backed Mesh
    // Loading Foundation".
    struct DemoMeshIds
    {
        AREngine::Scene::MeshId pyramid;
        AREngine::Scene::MeshId floor;
    };

    // M13: same minting pattern as DemoMeshIds, for the two demo
    // materials (each a distinctly-colored checkerboard texture).
    struct DemoMaterialIds
    {
        AREngine::Scene::MaterialId redChecker;
        AREngine::Scene::MaterialId blueChecker;
    };

    // The entities PopulateDemoScene created, so callers can mutate them
    // per frame (e.g. drive referenceCube's rotation/scale/tint, or
    // moveOffsetCube's position, from interaction state) without needing
    // their own separate bookkeeping.
    struct DemoSceneEntities
    {
        AREngine::Scene::EntityId floor;
        AREngine::Scene::EntityId referenceCube;
        AREngine::Scene::EntityId cubeA;
        AREngine::Scene::EntityId cubeB; // parented under referenceCube - the milestone's hierarchy proof
        AREngine::Scene::EntityId moveOffsetCube;
    };

    // Populates `scene` with the same 5-object layout M10.5/M10.6
    // originally hand-rolled as a std::vector<SceneObject>: a floor, a
    // reference object, two small objects, and one small "move offset"
    // object - except cubeB is now a real child of referenceCube (local
    // position chosen so its initial WORLD position is unchanged),
    // giving both demos a genuine, visually meaningful hierarchy proof
    // for free (it swings through world space as referenceCube's
    // existing rotation animation runs). The C++ identifiers below
    // (referenceCube, cubeA, cubeB, moveOffsetCube) are kept unchanged
    // from M12/M13 even though M15 gave them a pyramid mesh instead of
    // a cube - only their debug-name string literals were updated - to
    // avoid an unrelated cascading rename into every call site that
    // already references these fields (see tests/xr_demo.cpp).
    //
    // M13: material assignment across these same 5 entities proves all
    // three required combinations in one scene (see
    // docs/ARCHITECTURE.md, "M13 - Material & Render Resource Binding
    // Foundation"): referenceCube/cubeB/floor share `redChecker`
    // (multiple entities sharing one material, including across
    // different meshes - floor is a quad, the other two are pyramids);
    // cubeA shares referenceCube's MeshId but uses `blueChecker` instead
    // (same mesh, different material).
    //
    // M15: referenceCube/cubeA/cubeB/moveOffsetCube all share the SAME
    // asset-backed pyramid MeshId - simultaneously proving "one
    // GPU-uploaded asset-backed mesh, reused by multiple entities" and
    // (combined with the material split above) "different materials on
    // one imported mesh."
    [[nodiscard]] DemoSceneEntities PopulateDemoScene(
        AREngine::Scene::Scene& scene, const DemoMeshIds& meshIds, const DemoMaterialIds& materialIds);
}
