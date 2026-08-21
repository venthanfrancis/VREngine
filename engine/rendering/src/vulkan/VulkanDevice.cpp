#include "VulkanDevice.hpp"

#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice, std::uint32_t graphicsQueueFamilyIndex)
    {
        constexpr float kQueuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &kQueuePriority;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        // No pEnabledFeatures, no device extensions: nothing in M8A
        // needs any optional feature or extension — see
        // docs/ARCHITECTURE.md, "Device Ownership".

        const VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_device);
        CheckVkResult(result, "vkCreateDevice");

        vkGetDeviceQueue(m_device, graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
    }

    VulkanDevice::~VulkanDevice()
    {
        if (m_device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
        }
    }
}
