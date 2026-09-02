#include "MeshCache.hpp"

#include "AREngine/Rendering/MeshData.hpp"

namespace ARDemo
{
    const AREngine::Rendering::Vulkan::VulkanMesh* MeshCache::GetOrCreate(
        AREngine::Assets::AssetId assetId, const AREngine::Assets::MeshAsset& meshAsset,
        VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue)
    {
        const auto it = m_meshes.find(assetId);
        if (it != m_meshes.end())
        {
            return it->second.get(); // cache hit - zero Vulkan calls
        }

        AREngine::Rendering::MeshData meshData;
        meshData.vertices.reserve(meshAsset.vertices.size());
        for (const AREngine::Assets::MeshVertexData& assetVertex : meshAsset.vertices)
        {
            meshData.vertices.push_back(AREngine::Rendering::MeshVertex{
                assetVertex.position, assetVertex.color, assetVertex.uv});
        }
        meshData.indices = meshAsset.indices;

        auto mesh = AREngine::Rendering::Vulkan::CreateVulkanMesh(physicalDevice, device, commandPool, queue, meshData);

        const AREngine::Rendering::Vulkan::VulkanMesh* result = mesh.get();
        m_meshes.emplace(assetId, std::move(mesh));
        return result;
    }
}
