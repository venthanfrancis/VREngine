#include "VulkanRenderResourceContext.hpp"

#include "VulkanImage.hpp"
#include "VulkanMesh.hpp"

#include "AREngine/Core/Assert.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanRenderResourceContext::VulkanRenderResourceContext(
        VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue,
        VkDescriptorSetLayout descriptorSetLayout, std::uint32_t maxMaterials)
        : m_physicalDevice(physicalDevice)
        , m_device(device)
        , m_commandPool(commandPool)
        , m_queue(queue)
        , m_descriptorSetLayout(descriptorSetLayout)
        , m_sampler(device)
        , m_descriptorPool(device, maxMaterials)
    {
    }

    // Declared out-of-line specifically so this translation unit - which
    // includes VulkanImage.hpp/VulkanMesh.hpp - is the one that
    // instantiates std::unique_ptr<VulkanImage>/<VulkanMesh>'s
    // destructors, not every file that merely includes
    // VulkanRenderResourceContext.hpp (which only forward-declares them).
    VulkanRenderResourceContext::~VulkanRenderResourceContext() = default;

    MeshHandle VulkanRenderResourceContext::CreateMesh(Assets::AssetId assetId, const Assets::MeshAsset& meshAsset)
    {
        const MeshHandle handle = m_meshHandles.GetOrCreate(assetId);
        if (m_meshes.contains(handle))
        {
            return handle; // cache hit - zero Vulkan calls
        }

        MeshData meshData;
        meshData.vertices.reserve(meshAsset.vertices.size());
        for (const Assets::MeshVertexData& assetVertex : meshAsset.vertices)
        {
            meshData.vertices.push_back(MeshVertex{assetVertex.position, assetVertex.color, assetVertex.uv});
        }
        meshData.indices = meshAsset.indices;

        m_meshes.emplace(handle, CreateVulkanMesh(m_physicalDevice, m_device, m_commandPool, m_queue, meshData));
        return handle;
    }

    MeshHandle VulkanRenderResourceContext::CreateProceduralMesh(const MeshData& meshData)
    {
        const MeshHandle handle = m_meshHandles.CreateNew();
        m_meshes.emplace(handle, CreateVulkanMesh(m_physicalDevice, m_device, m_commandPool, m_queue, meshData));
        return handle;
    }

    const VulkanMesh* VulkanRenderResourceContext::GetMesh(MeshHandle handle) const
    {
        const auto it = m_meshes.find(handle);
        return it != m_meshes.end() ? it->second.get() : nullptr;
    }

    void VulkanRenderResourceContext::CreateTexture(Assets::AssetId assetId, const Assets::TextureAsset& textureAsset)
    {
        if (m_textures.contains(assetId))
        {
            return; // cache hit - zero Vulkan calls
        }

        m_textures.emplace(assetId, CreateTextureFromPixels(
            m_physicalDevice, m_device, m_commandPool, m_queue,
            textureAsset.width, textureAsset.height, textureAsset.pixels.data()));
    }

    MaterialHandle VulkanRenderResourceContext::CreateMaterial(Assets::AssetId textureAssetId)
    {
        const auto textureIt = m_textures.find(textureAssetId);
        AR_ASSERT_MSG(textureIt != m_textures.end(),
            "CreateMaterial called with a textureAssetId that was never passed to CreateTexture - setup-order bug");

        const VkDescriptorSet descriptorSet = m_descriptorPool.Allocate(m_descriptorSetLayout);
        WriteCombinedImageSamplerDescriptor(m_device, descriptorSet, textureIt->second->GetView(), m_sampler.Get());

        const MaterialHandle handle = m_materialHandles.CreateNew();
        m_materials.emplace(handle, descriptorSet);
        return handle;
    }

    VkDescriptorSet VulkanRenderResourceContext::GetMaterial(MaterialHandle handle) const
    {
        const auto it = m_materials.find(handle);
        return it != m_materials.end() ? it->second : VK_NULL_HANDLE;
    }
}
