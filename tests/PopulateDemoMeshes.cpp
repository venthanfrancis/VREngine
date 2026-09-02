#include "PopulateDemoMeshes.hpp"

#include "AREngine/Core/Assert.hpp"

namespace ARDemo
{
    AREngine::Scene::MeshId PopulateDemoMeshes(
        AREngine::Assets::AssetManager& assetManager,
        MeshCache& meshCache,
        MeshRegistry& meshRegistry,
        VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue)
    {
        const std::optional<AREngine::Assets::AssetId> assetId = assetManager.LoadMesh("meshes/pyramid.obj");
        AR_ASSERT_MSG(assetId.has_value(), "Failed to load the committed demo mesh asset meshes/pyramid.obj - packaging/asset-root bug");

        const AREngine::Assets::MeshAsset& meshAsset = assetManager.GetMesh(*assetId);
        const AREngine::Rendering::Vulkan::VulkanMesh* mesh = meshCache.GetOrCreate(
            *assetId, meshAsset, physicalDevice, device, commandPool, queue);

        const AREngine::Scene::MeshId pyramidMeshId{1};
        meshRegistry.Register(pyramidMeshId, mesh);

        return pyramidMeshId;
    }
}
