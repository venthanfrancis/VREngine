#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <cstdint>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    // The queue families a presentation-capable logical device needs.
    // Graphics and present may be the same family or different ones —
    // M8B must not assume they're the same (there is no guarantee a
    // queue family that supports graphics also supports presenting to
    // a given surface). See docs/ARCHITECTURE.md, "Queue Families
    // (M8B)".
    struct QueueFamilyIndices
    {
        std::uint32_t graphicsFamily = 0;
        std::uint32_t presentFamily = 0;
    };

    // Pure logic, no Vulkan calls — directly unit-testable.
    [[nodiscard]] constexpr bool HasSeparatePresentQueue(const QueueFamilyIndices& indices)
    {
        return indices.graphicsFamily != indices.presentFamily;
    }

    // The distinct queue family indices device/swapchain creation needs
    // an entry for — one if graphics and present are the same family,
    // two if not. Used both for VkDeviceQueueCreateInfo (how many
    // queues to request) and for VkSwapchainCreateInfoKHR's sharing
    // mode (see VulkanSwapchainSupport.hpp). Pure logic, no Vulkan
    // calls — directly unit-testable.
    [[nodiscard]] std::vector<std::uint32_t> GetUniqueQueueFamilies(const QueueFamilyIndices& indices);
}
