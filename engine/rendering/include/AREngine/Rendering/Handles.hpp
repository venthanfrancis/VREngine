#pragma once

#include <cstdint>

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

    constexpr bool operator==(const BufferHandle& a, const BufferHandle& b) { return a.id == b.id; }
    constexpr bool operator==(const TextureHandle& a, const TextureHandle& b) { return a.id == b.id; }
}
