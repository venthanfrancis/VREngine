#pragma once

#include <cstdint>
#include <functional>

namespace AREngine::Assets
{
    // A lightweight, engine-owned asset identity. Deliberately just a
    // monotonically increasing integer with an "invalid" sentinel (0) —
    // the same minimal-handle philosophy as Rendering::BufferHandle and
    // Scene::EntityId.
    //
    // AssetId identity is only meaningful within the AssetManager
    // instance that issued it. Two different AssetManager instances
    // both start counting from 1, so an AssetId from one manager must
    // never be compared against or looked up in another — there is no
    // global uniqueness guarantee, the same per-instance semantics
    // Scene::EntityId already has relative to a single Scene. See
    // docs/ARCHITECTURE.md, "AssetId Semantics".
    struct AssetId
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    constexpr bool operator==(const AssetId& a, const AssetId& b) { return a.id == b.id; }
}

template <>
struct std::hash<AREngine::Assets::AssetId>
{
    std::size_t operator()(const AREngine::Assets::AssetId& asset) const noexcept
    {
        return std::hash<std::uint64_t>{}(asset.id);
    }
};
