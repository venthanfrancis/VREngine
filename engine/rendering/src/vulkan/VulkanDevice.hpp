#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "VulkanQueueFamilies.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace AREngine::Rendering::Vulkan
{
    // Owns a logical VkDevice and its queue(s). Two constructors:
    //
    //  - VulkanDevice(physicalDevice, graphicsQueueFamilyIndex): M8A's
    //    original bring-up path. Enables no device extensions, one
    //    queue, no present queue - unchanged since M8A, still used by
    //    the M8A bring-up demo/tests. See docs/ARCHITECTURE.md,
    //    "Device Ownership".
    //
    //  - VulkanDevice(physicalDevice, queueFamilies,
    //    enableSwapchainExtension): M8B's presentation path. Requests
    //    one VkDeviceQueueCreateInfo per unique queue family
    //    (GetUniqueQueueFamilies - one if graphics and present share a
    //    family, two if not; never assumes they're the same) and, when
    //    `enableSwapchainExtension` is true, enables VK_KHR_swapchain.
    //    See docs/ARCHITECTURE.md, "Device Extensions (M8B)" and
    //    "Queue Families (M8B)".
    //
    // Not copyable or movable: exactly one VkDevice per VulkanDevice,
    // destroyed exactly once, by this object alone.
    class VulkanDevice
    {
    public:
        VulkanDevice(VkPhysicalDevice physicalDevice, std::uint32_t graphicsQueueFamilyIndex);
        VulkanDevice(VkPhysicalDevice physicalDevice, const QueueFamilyIndices& queueFamilies, bool enableSwapchainExtension);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;
        VulkanDevice(VulkanDevice&&) = delete;
        VulkanDevice& operator=(VulkanDevice&&) = delete;

        [[nodiscard]] VkDevice Get() const { return m_device; }
        [[nodiscard]] VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }

        // Only meaningful for the M8B constructor. When graphics and
        // present share a queue family, this returns the same VkQueue
        // as GetGraphicsQueue() (Vulkan queues are retrieved per
        // family+index, and requesting the same family twice would be
        // invalid - see GetUniqueQueueFamilies) - not a second queue.
        [[nodiscard]] VkQueue GetPresentQueue() const { return m_presentQueue; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        VkQueue m_presentQueue = VK_NULL_HANDLE;
    };
}
