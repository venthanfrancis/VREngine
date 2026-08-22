#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <vector>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkFramebuffer per swapchain image view, all matching
    // the swapchain's extent and compatible with one VkRenderPass.
    //
    // Swapchain-dependent, unlike VulkanRenderPass: image views and
    // extent change on every swapchain recreation, so a
    // VulkanFramebuffers instance must be destroyed and rebuilt
    // whenever the swapchain is (same destroy-then-reconstruct policy
    // as VulkanSwapchain itself — see docs/ARCHITECTURE.md, "Swapchain-
    // Dependent Pipeline Resources (M8C)"). The VkRenderPass it's built
    // against does NOT need to change, since the format it was created
    // for doesn't change across a resize.
    //
    // Not copyable or movable: exactly one set of VkFramebuffers per
    // VulkanFramebuffers, destroyed exactly once, by this object alone.
    class VulkanFramebuffers
    {
    public:
        VulkanFramebuffers(VkDevice device,
                            VkRenderPass renderPass,
                            const std::vector<VkImageView>& imageViews,
                            VkExtent2D extent);
        ~VulkanFramebuffers();

        VulkanFramebuffers(const VulkanFramebuffers&) = delete;
        VulkanFramebuffers& operator=(const VulkanFramebuffers&) = delete;
        VulkanFramebuffers(VulkanFramebuffers&&) = delete;
        VulkanFramebuffers& operator=(VulkanFramebuffers&&) = delete;

        [[nodiscard]] VkFramebuffer Get(std::size_t imageIndex) const { return m_framebuffers[imageIndex]; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_framebuffers;
    };
}
