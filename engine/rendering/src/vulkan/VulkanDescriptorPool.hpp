#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkDescriptorPool sized for exactly what M8E needs: one
    // combined-image-sampler descriptor, one descriptor set, ever. Not
    // a generic descriptor manager - no growth, no recycling, no per-
    // frame allocation. Created WITHOUT
    // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, so the one set
    // allocated from it is freed implicitly when the pool itself is
    // destroyed, not individually - the simplest option for "one
    // texture, one descriptor set, one draw, for the whole demo's
    // lifetime." See docs/ARCHITECTURE.md, "Descriptor Pool/Set
    // Ownership (M8E)".
    //
    // Not copyable or movable: exactly one VkDescriptorPool per
    // VulkanDescriptorPool, destroyed exactly once, by this object
    // alone.
    class VulkanDescriptorPool
    {
    public:
        explicit VulkanDescriptorPool(VkDevice device);
        ~VulkanDescriptorPool();

        VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool(VulkanDescriptorPool&&) = delete;
        VulkanDescriptorPool& operator=(VulkanDescriptorPool&&) = delete;

        // Allocates one descriptor set matching `layout` from this
        // pool. The returned VkDescriptorSet is owned by the pool, not
        // by the caller - it stays valid until this VulkanDescriptorPool
        // is destroyed, and is never individually freed.
        [[nodiscard]] VkDescriptorSet Allocate(VkDescriptorSetLayout layout);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorPool m_pool = VK_NULL_HANDLE;
    };

    // Points binding 0 of `descriptorSet` at `imageView`/`sampler`,
    // expecting VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (the layout
    // CreateTextureFromPixels leaves the image in). One
    // vkUpdateDescriptorSets call, no batching - M8E writes exactly one
    // descriptor, once.
    void WriteCombinedImageSamplerDescriptor(
        VkDevice device, VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler sampler);
}
