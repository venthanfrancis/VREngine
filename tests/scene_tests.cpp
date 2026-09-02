// Automated tests for AREngine::Scene: entity identity, Transform,
// parent/child hierarchy, and world-matrix composition (M5), plus
// Transform's forward/right/up accessors and Camera (M8G), plus the
// Renderable component and Scene::ExtractRenderables (M12). No human
// interaction, no window, no Rendering involved at all — proving Scene
// and Rendering stay fully decoupled.

#include "AREngine/Core/Math/MathUtil.hpp"
#include "AREngine/Scene/Camera.hpp"
#include "AREngine/Scene/Scene.hpp"

#include <cstdio>
#include <numbers>

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

    void CheckNearlyEqual(float actual, float expected, const char* description)
    {
        Check(AREngine::Core::Math::NearlyEqual(actual, expected), description);
    }

    using namespace AREngine::Scene;
    using namespace AREngine::Core::Math;

    void TestEntityCreation()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity("Cube");

        Check(entity.IsValid(), "CreateEntity returns a valid EntityId");
        Check(scene.IsValid(entity), "Scene::IsValid agrees the new entity is valid");
        Check(scene.GetName(entity) == "Cube", "GetName returns the name passed to CreateEntity");
    }

    void TestDistinctIds()
    {
        Scene scene;
        const EntityId a = scene.CreateEntity("A");
        const EntityId b = scene.CreateEntity("B");
        const EntityId c = scene.CreateEntity("C");

        Check(!(a == b), "Two entities get distinguishable ids (a != b)");
        Check(!(a == c), "Two entities get distinguishable ids (a != c)");
        Check(!(b == c), "Two entities get distinguishable ids (b != c)");
    }

    void TestDestroyedEntityBecomesInvalid()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity("Temp");
        Check(scene.IsValid(entity), "Entity is valid before destruction");

        scene.DestroyEntity(entity);
        Check(!scene.IsValid(entity), "Entity is invalid after DestroyEntity");
    }

    void TestDefaultTransform()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        const Transform& transform = scene.GetTransform(entity);

        Check(transform.position == Vec3(0.0f, 0.0f, 0.0f), "Default Transform position is (0,0,0)");
        Check(transform.rotation == Quaternion::Identity(), "Default Transform rotation is identity");
        Check(transform.scale == Vec3(1.0f, 1.0f, 1.0f), "Default Transform scale is (1,1,1)");
    }

    void TestTransformStorage()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();

        Transform& mutableTransform = scene.GetTransform(entity);
        mutableTransform.position = Vec3(1.0f, 2.0f, 3.0f);
        mutableTransform.rotation = Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float> / 2.0f);
        mutableTransform.scale = Vec3(2.0f, 2.0f, 2.0f);

        const Transform& readBack = scene.GetTransform(entity);
        Check(readBack.position == Vec3(1.0f, 2.0f, 3.0f), "Position written through the mutable accessor is read back correctly");
        Check(readBack.scale == Vec3(2.0f, 2.0f, 2.0f), "Scale written through the mutable accessor is read back correctly");
        Check(readBack.rotation == mutableTransform.rotation, "Rotation written through the mutable accessor is read back correctly");
    }

    void TestTransformToMatrixMatchesMat4TRS()
    {
        Transform transform;
        transform.position = Vec3(1.0f, 2.0f, 3.0f);
        transform.rotation = Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float> / 2.0f);
        transform.scale = Vec3(2.0f, 1.0f, 1.0f);

        const Mat4 expected = Mat4::TRS(transform.position, transform.rotation, transform.scale);
        Check(transform.ToMatrix() == expected, "Transform::ToMatrix delegates to Mat4::TRS with its own fields");
    }

    void TestTransformDefaultForwardRightUp()
    {
        const Transform transform; // identity rotation
        Check(transform.GetForward() == kWorldForward, "Default Transform's forward is world Forward (-Z)");
        Check(transform.GetRight() == kWorldRight, "Default Transform's right is world Right (+X)");
        Check(transform.GetUp() == kWorldUp, "Default Transform's up is world Up (+Y)");
    }

    void TestTransformForwardAfterYaw()
    {
        // Same fact core_tests.cpp proves at the Mat4 and Quaternion
        // levels: rotating Right 90deg around Up lands on Forward.
        // Proving it once more through Transform::GetForward (starting
        // from Right, after a 90deg yaw) confirms Transform's own
        // accessor agrees, not just Quaternion::Rotate in isolation.
        Transform transform;
        transform.rotation = Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float> / 2.0f);

        const Vec3 forward = transform.GetForward();
        // At yaw=90deg, GetForward() rotates world Forward (-Z, not
        // Right) by 90deg around Up. Rotating -Z by +90deg around +Y
        // lands on -X (by the same cyclic rotation pattern
        // Right(+X)->Forward(-Z)->-Right(-X)->-Forward(+Z)->Right(+X)).
        CheckNearlyEqual(forward.x, -1.0f, "Transform::GetForward after a 90deg yaw: x");
        CheckNearlyEqual(forward.y, 0.0f, "Transform::GetForward after a 90deg yaw: y");
        CheckNearlyEqual(forward.z, 0.0f, "Transform::GetForward after a 90deg yaw: z");
    }

    void TestCameraDefaults()
    {
        const Camera camera;
        Check(camera.nearZ > 0.0f && camera.nearZ < camera.farZ, "Default Camera has a sane near < far range");
        Check(camera.verticalFovRadians > 0.0f && camera.verticalFovRadians < std::numbers::pi_v<float>,
              "Default Camera's vertical FOV is a plausible radians value (0 < fov < pi)");
        CheckNearlyEqual(camera.GetAspectRatio(), 16.0f / 9.0f, "Default Camera aspect ratio is a sane placeholder (16:9)");
    }

    void TestCameraSetAspectRatio()
    {
        Camera camera;
        camera.SetAspectRatio(4.0f / 3.0f);
        CheckNearlyEqual(camera.GetAspectRatio(), 4.0f / 3.0f, "SetAspectRatio updates GetAspectRatio");

        const Mat4 proj = camera.GetProjectionMatrix();
        const float expectedXScale = proj.At(1, 1) / (4.0f / 3.0f); // focalLength/aspect == proj.At(1,1)/aspect
        CheckNearlyEqual(proj.At(0, 0), expectedXScale, "GetProjectionMatrix's X scale reflects the current aspect ratio");
    }

    void TestCameraViewMatrixFromTransform()
    {
        Camera camera;
        Transform transform;
        transform.position = Vec3(0.0f, 0.0f, 3.0f); // identity rotation -> looking down -Z, i.e. toward the origin

        const Mat4 view = camera.GetViewMatrix(transform);
        const Vec3 originInView = TransformPoint(view, Vec3(0.0f, 0.0f, 0.0f));
        // Same fact M8F's TestLookAtRH already established directly for
        // LookAtRH — this confirms Camera::GetViewMatrix, built purely
        // from a Transform, reproduces it.
        CheckNearlyEqual(originInView.x, 0.0f, "Camera::GetViewMatrix: world origin.x in view space");
        CheckNearlyEqual(originInView.y, 0.0f, "Camera::GetViewMatrix: world origin.y in view space");
        CheckNearlyEqual(originInView.z, -3.0f, "Camera::GetViewMatrix: world origin is 3m in front (forward = -Z)");
    }

    void TestRootWorldMatrixEqualsLocal()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        scene.GetTransform(entity).position = Vec3(5.0f, 0.0f, -3.0f);

        Check(scene.GetWorldMatrix(entity) == scene.GetTransform(entity).ToMatrix(),
              "A root entity's world matrix equals its own local matrix");
    }

    void TestParentChildComposition()
    {
        // Pure translation on both, so the expected result is exact
        // (no trig rounding to account for).
        Scene scene;
        const EntityId parent = scene.CreateEntity("Parent");
        const EntityId child = scene.CreateEntity("Child");

        scene.GetTransform(parent).position = Vec3(10.0f, 0.0f, 0.0f);
        scene.GetTransform(child).position = Vec3(0.0f, 5.0f, 0.0f);

        Check(scene.SetParent(child, parent), "SetParent succeeds for a valid, non-cyclic pair");

        const Mat4 expected = Mat4::Translation(Vec3(10.0f, 0.0f, 0.0f)) * Mat4::Translation(Vec3(0.0f, 5.0f, 0.0f));
        Check(scene.GetWorldMatrix(child) == expected,
              "Child world matrix == parent local matrix * child local matrix");

        const Vec3 childWorldOrigin = TransformPoint(scene.GetWorldMatrix(child), Vec3(0.0f, 0.0f, 0.0f));
        Check(childWorldOrigin == Vec3(10.0f, 5.0f, 0.0f), "Child's world position combines both translations");
    }

    void TestThreeLevelHierarchy()
    {
        Scene scene;
        const EntityId grandparent = scene.CreateEntity("Grandparent");
        const EntityId parent = scene.CreateEntity("Parent");
        const EntityId child = scene.CreateEntity("Child");

        scene.GetTransform(grandparent).position = Vec3(20.0f, 0.0f, 0.0f);
        scene.GetTransform(parent).position = Vec3(0.0f, 3.0f, 0.0f);
        scene.GetTransform(child).position = Vec3(0.0f, 0.0f, 7.0f);

        Check(scene.SetParent(parent, grandparent), "SetParent(parent, grandparent) succeeds");
        Check(scene.SetParent(child, parent), "SetParent(child, parent) succeeds");

        const Vec3 childWorldOrigin = TransformPoint(scene.GetWorldMatrix(child), Vec3(0.0f, 0.0f, 0.0f));
        Check(childWorldOrigin == Vec3(20.0f, 3.0f, 7.0f),
              "Three-level hierarchy composes all three translations");
    }

    void TestParentChildMultiplicationOrderObvious()
    {
        // The important, easy-to-get-backwards case: a parent's
        // ROTATION applied to a child's LOCAL POSITION. If Mat4
        // multiplication order (or TRS order) were wrong, this would
        // silently produce a plausible-looking but incorrect result
        // instead of an obviously broken one — see
        // docs/WORLD_CONVENTIONS.md for the underlying handedness fact
        // this relies on.
        Scene scene;
        const EntityId parent = scene.CreateEntity("Parent");
        const EntityId child = scene.CreateEntity("Child");

        // Parent faces 90 degrees around world Up (+Y).
        scene.GetTransform(parent).rotation = Quaternion::FromAxisAngle(kWorldUp, std::numbers::pi_v<float> / 2.0f);
        // Child sits 1 meter along the parent's local +X (Right).
        scene.GetTransform(child).position = Vec3(1.0f, 0.0f, 0.0f);

        Check(scene.SetParent(child, parent), "SetParent succeeds");

        const Vec3 childWorldOrigin = TransformPoint(scene.GetWorldMatrix(child), Vec3(0.0f, 0.0f, 0.0f));

        // Rotating Right (+X) by 90 degrees around Up (+Y) lands on
        // Forward (-Z) — see the matching Core-level proof in
        // core_tests.cpp's TestMat4TransformFactories.
        CheckNearlyEqual(childWorldOrigin.x, 0.0f, "Rotated child world position: x");
        CheckNearlyEqual(childWorldOrigin.y, 0.0f, "Rotated child world position: y");
        CheckNearlyEqual(childWorldOrigin.z, -1.0f, "Rotated child world position: z (ends up 1m Forward)");
    }

    void TestSetParentAndClearParent()
    {
        Scene scene;
        const EntityId parent = scene.CreateEntity("Parent");
        const EntityId child = scene.CreateEntity("Child");

        Check(scene.SetParent(child, parent), "SetParent succeeds");
        Check(scene.GetParent(child) == parent, "GetParent reports the new parent");
        Check(scene.GetChildren(parent).size() == 1 && scene.GetChildren(parent)[0] == child,
              "GetChildren reports the new child");

        scene.ClearParent(child);
        Check(!scene.GetParent(child).IsValid(), "GetParent is invalid after ClearParent (child is now a root)");
        Check(scene.GetChildren(parent).empty(), "Former parent's children list no longer contains the child");
    }

    void TestReparentingLeavesLocalTransformUnchanged()
    {
        Scene scene;
        const EntityId oldParent = scene.CreateEntity("OldParent");
        const EntityId newParent = scene.CreateEntity("NewParent");
        const EntityId child = scene.CreateEntity("Child");

        scene.GetTransform(newParent).position = Vec3(100.0f, 0.0f, 0.0f);
        scene.GetTransform(child).position = Vec3(1.0f, 0.0f, 0.0f);

        scene.SetParent(child, oldParent);
        const Vec3 localBefore = scene.GetTransform(child).position;

        scene.SetParent(child, newParent); // reparent
        const Vec3 localAfter = scene.GetTransform(child).position;

        Check(localBefore == localAfter, "Reparenting leaves the child's LOCAL transform unchanged");

        // Documented consequence: since the local transform didn't
        // change but the parent did, the WORLD transform changes.
        const Vec3 worldAfter = TransformPoint(scene.GetWorldMatrix(child), Vec3(0.0f, 0.0f, 0.0f));
        Check(worldAfter == Vec3(101.0f, 0.0f, 0.0f), "Reparenting changes the child's WORLD position, as documented");
    }

    void TestSelfParentingRejected()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        Check(scene.SetParent(entity, entity) == false, "An entity cannot be parented to itself");
    }

    void TestCycleRejected()
    {
        Scene scene;
        const EntityId a = scene.CreateEntity("A");
        const EntityId b = scene.CreateEntity("B");
        const EntityId c = scene.CreateEntity("C");

        // A parent of B, B parent of C: A -> B -> C.
        Check(scene.SetParent(b, a), "SetParent(B, A): A becomes B's parent");
        Check(scene.SetParent(c, b), "SetParent(C, B): B becomes C's parent");

        // SetParent(A, C) would make C the parent of A, closing the
        // loop A -> B -> C -> A. Must be rejected.
        Check(scene.SetParent(a, c) == false, "SetParent(A, C) is rejected: it would create a cycle");

        // Hierarchy must be unchanged after the rejected attempt.
        Check(!scene.GetParent(a).IsValid(), "A is still a root after the rejected SetParent");
        Check(scene.GetParent(b) == a, "B's parent is still A after the rejected SetParent");
        Check(scene.GetParent(c) == b, "C's parent is still B after the rejected SetParent");
    }

    void TestDestroyParentDestroysDescendants()
    {
        Scene scene;
        const EntityId grandparent = scene.CreateEntity("Grandparent");
        const EntityId parent = scene.CreateEntity("Parent");
        const EntityId child = scene.CreateEntity("Child");

        scene.SetParent(parent, grandparent);
        scene.SetParent(child, parent);

        scene.DestroyEntity(grandparent);

        Check(!scene.IsValid(grandparent), "Destroyed entity is invalid");
        Check(!scene.IsValid(parent), "Destroying a parent also destroys its child");
        Check(!scene.IsValid(child), "Destroying a grandparent also destroys its grandchild");
    }

    void TestInvalidEntityAccessIsPredictable()
    {
        // Query accessors (GetTransform/GetName/GetParent/GetChildren/
        // GetWorldMatrix) assert on an invalid/unknown EntityId by
        // design — see docs/ARCHITECTURE.md — so they are deliberately
        // NOT exercised here with bad input (that would abort the test
        // process). Command functions (SetParent/ClearParent/
        // DestroyEntity), which use predictable rejection instead, are
        // exactly what this test covers.
        Scene scene;
        const EntityId valid = scene.CreateEntity("Valid");
        const EntityId neverCreated{9999};
        const EntityId invalid{}; // default-constructed sentinel

        Check(!scene.IsValid(invalid), "A default-constructed EntityId is never valid");
        Check(!scene.IsValid(neverCreated), "An id that was never returned by CreateEntity is not valid");

        Check(scene.SetParent(invalid, valid) == false, "SetParent rejects an invalid child id");
        Check(scene.SetParent(valid, invalid) == false, "SetParent rejects an invalid parent id");
        Check(scene.SetParent(neverCreated, valid) == false, "SetParent rejects an unknown child id");

        // Must not crash, and must not affect the valid entity.
        scene.ClearParent(invalid);
        scene.ClearParent(neverCreated);
        scene.DestroyEntity(invalid);
        scene.DestroyEntity(neverCreated);
        Check(scene.IsValid(valid), "Operating on invalid/unknown ids never affects unrelated valid entities");

        // A stale id (valid once, then destroyed) is handled the same
        // predictable way as one that was never created.
        const EntityId destroyed = scene.CreateEntity("WillBeDestroyed");
        scene.DestroyEntity(destroyed);
        Check(scene.SetParent(destroyed, valid) == false, "SetParent rejects a stale (destroyed) id");
    }

    // --- M12: Renderable + ExtractRenderables ---

    void TestSetAndGetRenderable()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        Check(!scene.HasRenderable(entity), "A freshly created entity has no renderable by default");
        Check(scene.GetRenderable(entity) == nullptr,
              "GetRenderable returns nullptr for a valid entity with no renderable set - not the same as an invalid entity");

        const Renderable renderable{MeshId{1}, Vec4(1.0f, 0.5f, 0.0f, 1.0f), true};
        scene.SetRenderable(entity, renderable);

        Check(scene.HasRenderable(entity), "HasRenderable is true after SetRenderable");
        const Renderable* stored = scene.GetRenderable(entity);
        Check(stored != nullptr, "GetRenderable returns non-null after SetRenderable");
        Check(stored->mesh == MeshId{1}, "Stored renderable's MeshId round-trips");
        Check(stored->tint == Vec4(1.0f, 0.5f, 0.0f, 1.0f), "Stored renderable's tint round-trips");

        scene.RemoveRenderable(entity);
        Check(!scene.HasRenderable(entity), "HasRenderable is false after RemoveRenderable");
        Check(scene.GetRenderable(entity) == nullptr, "GetRenderable returns nullptr after RemoveRenderable");
    }

    void TestRemoveRenderableNoOpsOnInvalidEntity()
    {
        Scene scene;
        scene.RemoveRenderable(EntityId{9999}); // never created
        scene.RemoveRenderable(EntityId{});      // default-constructed sentinel
        Check(true, "RemoveRenderable does not crash on an invalid/unknown EntityId (mirrors ClearParent)");
    }

    void TestExtractRenderablesIncludesOnlyRenderableEntities()
    {
        Scene scene;
        const EntityId withRenderable = scene.CreateEntity("WithRenderable");
        const EntityId withoutRenderable = scene.CreateEntity("WithoutRenderable");
        (void)withoutRenderable; // created only to prove ExtractRenderables ignores it - id itself unused
        scene.SetRenderable(withRenderable, Renderable{MeshId{7}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

        const std::vector<RenderableInstance> instances = scene.ExtractRenderables();
        Check(instances.size() == 1, "ExtractRenderables returns exactly one instance when only one entity has a renderable");
        Check(instances[0].entity == withRenderable, "The extracted instance references the correct entity");
    }

    void TestExtractRenderablesWorldTransformMatchesGetWorldMatrix()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        scene.GetTransform(entity).position = Vec3(4.0f, 5.0f, 6.0f);
        scene.SetRenderable(entity, Renderable{MeshId{1}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

        const std::vector<RenderableInstance> instances = scene.ExtractRenderables();
        Check(instances.size() == 1, "One renderable extracted");
        Check(instances[0].worldTransform == scene.GetWorldMatrix(entity),
              "Extracted worldTransform matches Scene::GetWorldMatrix for the same entity");
    }

    void TestExtractRenderablesSkipsInvisible()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        scene.SetRenderable(entity, Renderable{MeshId{1}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), /*visible=*/false});

        Check(scene.ExtractRenderables().empty(), "ExtractRenderables skips an entity whose Renderable::visible is false");
    }

    void TestExtractRenderablesSkipsDestroyedEntity()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        scene.SetRenderable(entity, Renderable{MeshId{1}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});
        Check(scene.ExtractRenderables().size() == 1, "Renderable extracted before destruction");

        scene.DestroyEntity(entity);
        Check(scene.ExtractRenderables().empty(), "A destroyed entity's renderable no longer appears in extraction");
    }

    void TestExtractRenderablesSkipsRecursivelyDestroyedDescendants()
    {
        Scene scene;
        const EntityId parent = scene.CreateEntity("Parent");
        const EntityId child = scene.CreateEntity("Child");
        scene.SetParent(child, parent);
        scene.SetRenderable(parent, Renderable{MeshId{1}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});
        scene.SetRenderable(child, Renderable{MeshId{2}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});
        Check(scene.ExtractRenderables().size() == 2, "Both parent and child renderables extracted before destruction");

        scene.DestroyEntity(parent);
        Check(scene.ExtractRenderables().empty(),
              "Destroying a parent recursively removes the child's renderable from extraction too - no dangling mapping");
    }

    void TestExtractRenderablesChildWorldTransformIncludesParent()
    {
        Scene scene;
        const EntityId parent = scene.CreateEntity("Parent");
        const EntityId child = scene.CreateEntity("Child");
        scene.GetTransform(parent).position = Vec3(10.0f, 0.0f, 0.0f);
        scene.GetTransform(child).position = Vec3(0.0f, 5.0f, 0.0f);
        scene.SetParent(child, parent);
        scene.SetRenderable(child, Renderable{MeshId{3}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

        const std::vector<RenderableInstance> instances = scene.ExtractRenderables();
        Check(instances.size() == 1, "Only the child (which has a renderable) is extracted");
        const Vec3 worldOrigin = TransformPoint(instances[0].worldTransform, Vec3(0.0f, 0.0f, 0.0f));
        Check(worldOrigin == Vec3(10.0f, 5.0f, 0.0f),
              "Extracted child world transform == parentWorld * childLocal, matching Scene::GetWorldMatrix");
    }

    void TestExtractRenderablesSameMeshIdMultipleInstances()
    {
        Scene scene;
        const EntityId a = scene.CreateEntity("A");
        const EntityId b = scene.CreateEntity("B");
        const MeshId sharedMesh{42};
        scene.SetRenderable(a, Renderable{sharedMesh, Vec4(1.0f, 0.0f, 0.0f, 1.0f), true});
        scene.SetRenderable(b, Renderable{sharedMesh, Vec4(0.0f, 1.0f, 0.0f, 1.0f), true});

        const std::vector<RenderableInstance> instances = scene.ExtractRenderables();
        Check(instances.size() == 2, "Two entities sharing the same MeshId both extract independently");
        Check(instances[0].mesh == sharedMesh && instances[1].mesh == sharedMesh,
              "Both extracted instances reference the same shared MeshId - one mesh, multiple instances");
    }

    void TestExtractRenderablesReflectsTransformUpdateOnNextCall()
    {
        Scene scene;
        const EntityId entity = scene.CreateEntity();
        scene.SetRenderable(entity, Renderable{MeshId{1}, Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

        scene.GetTransform(entity).position = Vec3(1.0f, 0.0f, 0.0f);
        const Vec3 worldA = TransformPoint(scene.ExtractRenderables()[0].worldTransform, Vec3(0.0f, 0.0f, 0.0f));
        Check(worldA == Vec3(1.0f, 0.0f, 0.0f), "First extraction reflects the entity's transform at that time");

        scene.GetTransform(entity).position = Vec3(2.0f, 0.0f, 0.0f);
        const Vec3 worldB = TransformPoint(scene.ExtractRenderables()[0].worldTransform, Vec3(0.0f, 0.0f, 0.0f));
        Check(worldB == Vec3(2.0f, 0.0f, 0.0f),
              "A second extraction after moving the entity reflects the new transform - no stale caching");
    }
}

int main()
{
    TestEntityCreation();
    TestDistinctIds();
    TestDestroyedEntityBecomesInvalid();
    TestDefaultTransform();
    TestTransformStorage();
    TestTransformToMatrixMatchesMat4TRS();
    TestTransformDefaultForwardRightUp();
    TestTransformForwardAfterYaw();
    TestCameraDefaults();
    TestCameraSetAspectRatio();
    TestCameraViewMatrixFromTransform();
    TestRootWorldMatrixEqualsLocal();
    TestParentChildComposition();
    TestThreeLevelHierarchy();
    TestParentChildMultiplicationOrderObvious();
    TestSetParentAndClearParent();
    TestReparentingLeavesLocalTransformUnchanged();
    TestSelfParentingRejected();
    TestCycleRejected();
    TestDestroyParentDestroysDescendants();
    TestInvalidEntityAccessIsPredictable();

    TestSetAndGetRenderable();
    TestRemoveRenderableNoOpsOnInvalidEntity();
    TestExtractRenderablesIncludesOnlyRenderableEntities();
    TestExtractRenderablesWorldTransformMatchesGetWorldMatrix();
    TestExtractRenderablesSkipsInvisible();
    TestExtractRenderablesSkipsDestroyedEntity();
    TestExtractRenderablesSkipsRecursivelyDestroyedDescendants();
    TestExtractRenderablesChildWorldTransformIncludesParent();
    TestExtractRenderablesSameMeshIdMultipleInstances();
    TestExtractRenderablesReflectsTransformUpdateOnNextCall();

    if (g_failureCount == 0)
    {
        std::printf("All Scene checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
