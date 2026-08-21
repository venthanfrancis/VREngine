#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>

namespace AREngine::Rendering::Vulkan
{
    // Owns a logical VkDevice and its graphics VkQueue. Enables no
    // device extensions and requests no optional features — M8A is
    // bring-up only; VK_KHR_swapchain and friends are added once a
    // swapchain milestone genuinely needs them. See
    // docs/ARCHITECTURE.md, "Device Ownership".
    //
    // M8A does not yet distinguish a graphics queue from a present
    // queue — there is no surface to present to, so no present queue
    // to find. That distinction is deferred to whichever milestone
    // adds a swapchain (see docs/ARCHITECTURE.md, "Queue Selection").
    //
    // Not copyable or movable: exactly one VkDevice per VulkanDevice,
    // destroyed exactly once, by this object alone.
    class VulkanDevice
    {
    public:
        VulkanDevice(VkPhysicalDevice physicalDevice, std::uint32_t graphicsQueueFamilyIndex);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;
        VulkanDevice(VulkanDevice&&) = delete;
        VulkanDevice& operator=(VulkanDevice&&) = delete;

        [[nodiscard]] VkDevice Get() const { return m_device; }
        [[nodiscard]] VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    };
}
