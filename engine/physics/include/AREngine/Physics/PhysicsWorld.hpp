#pragma once

// Requires ARENGINE_ENABLE_PHYSICS. Not part of the unconditional
// Physics.hpp umbrella - include this directly where a real physics
// backend is actually needed.
//
// PIMPL: no Jolt type is ever named in this header (or any other public
// AREngine header) - see docs/ARCHITECTURE.md, "M18 - Physics
// Foundation", "Backend-Independent Public API".

#include "AREngine/Physics/PhysicsBodyId.hpp"
#include "AREngine/Physics/PhysicsPose.hpp"
#include "AREngine/Physics/RigidBodyDesc.hpp"

#include <cstddef>
#include <memory>

namespace AREngine::Physics
{
    // Owns one independent physics simulation: the backend world/system,
    // every rigid body created through it, and the backend's own worker
    // thread pool. Multiple independent PhysicsWorld instances may
    // coexist in one process (e.g. across sequential test cases) - see
    // docs/ARCHITECTURE.md for how this is reconciled with the backend's
    // own process-global one-time initialization requirements.
    //
    // Does NOT own a Scene, does not render, does not poll Input, does
    // not know OpenXR or Vulkan.
    //
    // Not copyable or movable: owns real backend resources, destroyed
    // exactly once, by this object alone - same discipline as every
    // owned Vulkan resource in this codebase.
    class PhysicsWorld
    {
    public:
        // `gravity` defaults to AREngine's documented world convention
        // (1 unit = 1 meter, +Y up) applied as (0, -9.81, 0) m/s^2.
        // `workerThreads` = 0 means "auto" (hardware_concurrency() - 1,
        // minimum 1); pass an explicit small value (e.g. 1) for test
        // code constructing many PhysicsWorld instances in one process,
        // to avoid spinning up a full thread pool per instance.
        explicit PhysicsWorld(const Core::Math::Vec3& gravity = Core::Math::Vec3(0.0f, -9.81f, 0.0f),
                               unsigned int workerThreads = 0);
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;
        PhysicsWorld(PhysicsWorld&&) = delete;
        PhysicsWorld& operator=(PhysicsWorld&&) = delete;

        // Validates `desc` (positive collider extents/radius, finite
        // pose - AR_ASSERT_MSG on violation, a caller/content bug, not a
        // predictable runtime condition) and creates a body. Asserts if
        // the backend itself fails to create the body (e.g. an
        // exceeded body-capacity limit) - a setup/tuning bug at this
        // narrow foundation-milestone scale, not untrusted input.
        [[nodiscard]] PhysicsBodyId CreateBody(const RigidBodyDesc& desc);

        // Asserts if `id` is unknown or already destroyed - the caller
        // always owns the id it's destroying (unlike, say,
        // Scene::DestroyEntity, which no-ops on a stale id because
        // cross-cutting code unrelated to the original creator may
        // plausibly hold a stale EntityId; a PhysicsBodyId has no such
        // cross-cutting-access pattern in this codebase).
        void DestroyBody(PhysicsBodyId id);

        // Advances the simulation by exactly `fixedDeltaSeconds` - the
        // caller (a FixedTimestepAccumulator-driven loop) is responsible
        // for calling this a fixed number of times per frame, never with
        // an arbitrary render delta.
        void Step(float fixedDeltaSeconds);

        // Asserts if `id` is unknown or already destroyed.
        [[nodiscard]] PhysicsPose GetBodyPose(PhysicsBodyId id) const;

        // Explicitly teleports a body to `pose`, waking it first if the
        // backend requires that for a dynamic body to respond to the
        // change. Not a continuous kinematic-controller API - just a
        // one-shot position/orientation set, useful for tests and
        // initial placement. Asserts if `id` is unknown or already
        // destroyed.
        void SetBodyPose(PhysicsBodyId id, const PhysicsPose& pose);

        // Diagnostics only.
        [[nodiscard]] std::size_t BodyCount() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
