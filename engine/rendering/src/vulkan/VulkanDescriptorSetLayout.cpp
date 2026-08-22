#include "VulkanDescriptorSetLayout.hpp"

#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VkDevice device)
        : m_device(device)
    {
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding = 0;
        samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = 1;
        createInfo.pBindings = &samplerBinding;

        const VkResult result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &m_layout);
        CheckVkResult(result, "vkCreateDescriptorSetLayout");
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
    {
        if (m_layout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
            m_layout = VK_NULL_HANDLE;
        }
    }
}
