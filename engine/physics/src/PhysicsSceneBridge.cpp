#include "AREngine/Physics/PhysicsSceneBridge.hpp"

#include "AREngine/Core/Assert.hpp"
#include "AREngine/Physics/PhysicsWorld.hpp"

namespace AREngine::Physics
{
    PhysicsBodyId PhysicsSceneBridge::RegisterBody(
        Scene::Scene& scene, Scene::EntityId entity, PhysicsWorld& world,
        BodyType type, const ColliderDesc& collider, float mass)
    {
        AR_ASSERT_MSG(!m_bodies.contains(entity), "An entity cannot have two registered physics bodies");
        AR_ASSERT_MSG(!scene.GetParent(entity).IsValid(),
            "PhysicsSceneBridge::RegisterBody requires a Scene root entity - M18 does not support parent-relative rigid-body synchronization");

        const Scene::Transform& transform = scene.GetTransform(entity);

        RigidBodyDesc desc;
        desc.type = type;
        desc.initialPose = PhysicsPose{transform.position, transform.rotation};
        desc.collider = collider;
        desc.mass = mass;

        const PhysicsBodyId body = world.CreateBody(desc);
        m_bodies[entity] = Entry{body, type};
        return body;
    }

    void PhysicsSceneBridge::SyncDynamicBodiesToScene(Scene::Scene& scene, const PhysicsWorld& world)
    {
        for (const auto& [entity, entry] : m_bodies)
        {
            if (entry.type != BodyType::Dynamic)
            {
                continue;
            }

            const PhysicsPose pose = world.GetBodyPose(entry.body);
            Scene::Transform& transform = scene.GetTransform(entity);
            transform.position = pose.position;
            transform.rotation = pose.rotation;
        }
    }

    void PhysicsSceneBridge::Unregister(Scene::EntityId entity)
    {
        m_bodies.erase(entity);
    }
}
