#pragma once

#include "AREngine/Core/Math/Vec3.hpp"

#include <variant>

namespace AREngine::Physics
{
    // M18's narrow collider scope: box and sphere only - no capsule
    // (optional per the milestone spec, skipped entirely to keep scope
    // narrow), no convex hull, no triangle mesh, no compound shapes, no
    // heightfields. See docs/ARCHITECTURE.md, "M18 - Physics
    // Foundation".
    struct BoxCollider
    {
        Core::Math::Vec3 halfExtents; // must be positive on every axis - validated by PhysicsWorld::CreateBody
    };

    struct SphereCollider
    {
        float radius = 0.0f; // must be positive - validated by PhysicsWorld::CreateBody
    };

    // A std::variant, not a generic reflection/component framework -
    // exactly two collider shapes exist today, and a variant is the
    // smallest thing that represents "exactly one of these."
    using ColliderDesc = std::variant<BoxCollider, SphereCollider>;
}
