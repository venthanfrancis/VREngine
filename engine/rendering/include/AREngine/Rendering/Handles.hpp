#pragma once

#include <cstdint>
#include <functional>

namespace AREngine::Rendering
{
    // Lightweight, engine-owned resource identities. Deliberately just
    // an integer with an "invalid" sentinel (0) — no generational reuse
    // checks, no exposed backend memory objects. A backend is free to
    // map these to whatever it needs internally (NullRenderDevice maps
    // them to std::unordered_map keys; a future Vulkan backend would
    // map them to VkBuffer/VkImage handles it owns privately). See
    // docs/ARCHITECTURE.md, "Resource Handles".
    struct BufferHandle
    {
        std::uint32_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    struct TextureHandle
    {
        std::uint32_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    // M16: minted by VulkanRenderResourceContext (engine/rendering/src/vulkan/,
    // private) and returned to callers as a backend-neutral, Scene-free
    // identity for a GPU mesh/material resource. Deliberately NOT
    // Scene::MeshId/MaterialId itself - Rendering must never depend on
    // Scene (see docs/ARCHITECTURE.md, "M16 - Render Resource Context
    // Foundation"). The only place a MeshHandle/MaterialHandle and a
    // Scene::MeshId/MaterialId are ever both mentioned is demo-glue code
    // that already depends on both modules - a one-line wrap
    // (Scene::MeshId{handle.id}) converts between them, since both are
    // structurally identical {uint64_t} wrappers.
    struct MeshHandle
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    struct MaterialHandle
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    constexpr bool operator==(const BufferHandle& a, const BufferHandle& b) { return a.id == b.id; }
    constexpr bool operator==(const TextureHandle& a, const TextureHandle& b) { return a.id == b.id; }
    constexpr bool operator==(const MeshHandle& a, const MeshHandle& b) { return a.id == b.id; }
    constexpr bool operator==(const MaterialHandle& a, const MaterialHandle& b) { return a.id == b.id; }
}

// MeshHandle/MaterialHandle are used as unordered_map keys inside
// VulkanRenderResourceContext - BufferHandle/TextureHandle don't need
// this today (nothing keys a map by them), so they're deliberately not
// given a matching specialization here.
template <>
struct std::hash<AREngine::Rendering::MeshHandle>
{
    std::size_t operator()(const AREngine::Rendering::MeshHandle& handle) const noexcept
    {
        return std::hash<std::uint64_t>{}(handle.id);
    }
};

template <>
struct std::hash<AREngine::Rendering::MaterialHandle>
{
    std::size_t operator()(const AREngine::Rendering::MaterialHandle& handle) const noexcept
    {
        return std::hash<std::uint64_t>{}(handle.id);
    }
};
