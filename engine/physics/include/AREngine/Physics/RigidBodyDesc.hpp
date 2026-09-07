#pragma once

#include "AREngine/Physics/ColliderDesc.hpp"
#include "AREngine/Physics/PhysicsPose.hpp"

namespace AREngine::Physics
{
    // Static bodies never move dynamically (though they can still be
    // explicitly repositioned via PhysicsWorld::SetBodyPose) and
    // participate in collisions; Dynamic bodies fall under gravity and
    // respond to collisions. No Kinematic type yet - M18 doesn't need
    // one (see docs/ARCHITECTURE.md).
    enum class BodyType
    {
        Static,
        Dynamic
    };

    // A minimal, backend-neutral body-creation description. `mass` is
    // ignored for Static bodies (kept in the struct rather than made a
    // separate Dynamic-only type, to keep this one narrow, ungeneralized
    // struct rather than a small hierarchy for a two-member difference).
    // No density/material system, no friction/restitution override -
    // PhysicsWorld uses reasonable backend defaults for those.
    struct RigidBodyDesc
    {
        BodyType type = BodyType::Static;
        PhysicsPose initialPose;
        ColliderDesc collider;
        float mass = 1.0f; // ignored for BodyType::Static
    };
}
