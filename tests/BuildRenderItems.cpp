#include "BuildRenderItems.hpp"

namespace ARDemo
{
    std::vector<AREngine::Rendering::RenderItem> BuildRenderItems(
        std::span<const AREngine::Scene::RenderableInstance> renderables)
    {
        std::vector<AREngine::Rendering::RenderItem> items;
        items.reserve(renderables.size());

        for (const AREngine::Scene::RenderableInstance& renderable : renderables)
        {
            items.push_back(AREngine::Rendering::RenderItem{
                AREngine::Rendering::MeshHandle{renderable.mesh.id},
                AREngine::Rendering::MaterialHandle{renderable.material.id},
                renderable.worldTransform,
                renderable.tint});
        }

        return items;
    }
}
