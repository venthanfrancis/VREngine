#pragma once

#include <cstdint>
#include <functional>

namespace AREngine::Physics
{
    // A lightweight, engine-owned physics-body identity. Deliberately
    // just a monotonically increasing integer with an "invalid"
    // sentinel (0) - the same minimal-handle philosophy already used
    // for Scene::EntityId/Rendering::MeshHandle. Minted by
    // PhysicsWorld's own internal counter, never a global one, and
    // never numerically equal to any backend body handle (PhysicsWorld
    // maintains its own internal translation table) - see
    // docs/ARCHITECTURE.md, "M18 - Physics Foundation" for why no
    // backend type ever crosses this boundary.
    struct PhysicsBodyId
    {
        std::uint64_t id = 0;

        [[nodiscard]] constexpr bool IsValid() const { return id != 0; }
    };

    constexpr bool operator==(const PhysicsBodyId& a, const PhysicsBodyId& b) { return a.id == b.id; }
}

template <>
struct std::hash<AREngine::Physics::PhysicsBodyId>
{
    std::size_t operator()(const AREngine::Physics::PhysicsBodyId& body) const noexcept
    {
        return std::hash<std::uint64_t>{}(body.id);
    }
};
