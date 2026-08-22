#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns the smallest possible VkRenderPass: one color attachment
    // (the swapchain's format), cleared on load, presentable on
    // completion. No depth attachment, no multisampling, one subpass.
    // See docs/ARCHITECTURE.md, "Render Pass vs. Dynamic Rendering
    // (M8C)" for why a traditional render pass was chosen over Vulkan
    // 1.3 dynamic rendering (AREngine targets Vulkan 1.2).
    //
    // Independent of swapchain extent and image count - only the
    // swapchain's *format* matters here, and that doesn't change
    // across a resize (see docs/ARCHITECTURE.md, "Swapchain-Dependent
    // Pipeline Resources (M8C)"). So unlike VulkanSwapchain/
    // VulkanFramebuffers, this does NOT need to be recreated on
    // resize.
    //
    // Not copyable or movable: exactly one VkRenderPass per
    // VulkanRenderPass, destroyed exactly once, by this object alone.
    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(VkDevice device, VkFormat swapchainImageFormat);
        ~VulkanRenderPass();

        VulkanRenderPass(const VulkanRenderPass&) = delete;
        VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
        VulkanRenderPass(VulkanRenderPass&&) = delete;
        VulkanRenderPass& operator=(VulkanRenderPass&&) = delete;

        [[nodiscard]] VkRenderPass Get() const { return m_renderPass; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
    };
}
