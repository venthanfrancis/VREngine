#pragma once

// Requires ARENGINE_ENABLE_PHYSICS. The one AREngine::Physics type that
// depends on AREngine::Scene - a documented, pre-authorized exception:
// docs/ARCHITECTURE.md places Physics at the same layer as Scene and
// states "Physics ... reads/writes Scene's transform data through an
// interface. Scene never depends on Physics" - structurally identical
// to the already-implemented XR -> Input same-layer exception. Scene
// itself gains no new field/dependency; only this one file references
// both modules. See docs/ARCHITECTURE.md, "M18 - Physics Foundation".

#include "AREngine/Physics/ColliderDesc.hpp"
#include "AREngine/Physics/PhysicsBodyId.hpp"
#include "AREngine/Physics/RigidBodyDesc.hpp"
#include "AREngine/Scene/EntityId.hpp"
#include "AREngine/Scene/Scene.hpp"

#include <unordered_map>

namespace AREngine::Physics
{
    class PhysicsWorld;

    // Maps Scene entities to physics bodies and synchronizes state
    // between them in one explicit direction per body type - never an
    // ambiguous two-way sync every frame. Owned by whichever higher-
    // level code drives the simulation loop (a demo today; Runtime or
    // similar later) - not a singleton.
    class PhysicsSceneBridge
    {
    public:
        // `entity` MUST be a Scene root - asserts if
        // scene.GetParent(entity).IsValid(). This uniform policy
        // applies to both Static and Dynamic bodies (no special-cased
        // exception for one or the other): M18 does not support
        // parent-relative rigid-body synchronization. Builds a
        // RigidBodyDesc from `entity`'s CURRENT Transform
        // position+rotation (scale is never passed to physics) plus
        // `type`/`collider`/`mass`, creates the body via `world`, and
        // records the entity<->body mapping. This is the one-time
        // Scene -> Physics transfer for BOTH body types - for Static
        // bodies, this is the only synchronization that ever happens;
        // for Dynamic bodies, Physics becomes authoritative from this
        // point on (see SyncDynamicBodiesToScene).
        // Returns the world-owned body id so callers can explicitly destroy it
        // after unregistering. Registering the same entity twice is a caller bug.
        PhysicsBodyId RegisterBody(
            Scene::Scene& scene, Scene::EntityId entity, PhysicsWorld& world,
            BodyType type, const ColliderDesc& collider, float mass = 1.0f);

        // For every registered DYNAMIC body only (Static bodies are
        // one-way Scene->Physics at RegisterBody time only - see
        // above): reads the body's current pose from `world` and
        // writes position+rotation into the mapped Scene entity's
        // Transform (scale is left untouched). Call this ONCE per
        // rendered frame, after all of that frame's fixed physics
        // steps have run - never once per substep, and never once per
        // view.
        void SyncDynamicBodiesToScene(Scene::Scene& scene, const PhysicsWorld& world);

        // Drops the entity<->body mapping only - does not destroy the
        // underlying physics body (call PhysicsWorld::DestroyBody
        // explicitly first if that's also wanted). A no-op if `entity`
        // was never registered.
        void Unregister(Scene::EntityId entity);

    private:
        struct Entry
        {
            PhysicsBodyId body;
            BodyType type;
        };

        std::unordered_map<Scene::EntityId, Entry> m_bodies;
    };
}
