#pragma once

#include <cstdint>
#include <functional>

namespace AREngine::Scene
{
    // A lightweight, engine-owned entity identity. Deliberately just a
    // monotonically increasing integer with an "invalid" sentinel (0) —
    // the same minimal-handle philosophy already used for
    // Rendering::BufferHandle/TextureHandle.
    //
    // No generation counter: Scene's next-id counter only ever
    // increases, so an id is never reused after the entity that held it
    // is destroyed. A stale EntityId therefore just becomes "not found"
    // rather than silently referring to a different, newer entity. This
    // is weaker than a full generational-index scheme (which could also
    // distinguish "this exact id was reused by an unrelated entity" —
    // impossible here, since reuse never happens), but that guarantee
    // isn't needed for M5. See docs/ARCHITECTURE.md, "EntityId
    // Strategy".
    struct EntityId
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    constexpr bool operator==(const EntityId& a, const EntityId& b) { return a.id == b.id; }
}

template <>
struct std::hash<AREngine::Scene::EntityId>
{
    std::size_t operator()(const AREngine::Scene::EntityId& entity) const noexcept
    {
        return std::hash<std::uint64_t>{}(entity.id);
    }
};
