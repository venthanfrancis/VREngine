#include "PopulateDemoScene.hpp"

#include "AREngine/Scene/Renderable.hpp"

namespace ARDemo
{
    DemoSceneEntities PopulateDemoScene(
        AREngine::Scene::Scene& scene, const DemoMeshIds& meshIds, const DemoMaterialIds& materialIds)
    {
        namespace Math = AREngine::Core::Math;
        using AREngine::Scene::Renderable;

        DemoSceneEntities entities;

        entities.floor = scene.CreateEntity("Floor");
        scene.GetTransform(entities.floor).position = Math::Vec3(0.0f, -0.6f, -2.0f);
        scene.GetTransform(entities.floor).rotation =
            Math::Quaternion::FromAxisAngle(Math::Vec3(1.0f, 0.0f, 0.0f), -1.5707963f); // -90 deg: quad +Z -> world +Y
        scene.GetTransform(entities.floor).scale = Math::Vec3(4.0f, 4.0f, 4.0f);
        scene.SetRenderable(entities.floor,
            Renderable{meshIds.floor, materialIds.redChecker, Math::Vec4(0.6f, 0.6f, 0.6f, 1.0f), true});

        entities.referenceCube = scene.CreateEntity("ReferenceCube");
        scene.GetTransform(entities.referenceCube).position = Math::Vec3(0.0f, 0.0f, -2.0f);
        scene.GetTransform(entities.referenceCube).scale = Math::Vec3(0.6f, 0.6f, 0.6f);
        scene.SetRenderable(entities.referenceCube,
            Renderable{meshIds.cube, materialIds.redChecker, Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f), true});

        // Same MeshId as referenceCube, but a DIFFERENT material - the
        // "same mesh, different materials" proof.
        entities.cubeA = scene.CreateEntity("CubeA");
        scene.GetTransform(entities.cubeA).position = Math::Vec3(-0.8f, 0.0f, -2.2f);
        scene.GetTransform(entities.cubeA).scale = Math::Vec3(0.3f, 0.3f, 0.3f);
        scene.SetRenderable(entities.cubeA,
            Renderable{meshIds.cube, materialIds.blueChecker, Math::Vec4(1.0f, 0.4f, 0.4f, 1.0f), true});

        // CubeB is a CHILD of referenceCube - M12's hierarchy proof. Its
        // local position is chosen so its initial WORLD position
        // ((0.8, 0, -2.2)) is unchanged from before it had a parent:
        // referenceCube's own world position is (0, 0, -2), so a local
        // offset of (0.8, 0, -0.2) composes back to the same place. Once
        // referenceCube's existing slow rotation animation runs, cubeB
        // visibly swings through world space with it.
        entities.cubeB = scene.CreateEntity("CubeB");
        scene.GetTransform(entities.cubeB).position = Math::Vec3(0.8f, 0.0f, -0.2f);
        scene.GetTransform(entities.cubeB).scale = Math::Vec3(0.3f, 0.3f, 0.3f);
        scene.SetRenderable(entities.cubeB,
            Renderable{meshIds.cube, materialIds.redChecker, Math::Vec4(0.4f, 1.0f, 0.4f, 1.0f), true});
        scene.SetParent(entities.cubeB, entities.referenceCube);

        entities.moveOffsetCube = scene.CreateEntity("MoveOffsetCube");
        scene.GetTransform(entities.moveOffsetCube).position = Math::Vec3(0.0f, 0.6f, -2.5f);
        scene.GetTransform(entities.moveOffsetCube).scale = Math::Vec3(0.3f, 0.3f, 0.3f);
        scene.SetRenderable(entities.moveOffsetCube,
            Renderable{meshIds.cube, materialIds.blueChecker, Math::Vec4(0.4f, 0.4f, 1.0f, 1.0f), true});

        return entities;
    }
}
