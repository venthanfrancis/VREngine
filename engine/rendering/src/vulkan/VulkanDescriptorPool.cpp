#include "VulkanDescriptorPool.hpp"

#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanDescriptorPool::VulkanDescriptorPool(VkDevice device)
        : m_device(device)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.poolSizeCount = 1;
        createInfo.pPoolSizes = &poolSize;
        createInfo.maxSets = 1;

        const VkResult result = vkCreateDescriptorPool(device, &createInfo, nullptr, &m_pool);
        CheckVkResult(result, "vkCreateDescriptorPool");
    }

    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        // Destroying the pool implicitly frees every descriptor set
        // allocated from it - this pool was created without
        // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, so there is
        // no separate vkFreeDescriptorSets call to make.
        if (m_pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(m_device, m_pool, nullptr);
            m_pool = VK_NULL_HANDLE;
        }
    }

    VkDescriptorSet VulkanDescriptorPool::Allocate(VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        const VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, &descriptorSet);
        CheckVkResult(result, "vkAllocateDescriptorSets");
        return descriptorSet;
    }

    void WriteCombinedImageSamplerDescriptor(
        VkDevice device, VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler sampler)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = imageView;
        imageInfo.sampler = sampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
}
