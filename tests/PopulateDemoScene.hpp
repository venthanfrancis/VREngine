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
#include "AREngine/Scene/MeshId.hpp"
#include "AREngine/Scene/Scene.hpp"

namespace ARDemo
{
    // Minted once by each demo's own main() (the single source of truth
    // for which MeshId means "cube" vs. "floor"), then passed to both
    // PopulateDemoScene (to assign Renderables) and a MeshRegistry (to
    // resolve them to actual uploaded VulkanMesh objects) - the two
    // never need to independently agree on matching integers.
    struct DemoMeshIds
    {
        AREngine::Scene::MeshId cube;
        AREngine::Scene::MeshId floor;
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
    // reference cube, two small cubes, and one small "move offset" cube
    // - except cubeB is now a real child of referenceCube (local
    // position chosen so its initial WORLD position is unchanged),
    // giving both demos a genuine, visually meaningful hierarchy proof
    // for free (it swings through world space as referenceCube's
    // existing rotation animation runs).
    [[nodiscard]] DemoSceneEntities PopulateDemoScene(AREngine::Scene::Scene& scene, const DemoMeshIds& meshIds);
}
