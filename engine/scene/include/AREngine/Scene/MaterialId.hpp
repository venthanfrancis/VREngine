#pragma once

#include <cstdint>
#include <functional>

namespace AREngine::Scene
{
    // An opaque, backend-neutral reference to a material/render-
    // appearance resource - exact mirror of MeshId's shape and
    // philosophy (see MeshId.hpp). Scene stores and compares
    // MaterialIds without knowing what a material actually is (no
    // Vulkan/Rendering type is visible here). Resolving a MaterialId to
    // actual GPU resources (a texture + descriptor set, sharing one
    // engine-wide pipeline) is entirely the caller's responsibility
    // (e.g. a small demo-owned MaterialRegistry) - see
    // docs/ARCHITECTURE.md, "M13 - Material & Render Resource Binding
    // Foundation".
    struct MaterialId
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    constexpr bool operator==(const MaterialId& a, const MaterialId& b) { return a.id == b.id; }
}

template <>
struct std::hash<AREngine::Scene::MaterialId>
{
    std::size_t operator()(const AREngine::Scene::MaterialId& material) const noexcept
    {
        return std::hash<std::uint64_t>{}(material.id);
    }
};
