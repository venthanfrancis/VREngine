#include "VulkanCommandPool.hpp"

#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanCommandPool::VulkanCommandPool(VkDevice device, std::uint32_t queueFamilyIndex)
        : m_device(device)
    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = queueFamilyIndex;

        const VkResult result = vkCreateCommandPool(device, &createInfo, nullptr, &m_commandPool);
        CheckVkResult(result, "vkCreateCommandPool");
    }

    VulkanCommandPool::~VulkanCommandPool()
    {
        if (m_commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }
    }
}
