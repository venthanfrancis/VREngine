#include "PopulateDemoMeshes.hpp"

#include "AREngine/Core/Assert.hpp"

namespace ARDemo
{
    AREngine::Scene::MeshId PopulateDemoMeshes(
        AREngine::Assets::AssetManager& assetManager,
        AREngine::Rendering::Vulkan::VulkanRenderResourceContext& context)
    {
        const std::optional<AREngine::Assets::AssetId> assetId = assetManager.LoadMesh("meshes/pyramid.obj");
        AR_ASSERT_MSG(assetId.has_value(), "Failed to load the committed demo mesh asset meshes/pyramid.obj - packaging/asset-root bug");

        const AREngine::Assets::MeshAsset& meshAsset = assetManager.GetMesh(*assetId);
        const AREngine::Rendering::MeshHandle handle = context.CreateMesh(*assetId, meshAsset);

        return AREngine::Scene::MeshId{handle.id};
    }
}
