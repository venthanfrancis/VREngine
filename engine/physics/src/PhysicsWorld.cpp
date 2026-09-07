#include "AREngine/Physics/PhysicsWorld.hpp"

#include "jolt/JoltConversions.hpp"
#include "jolt/JoltLayers.hpp"

#include "AREngine/Core/Assert.hpp"

// The one translation unit in this codebase that defines Jolt's global
// process state and touches its concrete shape/body types directly - no
// Jolt type crosses out of this file or jolt/*. See
// docs/ARCHITECTURE.md, "M18 - Physics Foundation".
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <mutex>
#include <cmath>
#include <thread>
#include <unordered_map>
#include <variant>

namespace AREngine::Physics
{
    namespace
    {
        bool IsFinite(Core::Math::Vec3 v)
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        bool IsValidPose(const PhysicsPose& pose)
        {
            const auto q = pose.rotation;
            const float norm = q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z;
            return IsFinite(pose.position) && std::isfinite(norm) && std::abs(norm - 1.0f) < 0.001f;
        }
        // Process-global, one-time Jolt initialization (allocator,
        // factory, type registration) - guarded so constructing however
        // many PhysicsWorld instances a process needs (e.g. sequential
        // test cases in one binary) only ever runs this once. Never
        // torn down (no UnregisterTypes()/delete Factory::sInstance) -
        // intentionally process-lifetime-scoped, the same category as
        // never explicitly unloading the Vulkan loader. See
        // docs/ARCHITECTURE.md.
        void EnsureGlobalJoltInitialized()
        {
            static std::once_flag flag;
            std::call_once(flag, []
            {
                JPH::RegisterDefaultAllocator();
                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            });
        }

        int ResolveWorkerThreadCount(unsigned int requested)
        {
            if (requested != 0)
            {
                return static_cast<int>(requested);
            }
            const unsigned int hardwareConcurrency = std::thread::hardware_concurrency();
            return static_cast<int>(hardwareConcurrency > 1 ? hardwareConcurrency - 1 : 1);
        }
    }

    struct PhysicsWorld::Impl
    {
        // Modest capacity constants sized for this foundation
        // milestone's small demo/test scenes - not tuned for
        // production scale.
        static constexpr JPH::uint kMaxBodies = 1024;
        static constexpr JPH::uint kNumBodyMutexes = 0; // 0 = Jolt picks a default
        static constexpr JPH::uint kMaxBodyPairs = 1024;
        static constexpr JPH::uint kMaxContactConstraints = 1024;
        static constexpr int kMaxPhysicsJobs = 2048;
        static constexpr int kMaxPhysicsBarriers = 8;
        static constexpr std::uint32_t kTempAllocatorSizeBytes = 10 * 1024 * 1024;

        Jolt::BPLayerInterfaceImpl broadPhaseLayerInterface;
        Jolt::ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
        Jolt::ObjectLayerPairFilterImpl objectLayerPairFilter;
        JPH::TempAllocatorImpl tempAllocator;
        JPH::JobSystemThreadPool jobSystem;
        JPH::PhysicsSystem physicsSystem;

        std::unordered_map<PhysicsBodyId, JPH::BodyID> bodies;
        std::uint64_t nextBodyId = 1;

        Impl(const Core::Math::Vec3& gravity, unsigned int workerThreads)
            : tempAllocator(kTempAllocatorSizeBytes)
            , jobSystem(kMaxPhysicsJobs, kMaxPhysicsBarriers, ResolveWorkerThreadCount(workerThreads))
        {
            physicsSystem.Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints,
                broadPhaseLayerInterface, objectVsBroadPhaseLayerFilter, objectLayerPairFilter);
            physicsSystem.SetGravity(Jolt::ToJolt(gravity));
        }
    };

    PhysicsWorld::PhysicsWorld(const Core::Math::Vec3& gravity, unsigned int workerThreads)
    {
        EnsureGlobalJoltInitialized();
        m_impl = std::make_unique<Impl>(gravity, workerThreads);
    }

    // Declared out-of-line specifically so this translation unit - which
    // includes the real Jolt headers - is the one that instantiates
    // std::unique_ptr<Impl>'s destructor, not every file that merely
    // includes PhysicsWorld.hpp (which only forward-declares Impl).
    PhysicsWorld::~PhysicsWorld() = default;

    PhysicsBodyId PhysicsWorld::CreateBody(const RigidBodyDesc& desc)
    {
        AR_ASSERT_MSG(IsValidPose(desc.initialPose), "Body pose must be finite with unit rotation");
        AR_ASSERT_MSG(desc.type == BodyType::Static || desc.type == BodyType::Dynamic, "Unknown body type");
        JPH::Ref<JPH::Shape> shape;
        if (const auto* box = std::get_if<BoxCollider>(&desc.collider))
        {
            AR_ASSERT_MSG(IsFinite(box->halfExtents) && box->halfExtents.x > 0.0f && box->halfExtents.y > 0.0f && box->halfExtents.z > 0.0f,
                "BoxCollider half-extents must be positive on every axis");
            shape = new JPH::BoxShape(Jolt::ToJolt(box->halfExtents));
        }
        else
        {
            const auto& sphere = std::get<SphereCollider>(desc.collider);
            AR_ASSERT_MSG(std::isfinite(sphere.radius) && sphere.radius > 0.0f, "SphereCollider radius must be positive and finite");
            shape = new JPH::SphereShape(sphere.radius);
        }

        AR_ASSERT_MSG(desc.type == BodyType::Static || (std::isfinite(desc.mass) && desc.mass > 0.0f), "A Dynamic body's mass must be positive and finite");

        const JPH::EMotionType motionType = desc.type == BodyType::Dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
        const JPH::ObjectLayer layer = desc.type == BodyType::Dynamic ? Jolt::Layers::MOVING : Jolt::Layers::NON_MOVING;

        JPH::BodyCreationSettings settings(
            shape.GetPtr(), Jolt::ToJoltR(desc.initialPose.position), Jolt::ToJolt(desc.initialPose.rotation),
            motionType, layer);
        if (desc.type == BodyType::Dynamic)
        {
            // Use the requested mass directly, letting Jolt compute the
            // inertia tensor from the shape - "keep mass simple," no
            // density/material system (see RigidBodyDesc.hpp).
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.mass;
        }

        JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
        JPH::Body* body = bodyInterface.CreateBody(settings);
        AR_ASSERT_MSG(body != nullptr, "Jolt failed to create a body - body-capacity limit exceeded?");

        bodyInterface.AddBody(body->GetID(), desc.type == BodyType::Dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

        const PhysicsBodyId id{m_impl->nextBodyId++};
        m_impl->bodies.emplace(id, body->GetID());
        return id;
    }

    void PhysicsWorld::DestroyBody(PhysicsBodyId id)
    {
        const auto it = m_impl->bodies.find(id);
        AR_ASSERT_MSG(it != m_impl->bodies.end(), "DestroyBody called with an unknown or already-destroyed PhysicsBodyId");

        JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
        bodyInterface.RemoveBody(it->second);
        bodyInterface.DestroyBody(it->second);
        m_impl->bodies.erase(it);
    }

    void PhysicsWorld::Step(float fixedDeltaSeconds)
    {
        AR_ASSERT_MSG(std::isfinite(fixedDeltaSeconds) && fixedDeltaSeconds > 0.0f, "Physics step must be positive and finite");
        constexpr int kCollisionSteps = 1;
        m_impl->physicsSystem.Update(fixedDeltaSeconds, kCollisionSteps, &m_impl->tempAllocator, &m_impl->jobSystem);
    }

    PhysicsPose PhysicsWorld::GetBodyPose(PhysicsBodyId id) const
    {
        const auto it = m_impl->bodies.find(id);
        AR_ASSERT_MSG(it != m_impl->bodies.end(), "GetBodyPose called with an unknown or already-destroyed PhysicsBodyId");

        JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
        return PhysicsPose{
            Jolt::FromJolt(bodyInterface.GetPosition(it->second)),
            Jolt::FromJolt(bodyInterface.GetRotation(it->second))};
    }

    void PhysicsWorld::SetBodyPose(PhysicsBodyId id, const PhysicsPose& pose)
    {
        AR_ASSERT_MSG(IsValidPose(pose), "Body pose must be finite with unit rotation");
        const auto it = m_impl->bodies.find(id);
        AR_ASSERT_MSG(it != m_impl->bodies.end(), "SetBodyPose called with an unknown or already-destroyed PhysicsBodyId");

        JPH::BodyInterface& bodyInterface = m_impl->physicsSystem.GetBodyInterface();
        bodyInterface.SetPositionAndRotation(it->second, Jolt::ToJoltR(pose.position), Jolt::ToJolt(pose.rotation), JPH::EActivation::Activate);
    }

    std::size_t PhysicsWorld::BodyCount() const
    {
        return m_impl->bodies.size();
    }
}
