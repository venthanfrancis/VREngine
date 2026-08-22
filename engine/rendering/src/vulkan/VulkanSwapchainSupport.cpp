#include "VulkanSwapchainSupport.hpp"

#include "AREngine/Core/Assert.hpp"

#include <algorithm>
#include <limits>

namespace AREngine::Rendering::Vulkan
{
    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        SwapchainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        std::uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount > 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }

        std::uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount > 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available)
    {
        AR_ASSERT_MSG(!available.empty(), "ChooseSurfaceFormat called with no available formats");

        for (const VkSurfaceFormatKHR& format : available)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format;
            }
        }

        // No SRGB BGRA available — fall back to whatever the device
        // listed first, rather than failing outright.
        return available.front();
    }

    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& available)
    {
        for (const VkPresentModeKHR mode : available)
        {
            if (mode == VK_PRESENT_MODE_FIFO_KHR)
            {
                return mode;
            }
        }

        // The Vulkan spec requires VK_PRESENT_MODE_FIFO_KHR to always
        // be supported — reaching here means something is seriously
        // wrong with the surface query, not a normal "unsupported"
        // outcome to degrade from gracefully.
        AR_ASSERT_MSG(false, "VK_PRESENT_MODE_FIFO_KHR not found in supported present modes - spec violation");
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    std::uint32_t ChooseSwapchainImageCount(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        std::uint32_t count = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount != 0 && count > capabilities.maxImageCount)
        {
            count = capabilities.maxImageCount;
        }
        return count;
    }

    VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                      std::uint32_t windowWidth,
                                      std::uint32_t windowHeight)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        VkExtent2D extent{windowWidth, windowHeight};
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return extent;
    }
}
