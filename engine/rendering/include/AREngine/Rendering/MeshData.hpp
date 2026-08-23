#pragma once

#include "AREngine/Core/Math/Vec2.hpp"
#include "AREngine/Core/Math/Vec3.hpp"

#include <cstdint>
#include <vector>

namespace AREngine::Rendering
{
    // A single vertex's worth of CPU-side mesh geometry: a position, a
    // per-vertex color, and a texture coordinate. Deliberately the same
    // shape the Vulkan backend's private per-vertex data has needed
    // since M8D/M8E/M8F (position/color/uv) — that repetition, now seen
    // across both the old hard-coded demo geometry and this generic
    // type, is the actual evidence this milestone needed to promote it
    // out of Vulkan's private implementation. See
    // docs/ARCHITECTURE.md, "CPU Mesh Data Placement (M8H)".
    //
    // No Vulkan types, no VkFormat, no byte offsets — this is plain
    // engine-owned data. A backend is free to read it however it needs
    // to (the Vulkan backend does so via offsetof — see
    // src/vulkan/VulkanVertex.hpp).
    struct MeshVertex
    {
        Core::Math::Vec3 position;
        Core::Math::Vec3 color;
        Core::Math::Vec2 uv;
    };

    // A complete piece of CPU-side, backend-independent mesh geometry:
    // one indexed vertex list. Deliberately minimal — no submeshes, no
    // material references, no AssetId (this is runtime-generated or
    // in-memory geometry, not a loaded asset yet — see
    // docs/ARCHITECTURE.md, "CPU Mesh Data Placement (M8H)"), no
    // Vulkan/GPU handles of any kind. Uploading this to the GPU is a
    // separate, explicit step (see src/vulkan/VulkanMesh.hpp's
    // CreateVulkanMesh) — MeshData itself never owns GPU resources.
    struct MeshData
    {
        std::vector<MeshVertex> vertices;
        std::vector<std::uint32_t> indices;

        // Lightweight, explicit validation — not a general mesh
        // validator (no degenerate-triangle checks, no duplicate-vertex
        // detection, no winding checks): just the minimum a GPU upload
        // path must be able to rely on before it touches this data.
        // See docs/ARCHITECTURE.md, "Mesh Validation (M8H)".
        [[nodiscard]] bool IsValid() const
        {
            if (vertices.empty() || indices.empty())
            {
                return false;
            }
            for (const std::uint32_t index : indices)
            {
                if (index >= vertices.size())
                {
                    return false;
                }
            }
            return true;
        }
    };
}
