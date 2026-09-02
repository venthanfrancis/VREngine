#pragma once

// M15: the ONLY point in this codebase that knows tinyobjloader exists.
// Deliberately kept private (engine/assets/src/, not include/) - not
// part of AssetManager's public API, no tinyobj type ever crosses out
// of MeshDecode.cpp. See docs/ARCHITECTURE.md, "M15 - Asset-Backed Mesh
// Loading Foundation".

#include "AREngine/Assets/MeshAsset.hpp"

#include <filesystem>
#include <optional>

namespace AREngine::Assets
{
    // Decodes the OBJ file at `resolvedPath` (already resolved/validated
    // by AssetManager::ResolvePath) into a MeshAsset: unit-triangle
    // positions/UVs, deduplicated into a single unified vertex+index
    // list. Returns std::nullopt on any failure (missing/unreadable
    // file, parse error, zero faces, or a face referencing an
    // out-of-range vertex/texcoord index) - never crashes. The returned
    // MeshAsset has `id`/`path` left default-constructed; the caller
    // (AssetManager) fills those in.
    //
    // Deliberate exception to ImageDecode.cpp's "always decode from an
    // in-memory buffer" invariant: this takes a path, not bytes, and
    // reads the file itself via tinyobjloader's own file-based LoadObj.
    // tinyobjloader's memory/callback-based alternative
    // (LoadObjWithCallback) uses a different, lower-level parser with
    // un-normalized, differently-sentineled indices - reimplementing
    // that normalization correctly would be strictly riskier than
    // accepting this one narrow divergence from the memory-decode
    // pattern. Only OBJ's v/vt/f records are honored: no vn (normals),
    // no vertex color, and no .mtl material import - materials parsed
    // by tinyobjloader (if any mtllib line is even present) are
    // discarded entirely, matching this milestone's explicit scope.
    [[nodiscard]] std::optional<MeshAsset> DecodeMeshOBJ(const std::filesystem::path& resolvedPath);
}
