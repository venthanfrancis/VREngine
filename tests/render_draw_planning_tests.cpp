// Automated tests for ARDemo::BuildDrawPlan (RenderDrawPlanning.hpp) —
// M12. Pure logic only: no Vulkan, no OpenXR, no window, no GPU. Proves
// "extract once, render against N views" generically, with no hardcoded
// eye-count assumption anywhere.

#include "RenderDrawPlanning.hpp"

#include "AREngine/Core/Math/MathUtil.hpp"

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

    using namespace AREngine::Core::Math;
    using namespace AREngine::Scene;
    using namespace ARDemo;

    std::vector<RenderableInstance> MakeRenderables(std::size_t count)
    {
        std::vector<RenderableInstance> renderables;
        renderables.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            renderables.push_back(RenderableInstance{
                EntityId{i + 1}, Mat4::Translation(Vec3(static_cast<float>(i), 0.0f, 0.0f)),
                MeshId{1}, Vec4(1.0f, 1.0f, 1.0f, 1.0f)});
        }
        return renderables;
    }

    std::vector<Mat4> MakeIdentityViews(std::size_t count)
    {
        return std::vector<Mat4>(count, Mat4::Identity());
    }

    void TestEmptyInputsProduceNoDraws()
    {
        const std::vector<PlannedDraw> plan = BuildDrawPlan({}, {});
        Check(plan.empty(), "Zero renderables and zero views produce zero planned draws");
    }

    void TestOneRenderableOneView()
    {
        const std::vector<RenderableInstance> renderables = MakeRenderables(1);
        const std::vector<Mat4> views = MakeIdentityViews(1);
        const std::vector<PlannedDraw> plan = BuildDrawPlan(renderables, views);

        Check(plan.size() == 1, "1 renderable x 1 view produces exactly 1 planned draw");
        Check(plan[0].viewIndex == 0, "The single planned draw targets view 0");
        Check(plan[0].mesh == MeshId{1}, "The planned draw carries the renderable's MeshId");
    }

    void TestFiveRenderablesOneView()
    {
        const std::vector<PlannedDraw> plan = BuildDrawPlan(MakeRenderables(5), MakeIdentityViews(1));
        Check(plan.size() == 5, "5 renderables x 1 view produces exactly 5 planned draws");
    }

    void TestFiveRenderablesTwoViews()
    {
        const std::vector<PlannedDraw> plan = BuildDrawPlan(MakeRenderables(5), MakeIdentityViews(2));
        Check(plan.size() == 10, "5 renderables x 2 views produces exactly 10 planned draws - no stereo-only assumption");
    }

    void TestFiveRenderablesThreeViews()
    {
        const std::vector<PlannedDraw> plan = BuildDrawPlan(MakeRenderables(5), MakeIdentityViews(3));
        Check(plan.size() == 15,
              "5 renderables x 3 views produces exactly 15 planned draws - never assumes exactly 1 or 2 views");
    }

    void TestPlanIsOrderedByViewThenRenderable()
    {
        const std::vector<PlannedDraw> plan = BuildDrawPlan(MakeRenderables(2), MakeIdentityViews(2));
        Check(plan.size() == 4, "2 renderables x 2 views produces 4 planned draws");
        Check(plan[0].viewIndex == 0 && plan[1].viewIndex == 0, "First two planned draws target view 0");
        Check(plan[2].viewIndex == 1 && plan[3].viewIndex == 1, "Last two planned draws target view 1");
    }

    void TestMvpCombinesViewProjectionAndWorldTransform()
    {
        std::vector<RenderableInstance> renderables;
        renderables.push_back(RenderableInstance{
            EntityId{1}, Mat4::Translation(Vec3(1.0f, 0.0f, 0.0f)), MeshId{1}, Vec4(1.0f, 1.0f, 1.0f, 1.0f)});

        std::vector<Mat4> views;
        views.push_back(Mat4::Translation(Vec3(0.0f, 10.0f, 0.0f)));

        const std::vector<PlannedDraw> plan = BuildDrawPlan(renderables, views);
        Check(plan.size() == 1, "One planned draw produced");

        const Vec3 origin = TransformPoint(plan[0].mvp, Vec3(0.0f, 0.0f, 0.0f));
        Check(origin == Vec3(1.0f, 10.0f, 0.0f),
              "Planned draw's mvp == viewProjection * worldTransform, applied in that order");
    }

    void TestTintPreservedInPlannedDraw()
    {
        std::vector<RenderableInstance> renderables;
        renderables.push_back(RenderableInstance{
            EntityId{1}, Mat4::Identity(), MeshId{1}, Vec4(0.2f, 0.4f, 0.6f, 1.0f)});

        const std::vector<PlannedDraw> plan = BuildDrawPlan(renderables, MakeIdentityViews(1));
        Check(plan.size() == 1 && plan[0].tint == Vec4(0.2f, 0.4f, 0.6f, 1.0f),
              "Planned draw's tint matches the source renderable's tint - unchanged by draw planning");
    }
}

int main()
{
    TestEmptyInputsProduceNoDraws();
    TestOneRenderableOneView();
    TestFiveRenderablesOneView();
    TestFiveRenderablesTwoViews();
    TestFiveRenderablesThreeViews();
    TestPlanIsOrderedByViewThenRenderable();
    TestMvpCombinesViewProjectionAndWorldTransform();
    TestTintPreservedInPlannedDraw();

    if (g_failureCount == 0)
    {
        std::printf("All render draw planning checks passed\n");
        return 0;
    }

    std::fprintf(stderr, "%d check(s) failed\n", g_failureCount);
    return 1;
}
