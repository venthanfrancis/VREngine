#include "VulkanDevice.hpp"

#include "VulkanResult.hpp"

#include <vector>

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

    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice, const QueueFamilyIndices& queueFamilies, bool enableSwapchainExtension)
    {
        constexpr float kQueuePriority = 1.0f;

        const std::vector<std::uint32_t> uniqueFamilies = GetUniqueQueueFamilies(queueFamilies);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueFamilies.size());
        for (const std::uint32_t family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = family;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &kQueuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        std::vector<const char*> enabledExtensions;
        if (enableSwapchainExtension)
        {
            enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<std::uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<std::uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        // No pEnabledFeatures: M8B needs no optional device feature —
        // see docs/ARCHITECTURE.md, "Device Ownership".

        const VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_device);
        CheckVkResult(result, "vkCreateDevice");

        vkGetDeviceQueue(m_device, queueFamilies.graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, queueFamilies.presentFamily, 0, &m_presentQueue);
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
