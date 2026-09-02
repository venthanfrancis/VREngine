#include "RenderDrawPlanning.hpp"

namespace ARDemo
{
    std::vector<PlannedDraw> BuildDrawPlan(
        std::span<const AREngine::Scene::RenderableInstance> renderables,
        std::span<const AREngine::Core::Math::Mat4> viewProjections)
    {
        std::vector<PlannedDraw> plan;
        plan.reserve(renderables.size() * viewProjections.size());

        for (std::size_t viewIndex = 0; viewIndex < viewProjections.size(); ++viewIndex)
        {
            for (const AREngine::Scene::RenderableInstance& renderable : renderables)
            {
                plan.push_back(PlannedDraw{
                    viewIndex, viewProjections[viewIndex] * renderable.worldTransform,
                    renderable.mesh, renderable.material, renderable.tint});
            }
        }

        return plan;
    }
}
