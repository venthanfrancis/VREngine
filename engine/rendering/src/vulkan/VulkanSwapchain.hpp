#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "VulkanQueueFamilies.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace AREngine::Rendering::Vulkan
{
    // Owns a VkSwapchainKHR and one VkImageView per swapchain image.
    // Policy (which format/present mode/extent/image count get chosen)
    // lives in VulkanSwapchainSupport.hpp's pure-logic helpers - this
    // class just calls them and owns the resulting Vulkan objects. See
    // docs/ARCHITECTURE.md, "Swapchain Ownership (M8B)".
    //
    // Resize/out-of-date policy: AREngine does NOT pass an
    // `oldSwapchain` for a smoother in-place transition. On resize or
    // VK_ERROR_OUT_OF_DATE_KHR/VK_SUBOPTIMAL_KHR, the demo destroys the
    // whole VulkanSwapchain object (after waiting for the GPU to go
    // idle) and constructs a new one - simpler, and sufuficient for
    // M8B's "don't break on resize" requirement. See
    // docs/ARCHITECTURE.md, "Resize / Swapchain Recreation (M8B)".
    //
    // Must be destroyed before the VkSurfaceKHR/VkDevice it was created
    // from - see docs/ARCHITECTURE.md, "Exact Destruction Order (M8B)".
    // Not copyable or movable: exactly one VkSwapchainKHR per
    // VulkanSwapchain, destroyed exactly once, by this object alone.
    class VulkanSwapchain
    {
    public:
        VulkanSwapchain(VkPhysicalDevice physicalDevice,
                         VkDevice device,
                         VkSurfaceKHR surface,
                         const QueueFamilyIndices& queueFamilies,
                         std::uint32_t windowWidth,
                         std::uint32_t windowHeight);
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain&) = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
        VulkanSwapchain(VulkanSwapchain&&) = delete;
        VulkanSwapchain& operator=(VulkanSwapchain&&) = delete;

        [[nodiscard]] VkSwapchainKHR Get() const { return m_swapchain; }
        [[nodiscard]] VkFormat GetImageFormat() const { return m_imageFormat; }
        [[nodiscard]] VkExtent2D GetExtent() const { return m_extent; }
        [[nodiscard]] const std::vector<VkImage>& GetImages() const { return m_images; }
        [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
        [[nodiscard]] std::uint32_t GetImageCount() const { return static_cast<std::uint32_t>(m_images.size()); }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};
        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_imageViews;
    };
}
