// M17 pure-logic tests for ARDemo::BuildRenderItems (BuildRenderItems.hpp) -
// the Scene <-> Rendering integration conversion step. Built
// unconditionally, not gated behind ARENGINE_ENABLE_VULKAN: this file
// depends only on AREngine::Core/AREngine::Scene/AREngine::Rendering's
// public headers, no Vulkan/OpenXR type anywhere. See
// docs/ARCHITECTURE.md, "M17 - Scene Render Submission Foundation".
//
// "No ViewInfo required to build world RenderItems" is satisfied by
// BuildRenderItems's own signature (it takes no view/projection
// parameter at all) - not retested here as a runtime check, since that
// would be vacuous. "Unresolved mesh/material skipped" and "view
// multiplication" both require a real VulkanRenderResourceContext /
// caller loop respectively - neither is an isolable pure-logic unit in
// this design (see SubmitRenderItems, verified manually through the
// demos' own draw-count diagnostics instead).

#include "BuildRenderItems.hpp"

#include "AREngine/Scene/EntityId.hpp"

#include <array>
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

    using namespace AREngine;
    using namespace AREngine::Core::Math;

    Scene::RenderableInstance MakeRenderable(
        std::uint64_t entityId, const Mat4& worldTransform,
        std::uint64_t meshId, std::uint64_t materialId, const Vec4& tint)
    {
        return Scene::RenderableInstance{
            Scene::EntityId{entityId}, worldTransform, Scene::MeshId{meshId}, Scene::MaterialId{materialId}, tint};
    }

    void TestConversionPreservesModelMatrix()
    {
        const Mat4 world = Mat4::Translation(Vec3(1.0f, 2.0f, 3.0f));
        const Scene::RenderableInstance renderable = MakeRenderable(1, world, 7, 9, Vec4(1.0f, 1.0f, 1.0f, 1.0f));

        const std::vector<Rendering::RenderItem> items = ARDemo::BuildRenderItems(std::span(&renderable, 1));

        Check(items.size() == 1, "One renderable produces exactly one RenderItem");
        Check(items[0].model == world, "The RenderItem's model matrix matches the source worldTransform exactly");
    }

    void TestConversionPreservesTint()
    {
        const Vec4 tint(0.25f, 0.5f, 0.75f, 1.0f);
        const Scene::RenderableInstance renderable = MakeRenderable(1, Mat4::Identity(), 1, 1, tint);

        const std::vector<Rendering::RenderItem> items = ARDemo::BuildRenderItems(std::span(&renderable, 1));

        Check(items[0].tint == tint, "The RenderItem's tint matches the source tint exactly");
    }

    void TestConversionPreservesMeshHandle()
    {
        const Scene::RenderableInstance renderable = MakeRenderable(1, Mat4::Identity(), 42, 1, Vec4());

        const std::vector<Rendering::RenderItem> items = ARDemo::BuildRenderItems(std::span(&renderable, 1));

        Check(items[0].mesh == Rendering::MeshHandle{42},
              "Scene::MeshId is converted to a Rendering::MeshHandle with the identical underlying value");
    }

    void TestConversionPreservesMaterialHandle()
    {
        const Scene::RenderableInstance renderable = MakeRenderable(1, Mat4::Identity(), 1, 99, Vec4());

        const std::vector<Rendering::RenderItem> items = ARDemo::BuildRenderItems(std::span(&renderable, 1));

        Check(items[0].material == Rendering::MaterialHandle{99},
              "Scene::MaterialId is converted to a Rendering::MaterialHandle with the identical underlying value");
    }

    void TestTransformUpdateProducesUpdatedModel()
    {
        // Frame N
        const Scene::RenderableInstance frameN = MakeRenderable(1, Mat4::Translation(Vec3(0.0f, 0.0f, 0.0f)), 1, 1, Vec4());
        const std::vector<Rendering::RenderItem> itemsN = ARDemo::BuildRenderItems(std::span(&frameN, 1));

        // Frame N+1 - a fresh RenderableInstance with an updated transform, exactly as a fresh
        // Scene::ExtractRenderables() call would produce every frame.
        const Mat4 updatedWorld = Mat4::Translation(Vec3(5.0f, 0.0f, 0.0f));
        const Scene::RenderableInstance frameNPlus1 = MakeRenderable(1, updatedWorld, 1, 1, Vec4());
        const std::vector<Rendering::RenderItem> itemsNPlus1 = ARDemo::BuildRenderItems(std::span(&frameNPlus1, 1));

        Check(!(itemsN[0].model == itemsNPlus1[0].model), "A changed worldTransform between frames produces a different RenderItem::model - no stale submission data");
        Check(itemsNPlus1[0].model == updatedWorld, "The new frame's RenderItem::model reflects exactly the updated transform");
    }

    void TestSourceRenderableIsNotMutated()
    {
        const Mat4 world = Mat4::Translation(Vec3(1.0f, 1.0f, 1.0f));
        const Scene::MeshId originalMesh{3};
        const Scene::MaterialId originalMaterial{4};
        const Vec4 originalTint(0.1f, 0.2f, 0.3f, 0.4f);
        const Scene::RenderableInstance renderable = MakeRenderable(1, world, originalMesh.id, originalMaterial.id, originalTint);

        (void)ARDemo::BuildRenderItems(std::span(&renderable, 1));

        Check(renderable.mesh == originalMesh && renderable.material == originalMaterial &&
              renderable.tint == originalTint && renderable.worldTransform == world,
              "BuildRenderItems takes renderables by const span and never mutates the source Scene data");
    }

    void TestMultipleRenderablesConvertIndependently()
    {
        const Scene::RenderableInstance a = MakeRenderable(1, Mat4::Translation(Vec3(1.0f, 0.0f, 0.0f)), 10, 20, Vec4(1, 0, 0, 1));
        const Scene::RenderableInstance b = MakeRenderable(2, Mat4::Translation(Vec3(2.0f, 0.0f, 0.0f)), 30, 40, Vec4(0, 1, 0, 1));
        const std::array<Scene::RenderableInstance, 2> renderables{a, b};

        const std::vector<Rendering::RenderItem> items = ARDemo::BuildRenderItems(renderables);

        Check(items.size() == 2, "Two renderables produce exactly two RenderItems, in order");
        Check(items[0].mesh == Rendering::MeshHandle{10} && items[1].mesh == Rendering::MeshHandle{30},
              "Each RenderItem's mesh handle matches its own source renderable, not a mixed-up one");
    }

    void TestEmptyInputProducesEmptyOutput()
    {
        const std::vector<Rendering::RenderItem> items = ARDemo::BuildRenderItems({});
        Check(items.empty(), "Zero renderables produce zero RenderItems");
    }
}

int main()
{
    TestConversionPreservesModelMatrix();
    TestConversionPreservesTint();
    TestConversionPreservesMeshHandle();
    TestConversionPreservesMaterialHandle();
    TestTransformUpdateProducesUpdatedModel();
    TestSourceRenderableIsNotMutated();
    TestMultipleRenderablesConvertIndependently();
    TestEmptyInputProducesEmptyOutput();

    if (g_failureCount == 0)
    {
        std::printf("All M17 BuildRenderItems checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
