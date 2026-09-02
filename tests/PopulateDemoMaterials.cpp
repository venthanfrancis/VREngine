#include "PopulateDemoMaterials.hpp"

#include "AREngine/Core/Assert.hpp"

namespace ARDemo
{
    namespace
    {
        // Loads+resolves one texture asset by relative path and
        // registers it into `materialRegistry` under `materialId`.
        // Returns false (does not register anything) if the asset
        // fails to load - the caller decides how to react; M14's demos
        // treat this as a setup-time hard failure (AR_ASSERT), since a
        // missing committed test/demo fixture is a build/packaging bug,
        // not a normal runtime condition.
        bool LoadAndRegisterMaterial(
            AREngine::Assets::AssetManager& assetManager, TextureCache& textureCache,
            const AREngine::Rendering::Vulkan::VulkanDescriptorSetLayout& descriptorSetLayout,
            AREngine::Rendering::Vulkan::VulkanDescriptorPool& descriptorPool,
            const AREngine::Rendering::Vulkan::VulkanSampler& sampler,
            MaterialRegistry& materialRegistry,
            VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue,
            const std::filesystem::path& relativePath, AREngine::Scene::MaterialId materialId)
        {
            const std::optional<AREngine::Assets::AssetId> assetId = assetManager.LoadTexture(relativePath);
            if (!assetId.has_value())
            {
                return false;
            }

            const AREngine::Assets::TextureAsset& imageAsset = assetManager.GetTexture(*assetId);
            const AREngine::Rendering::Vulkan::VulkanImage* texture = textureCache.GetOrCreate(
                *assetId, imageAsset, physicalDevice, device, commandPool, queue);

            const VkDescriptorSet descriptorSet = descriptorPool.Allocate(descriptorSetLayout.Get());
            AREngine::Rendering::Vulkan::WriteCombinedImageSamplerDescriptor(
                device, descriptorSet, texture->GetView(), sampler.Get());

            materialRegistry.Register(materialId, descriptorSet);
            return true;
        }
    }

    DemoMaterialIds PopulateDemoMaterials(
        AREngine::Assets::AssetManager& assetManager,
        TextureCache& textureCache,
        const AREngine::Rendering::Vulkan::VulkanDescriptorSetLayout& descriptorSetLayout,
        AREngine::Rendering::Vulkan::VulkanDescriptorPool& descriptorPool,
        const AREngine::Rendering::Vulkan::VulkanSampler& sampler,
        MaterialRegistry& materialRegistry,
        VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool commandPool, VkQueue queue)
    {
        const DemoMaterialIds materialIds{AREngine::Scene::MaterialId{1}, AREngine::Scene::MaterialId{2}};

        const bool redOk = LoadAndRegisterMaterial(
            assetManager, textureCache, descriptorSetLayout, descriptorPool, sampler, materialRegistry,
            physicalDevice, device, commandPool, queue, "textures/checker_red.png", materialIds.redChecker);
        AR_ASSERT_MSG(redOk, "Failed to load the committed demo texture asset textures/checker_red.png - packaging/asset-root bug");

        const bool blueOk = LoadAndRegisterMaterial(
            assetManager, textureCache, descriptorSetLayout, descriptorPool, sampler, materialRegistry,
            physicalDevice, device, commandPool, queue, "textures/checker_blue.png", materialIds.blueChecker);
        AR_ASSERT_MSG(blueOk, "Failed to load the committed demo texture asset textures/checker_blue.png - packaging/asset-root bug");

        return materialIds;
    }
}
