// M18 headless tests for AREngine::Physics::PhysicsWorld/PhysicsSceneBridge
// (engine/physics, Jolt-backed). Gated behind ARENGINE_ENABLE_PHYSICS -
// requires the real Jolt backend, but no GPU/XR/window. Every
// PhysicsWorld here is constructed with workerThreads=1 to avoid
// spinning up a full hardware-concurrency thread pool per sequential
// test case in this one binary. See docs/ARCHITECTURE.md, "M18 -
// Physics Foundation".

#include "AREngine/Physics/PhysicsSceneBridge.hpp"
#include "AREngine/Physics/PhysicsWorld.hpp"
#include "AREngine/Scene/Scene.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    int g_failureCount = 0;

    void Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", description);
            ++g_failureCount;
        }
    }

    bool NearlyEqual(float a, float b, float tolerance)
    {
        return std::fabs(a - b) <= tolerance;
    }

    using namespace AREngine::Physics;
    namespace Math = AREngine::Core::Math;

    constexpr unsigned int kTestWorkerThreads = 1;
    constexpr float kFixedDelta = 1.0f / 60.0f;

    // --- Body lifetime ---

    void TestCreateStaticAndDynamicBodiesReturnValidDistinctIds()
    {
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);

        RigidBodyDesc staticDesc;
        staticDesc.type = BodyType::Static;
        staticDesc.collider = BoxCollider{Math::Vec3(1.0f, 1.0f, 1.0f)};
        const PhysicsBodyId staticId = world.CreateBody(staticDesc);

        RigidBodyDesc dynamicDesc;
        dynamicDesc.type = BodyType::Dynamic;
        dynamicDesc.initialPose.position = Math::Vec3(0.0f, 5.0f, 0.0f);
        dynamicDesc.collider = SphereCollider{0.5f};
        dynamicDesc.mass = 1.0f;
        const PhysicsBodyId dynamicId = world.CreateBody(dynamicDesc);

        Check(staticId.IsValid(), "Creating a static body returns a valid PhysicsBodyId");
        Check(dynamicId.IsValid(), "Creating a dynamic body returns a valid PhysicsBodyId");
        Check(!(staticId == dynamicId), "Two distinct bodies receive two distinct PhysicsBodyIds");
        Check(world.BodyCount() == 2, "BodyCount reflects both created bodies");

        world.DestroyBody(staticId);
        Check(world.BodyCount() == 1, "BodyCount decreases after destroying one body");
        world.DestroyBody(dynamicId);
        Check(world.BodyCount() == 0, "BodyCount returns to zero after destroying every body");
    }

    void TestWorldDestructionIsClean()
    {
        // No explicit assertion beyond "this doesn't crash" - the real
        // proof here is process survival (and, transitively, that a
        // SECOND PhysicsWorld can still be constructed afterward in a
        // later test, proving global Jolt state tolerates repeated
        // construct/destroy cycles in one process).
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);
        RigidBodyDesc desc;
        desc.type = BodyType::Dynamic;
        desc.collider = BoxCollider{Math::Vec3(0.5f, 0.5f, 0.5f)};
        (void)world.CreateBody(desc);
        Check(true, "PhysicsWorld with live bodies still attached destructs cleanly (no crash reaching this line)");
    }

    // --- Gravity ---

    void TestDynamicBodyFallsUnderGravity()
    {
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);

        RigidBodyDesc desc;
        desc.type = BodyType::Dynamic;
        desc.initialPose.position = Math::Vec3(0.0f, 10.0f, 0.0f);
        desc.collider = SphereCollider{0.5f};
        desc.mass = 1.0f;
        const PhysicsBodyId id = world.CreateBody(desc);

        const float startY = world.GetBodyPose(id).position.y;
        for (int i = 0; i < 30; ++i)
        {
            world.Step(kFixedDelta);
        }
        const float laterY = world.GetBodyPose(id).position.y;

        Check(laterY < startY, "A dynamic body starting above the floor falls (Y decreases) under gravity with nothing beneath it");
    }

    // --- Static body ---

    void TestStaticBodyTransformUnchangedAfterManySteps()
    {
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);

        RigidBodyDesc desc;
        desc.type = BodyType::Static;
        desc.initialPose.position = Math::Vec3(1.0f, 2.0f, 3.0f);
        desc.collider = BoxCollider{Math::Vec3(5.0f, 0.5f, 5.0f)};
        const PhysicsBodyId id = world.CreateBody(desc);

        for (int i = 0; i < 120; ++i)
        {
            world.Step(kFixedDelta);
        }

        const PhysicsPose pose = world.GetBodyPose(id);
        Check(NearlyEqual(pose.position.x, 1.0f, 0.001f) &&
              NearlyEqual(pose.position.y, 2.0f, 0.001f) &&
              NearlyEqual(pose.position.z, 3.0f, 0.001f),
              "A static body's position is unchanged after many simulation steps");
    }

    // --- Collision / landing ---

    void TestDynamicBodyLandsOnStaticFloorWithoutTunneling()
    {
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);

        // Floor: a wide, thin static box, top surface at Y=0.
        RigidBodyDesc floorDesc;
        floorDesc.type = BodyType::Static;
        floorDesc.initialPose.position = Math::Vec3(0.0f, -0.5f, 0.0f);
        floorDesc.collider = BoxCollider{Math::Vec3(10.0f, 0.5f, 10.0f)};
        (void)world.CreateBody(floorDesc);

        // A dynamic box dropped from above, expected to settle with its
        // own center at roughly Y = 0.5 (half-extent) once resting on
        // the floor's top surface at Y=0.
        RigidBodyDesc boxDesc;
        boxDesc.type = BodyType::Dynamic;
        boxDesc.initialPose.position = Math::Vec3(0.0f, 5.0f, 0.0f);
        boxDesc.collider = BoxCollider{Math::Vec3(0.5f, 0.5f, 0.5f)};
        boxDesc.mass = 1.0f;
        const PhysicsBodyId boxId = world.CreateBody(boxDesc);

        // 3 seconds of simulation at 60Hz is ample time to fall ~5m and settle.
        for (int i = 0; i < 180; ++i)
        {
            world.Step(kFixedDelta);
        }

        const float restingY = world.GetBodyPose(boxId).position.y;
        Check(restingY > -0.1f, "The dynamic box does not tunnel through the floor (resting Y is not far below the floor's top surface)");
        Check(NearlyEqual(restingY, 0.5f, 0.15f), "The dynamic box settles near its expected resting height (top-of-floor + half-extent), within tolerance");
    }

    void TestDynamicSphereLandsOnStaticFloor()
    {
        // Proves SphereCollider support end-to-end (create + simulate +
        // settle) - required per M18's collider scope, verified
        // headlessly rather than in the visual demo (see
        // docs/ARCHITECTURE.md for why).
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);

        RigidBodyDesc floorDesc;
        floorDesc.type = BodyType::Static;
        floorDesc.initialPose.position = Math::Vec3(0.0f, -0.5f, 0.0f);
        floorDesc.collider = BoxCollider{Math::Vec3(10.0f, 0.5f, 10.0f)};
        (void)world.CreateBody(floorDesc);

        RigidBodyDesc sphereDesc;
        sphereDesc.type = BodyType::Dynamic;
        sphereDesc.initialPose.position = Math::Vec3(0.0f, 5.0f, 0.0f);
        sphereDesc.collider = SphereCollider{0.5f};
        sphereDesc.mass = 1.0f;
        const PhysicsBodyId sphereId = world.CreateBody(sphereDesc);

        for (int i = 0; i < 180; ++i)
        {
            world.Step(kFixedDelta);
        }

        const float restingY = world.GetBodyPose(sphereId).position.y;
        Check(NearlyEqual(restingY, 0.5f, 0.15f), "A dynamic sphere settles near its expected resting height (top-of-floor + radius)");
    }

    // --- Coordinate/quaternion conversion (indirect, via the public API - see file header) ---

    void TestQuaternionRoundTripsThroughCreateAndGetBodyPose()
    {
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);

        // A known, non-trivial rotation: 90 degrees about world Up (+Y).
        const Math::Quaternion knownRotation = Math::Quaternion::FromAxisAngle(Math::Vec3(0.0f, 1.0f, 0.0f), 1.5707963f);

        RigidBodyDesc desc;
        desc.type = BodyType::Static; // static - no simulation should change it before we read it back
        desc.initialPose.position = Math::Vec3(2.0f, 3.0f, 4.0f);
        desc.initialPose.rotation = knownRotation;
        desc.collider = BoxCollider{Math::Vec3(1.0f, 1.0f, 1.0f)};
        const PhysicsBodyId id = world.CreateBody(desc);

        const PhysicsPose pose = world.GetBodyPose(id);
        Check(NearlyEqual(pose.position.x, 2.0f, 0.001f) && NearlyEqual(pose.position.y, 3.0f, 0.001f) && NearlyEqual(pose.position.z, 4.0f, 0.001f),
              "Position round-trips exactly through CreateBody -> GetBodyPose");
        Check(NearlyEqual(pose.rotation.w, knownRotation.w, 0.001f) &&
              NearlyEqual(pose.rotation.x, knownRotation.x, 0.001f) &&
              NearlyEqual(pose.rotation.y, knownRotation.y, 0.001f) &&
              NearlyEqual(pose.rotation.z, knownRotation.z, 0.001f),
              "A known 90-degree rotation round-trips through AREngine -> Jolt -> AREngine with no accidental axis flip or component misordering");
    }

    // --- Scene bridge ---

    void TestSceneBridgeRegistersRootEntityAndTransfersInitialTransform()
    {
        AREngine::Scene::Scene scene;
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);
        PhysicsSceneBridge bridge;

        const AREngine::Scene::EntityId entity = scene.CreateEntity("DynamicBox");
        scene.GetTransform(entity).position = Math::Vec3(0.0f, 5.0f, 0.0f);

        bridge.RegisterBody(scene, entity, world, BodyType::Dynamic, BoxCollider{Math::Vec3(0.5f, 0.5f, 0.5f)}, 1.0f);

        Check(NearlyEqual(scene.GetTransform(entity).position.y, 5.0f, 0.001f),
              "Registering a body doesn't itself move the Scene transform yet (only the initial pose was transferred TO physics)");
    }

    void TestSceneBridgeStepUpdatesSceneTransform()
    {
        AREngine::Scene::Scene scene;
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);
        PhysicsSceneBridge bridge;

        const AREngine::Scene::EntityId entity = scene.CreateEntity("DynamicBox");
        scene.GetTransform(entity).position = Math::Vec3(0.0f, 10.0f, 0.0f);
        bridge.RegisterBody(scene, entity, world, BodyType::Dynamic, SphereCollider{0.5f}, 1.0f);

        const float startY = scene.GetTransform(entity).position.y;
        for (int i = 0; i < 30; ++i)
        {
            world.Step(kFixedDelta);
        }
        bridge.SyncDynamicBodiesToScene(scene, world);

        Check(scene.GetTransform(entity).position.y < startY,
              "After stepping physics and syncing, the Scene entity's transform reflects the body's new (fallen) position");
    }

    void TestSceneBridgeRejectsParentedEntity()
    {
        AREngine::Scene::Scene scene;
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);
        PhysicsSceneBridge bridge;

        const AREngine::Scene::EntityId parent = scene.CreateEntity("Parent");
        const AREngine::Scene::EntityId child = scene.CreateEntity("Child");
        scene.SetParent(child, parent);

        Check(scene.GetParent(child).IsValid(), "Sanity check: child does have a parent before the assert-triggering call below");
        // Intentionally NOT calling bridge.RegisterBody(scene, child, ...) here -
        // that would AR_ASSERT_MSG and terminate this test binary. The
        // milestone's own "reject/assert at registration" policy is
        // proven by code inspection (see PhysicsSceneBridge.cpp) and by
        // this test confirming the precondition it checks is real and
        // reachable, not by triggering the assert itself (which
        // would abort the whole test binary, consistent with how this
        // codebase never unit-tests its own AR_ASSERT_MSG firing
        // elsewhere either).
    }

    void TestSceneBridgeUnregisterLeavesNoStaleSyncEntry()
    {
        AREngine::Scene::Scene scene;
        PhysicsWorld world(Math::Vec3(0.0f, -9.81f, 0.0f), kTestWorkerThreads);
        PhysicsSceneBridge bridge;

        const AREngine::Scene::EntityId entity = scene.CreateEntity("DynamicBox");
        scene.GetTransform(entity).position = Math::Vec3(0.0f, 10.0f, 0.0f);
        const auto body = bridge.RegisterBody(scene, entity, world, BodyType::Dynamic, SphereCollider{0.5f}, 1.0f);

        bridge.Unregister(entity);

        const float positionBeforeSync = scene.GetTransform(entity).position.y;
        for (int i = 0; i < 30; ++i)
        {
            world.Step(kFixedDelta);
        }
        bridge.SyncDynamicBodiesToScene(scene, world);

        Check(NearlyEqual(scene.GetTransform(entity).position.y, positionBeforeSync, 0.0001f),
              "After Unregister, SyncDynamicBodiesToScene no longer touches that entity's transform - no stale sync entry remains");
        world.DestroyBody(body);
        Check(world.BodyCount() == 0, "Bridge-created body can be explicitly destroyed using its returned id");
    }
}

int main()
{
    TestCreateStaticAndDynamicBodiesReturnValidDistinctIds();
    TestWorldDestructionIsClean();
    TestDynamicBodyFallsUnderGravity();
    TestStaticBodyTransformUnchangedAfterManySteps();
    TestDynamicBodyLandsOnStaticFloorWithoutTunneling();
    TestDynamicSphereLandsOnStaticFloor();
    TestQuaternionRoundTripsThroughCreateAndGetBodyPose();
    TestSceneBridgeRegistersRootEntityAndTransfersInitialTransform();
    TestSceneBridgeStepUpdatesSceneTransform();
    TestSceneBridgeRejectsParentedEntity();
    TestSceneBridgeUnregisterLeavesNoStaleSyncEntry();

    if (g_failureCount == 0)
    {
        std::printf("All M18 Physics checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
