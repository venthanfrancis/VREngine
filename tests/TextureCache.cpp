#include "TextureCache.hpp"

namespace ARDemo
{
    const AREngine::Rendering::Vulkan::VulkanImage* TextureCache::GetOrCreate(
        AREngine::Assets::AssetId assetId, const AREngine::Assets::TextureAsset& imageAsset,
        VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue)
    {
        const auto it = m_textures.find(assetId);
        if (it != m_textures.end())
        {
            return it->second.get(); // cache hit - zero Vulkan calls
        }

        auto texture = AREngine::Rendering::Vulkan::CreateTextureFromPixels(
            physicalDevice, device, commandPool, queue,
            imageAsset.width, imageAsset.height, imageAsset.pixels.data());

        const AREngine::Rendering::Vulkan::VulkanImage* result = texture.get();
        m_textures.emplace(assetId, std::move(texture));
        return result;
    }
}
