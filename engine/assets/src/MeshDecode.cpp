#include "MeshDecode.hpp"

// The one translation unit in this codebase that defines
// TINYOBJLOADER_IMPLEMENTATION - tiny_obj_loader.h must never be
// #included with this macro defined anywhere else, or the
// implementation would be duplicated across translation units. See
// third_party/tinyobjloader/README.md for provenance (commit
// 62ff207968f3dc14a64a1e2378dce67b760e7c4a).
//
// Wrapped in a warning-suppression pair so tinyobjloader's own internal
// warnings never leak into this project's /W4 build - this fences
// THIRD-PARTY code only, it does not weaken AREngine's own warning
// level anywhere else in the codebase. Same pattern as ImageDecode.cpp
// (stb_image.h).
//
// MSVC/C++20-specific workaround, applied here (not to the vendored
// file, which stays verbatim/unmodified) because it must take effect
// BEFORE tiny_obj_loader.h is included: the pinned commit's embedded
// fast_float snapshot guards a `constexpr` code path behind the
// standard feature-test macro __cpp_lib_constexpr_algorithms, which
// MSVC's STL headers already define (via <algorithm>/<version>,
// already transitively included by this point) even though the
// specific function it gates (loop_parse_if_eight_digits) is not
// actually constexpr-evaluable under MSVC's checker - a genuine
// upstream fast_float/MSVC incompatibility (MSVC error C3615), not an
// AREngine defect. Undefining the macro here steers tiny_obj_loader.h's
// own existing `#if defined(__cpp_lib_constexpr_algorithms) && ...`
// check (see its FASTFLOAT_CONSTEXPR20 definition) onto its plain,
// non-constexpr fallback path instead - purely a compile-time code
// path choice inside fast_float's float-parsing fast path, with no
// effect on parsed values.
#if defined(_MSC_VER)
    #include <version>
    #undef __cpp_lib_constexpr_algorithms
#endif

#if defined(_MSC_VER)
    #pragma warning(push, 0)
#endif
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include "AREngine/Core/Log.hpp"

#include <cstdint>
#include <format>
#include <map>
#include <utility>

namespace AREngine::Assets
{
    std::optional<MeshAsset> DecodeMeshOBJ(const std::filesystem::path& resolvedPath)
    {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials; // discarded entirely - no .mtl import, see this file's header comment
        std::string warn;
        std::string err;

        // mtl_basedir left at its default (NULL): this milestone's
        // fixtures never contain an `mtllib` line, so no .mtl load is
        // ever attempted regardless.
        const bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, resolvedPath.string().c_str());

        // Logged even on success, per this milestone's requirement -
        // ImageDecode.cpp's stb_image path has no equivalent "succeeded
        // with caveats" signal to preserve, but tinyobjloader's does.
        if (!warn.empty())
        {
            AR_LOG_WARNING(std::format("tinyobjloader warning while loading {}: {}", resolvedPath.string(), warn));
        }

        if (!ok || !err.empty())
        {
            return std::nullopt;
        }

        const int vertexCount = static_cast<int>(attrib.vertices.size() / 3);
        const int texcoordCount = static_cast<int>(attrib.texcoords.size() / 2);

        MeshAsset asset;

        // (vertex_index, texcoord_index) -> already-unified MeshVertexData
        // index. Concretely proves OBJ's separate per-attribute indexing
        // collapses into single MeshVertex entries, matching tinyobjloader's
        // own fixIndex-normalized, 0-based-with-(-1)-sentinel convention.
        std::map<std::pair<int, int>, std::uint32_t> unifiedIndex;

        for (const tinyobj::shape_t& shape : shapes)
        {
            for (const tinyobj::index_t& index : shape.mesh.indices)
            {
                // Defensive bounds checks - a face referencing a vertex or
                // texcoord index outside the parsed attribute arrays is
                // rejected, not trusted to the parser alone. vertex_index
                // must always be present (OBJ faces require positions);
                // texcoord_index may legitimately be -1 (tinyobjloader's
                // unambiguous "no vt for this corner" sentinel).
                if (index.vertex_index < 0 || index.vertex_index >= vertexCount)
                {
                    return std::nullopt;
                }
                if (index.texcoord_index >= 0 && index.texcoord_index >= texcoordCount)
                {
                    return std::nullopt;
                }

                const std::pair<int, int> key{index.vertex_index, index.texcoord_index};
                const auto existing = unifiedIndex.find(key);
                if (existing != unifiedIndex.end())
                {
                    asset.indices.push_back(existing->second);
                    continue;
                }

                MeshVertexData vertex;
                vertex.position = Core::Math::Vec3(
                    attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 0],
                    attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 1],
                    attrib.vertices[3 * static_cast<std::size_t>(index.vertex_index) + 2]);
                vertex.color = Core::Math::Vec3(1.0f, 1.0f, 1.0f); // no OBJ vertex-color import - see MeshVertexData
                vertex.uv = (index.texcoord_index >= 0)
                    ? Core::Math::Vec2(
                          attrib.texcoords[2 * static_cast<std::size_t>(index.texcoord_index) + 0],
                          attrib.texcoords[2 * static_cast<std::size_t>(index.texcoord_index) + 1])
                    : Core::Math::Vec2(0.0f, 0.0f);

                const std::uint32_t newIndex = static_cast<std::uint32_t>(asset.vertices.size());
                asset.vertices.push_back(vertex);
                asset.indices.push_back(newIndex);
                unifiedIndex.emplace(key, newIndex);
            }
        }

        if (asset.vertices.empty() || asset.indices.empty())
        {
            return std::nullopt; // syntactically valid but empty OBJ (no faces) - reject, same failure path as a corrupt image
        }

        return asset;
    }
}
