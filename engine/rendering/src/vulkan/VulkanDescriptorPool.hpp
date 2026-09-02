#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkDescriptorPool sized for exactly `maxSets` combined-
    // image-sampler descriptors/sets, ever - not a generic descriptor
    // manager, no growth, no recycling, no per-frame allocation.
    // Defaults to 1 (M8E's original "one texture, one descriptor set,
    // one draw, for the whole demo's lifetime" - see
    // docs/ARCHITECTURE.md, "Descriptor Pool/Set Ownership (M8E)"), so
    // every pre-M13 call site is unaffected; M13 passes an explicit
    // material count instead (one descriptor set per material - see
    // "M13 - Material & Render Resource Binding Foundation"). Created
    // WITHOUT VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, so
    // every set allocated from it is freed implicitly, all together,
    // when the pool itself is destroyed - never individually.
    //
    // Not copyable or movable: exactly one VkDescriptorPool per
    // VulkanDescriptorPool, destroyed exactly once, by this object
    // alone.
    class VulkanDescriptorPool
    {
    public:
        explicit VulkanDescriptorPool(VkDevice device, std::uint32_t maxSets = 1);
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
