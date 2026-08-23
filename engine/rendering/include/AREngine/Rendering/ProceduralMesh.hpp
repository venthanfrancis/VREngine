#pragma once

#include "AREngine/Rendering/MeshData.hpp"

namespace AREngine::Rendering
{
    // Small, backend-independent procedural geometry generators for
    // testing and manual demos — no downloaded/loaded models, no
    // MeshAsset, no glTF/OBJ. See docs/ARCHITECTURE.md, "Procedural
    // Test Meshes (M8H)".
    //
    // Every vertex color is white (1,1,1) — these are neutral,
    // reusable primitives, not a specific demo's vertex-color-gradient
    // proof (that already exists, unrelated to these, in M8D's tests).
    // A texture (any texture) is expected to supply the actual visual
    // detail; that's exactly what these are for.

    // A 1x1 meter quad in the local XY plane (z = 0), centered on the
    // origin, facing local +Z. Matches the vertex/index/UV layout the
    // Vulkan demo's hard-coded quad has used since M8D — this is that
    // same shape, now reusable and backend-independent.
    [[nodiscard]] MeshData CreateQuadMesh();

    // A 1x1x1 meter cube centered on the origin (x/y/z each spanning
    // -0.5..+0.5 — see docs/WORLD_CONVENTIONS.md for why this is "1
    // meter" and not an arbitrary unit). 24 vertices (4 per face, not
    // 8) — deliberately not vertex-count-optimized: sharing corner
    // vertices across faces would force one shared UV per corner,
    // which can't correctly map a texture onto all 3 faces meeting at
    // that corner independently. 36 indices (6 faces x 2 triangles x 3
    // indices). Every face is wound so its 4 corners read
    // counter-clockwise when viewed from outside the cube along its
    // outward normal — matching CreateQuadMesh's own +Z-face winding
    // exactly, and therefore matching the Vulkan pipeline's existing
    // VK_FRONT_FACE_CLOCKWISE front-face convention once the Vulkan Y
    // flip is applied (see docs/ARCHITECTURE.md, "Back-Face Culling
    // (M8H)").
    [[nodiscard]] MeshData CreateCubeMesh();
}
