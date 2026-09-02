#pragma once

#include <cstdint>
#include <functional>

namespace AREngine::Scene
{
    // An opaque, backend-neutral reference to a mesh resource, in the
    // same minimal-handle shape as EntityId/Rendering::BufferHandle —
    // Scene stores and compares MeshIds without knowing what a mesh
    // actually is (no Vulkan/Rendering type is visible here). Resolving
    // a MeshId to an actual GPU mesh is entirely the caller's
    // responsibility (e.g. a small demo-owned registry) — see
    // docs/ARCHITECTURE.md, "M12 - Renderable Scene Integration
    // Foundation".
    struct MeshId
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    constexpr bool operator==(const MeshId& a, const MeshId& b) { return a.id == b.id; }
}

template <>
struct std::hash<AREngine::Scene::MeshId>
{
    std::size_t operator()(const AREngine::Scene::MeshId& mesh) const noexcept
    {
        return std::hash<std::uint64_t>{}(mesh.id);
    }
};
