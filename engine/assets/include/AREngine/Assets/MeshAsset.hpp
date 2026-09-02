#pragma once

#include "AREngine/Assets/AssetId.hpp"
#include "AREngine/Core/Math/Vec2.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace AREngine::Assets
{
    // One CPU-side mesh vertex, loaded and unified by
    // AssetManager::LoadMesh from a real OBJ file on disk (M15).
    // Deliberately its own independent struct, not Rendering::MeshVertex
    // (same position/color/uv shape, coincidentally) - Assets currently
    // depends only on Core, and reusing a Rendering type here would
    // force a new Assets -> Rendering dependency purely to share a
    // vertex layout. See docs/ARCHITECTURE.md, "M15 - Asset-Backed Mesh
    // Loading Foundation".
    struct MeshVertexData
    {
        Core::Math::Vec3 position;
        Core::Math::Vec3 color; // always white (1,1,1) - OBJ vertex color is not imported, matching ProceduralMesh's own convention
        Core::Math::Vec2 uv;
    };

    // A decoded mesh, loaded from a real OBJ file on disk (M15), with
    // OBJ's separate per-attribute (position/uv) indexing already
    // unified into single MeshVertexData entries plus one shared index
    // list - ready to hand to a GPU upload path unchanged in shape. No
    // MaterialId here: .mtl material files are never imported (see
    // MeshDecode.cpp) - matching TextureAsset/TextAsset/BinaryAsset's
    // own precedent of independent, non-base-classed content structs.
    struct MeshAsset
    {
        AssetId id;
        std::filesystem::path path; // the relative path it was loaded from
        std::vector<MeshVertexData> vertices;
        std::vector<std::uint32_t> indices;
    };
}
