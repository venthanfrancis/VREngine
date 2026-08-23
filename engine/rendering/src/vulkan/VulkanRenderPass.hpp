#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns the smallest possible VkRenderPass: one color attachment
    // (the swapchain's format) and, as of M8F, one depth attachment
    // (see docs/ARCHITECTURE.md, "Render-Pass Depth Attachment (M8F)"),
    // both cleared on load. No multisampling, one subpass, no stencil
    // behavior. See docs/ARCHITECTURE.md, "Render Pass vs. Dynamic
    // Rendering (M8C)" for why a traditional render pass was chosen
    // over Vulkan 1.3 dynamic rendering (AREngine targets Vulkan 1.2).
    //
    // Independent of swapchain extent and image count - only the
    // swapchain's color *format* and the chosen depth *format* matter
    // here, and neither changes across a resize (the depth format is
    // fixed once, from device capabilities, at startup — see
    // docs/ARCHITECTURE.md, "Depth Format Selection (M8F)"). So unlike
    // VulkanSwapchain/VulkanFramebuffers/the depth image itself, this
    // does NOT need to be recreated on resize.
    //
    // Not copyable or movable: exactly one VkRenderPass per
    // VulkanRenderPass, destroyed exactly once, by this object alone.
    class VulkanRenderPass
    {
    public:
        VulkanRenderPass(VkDevice device, VkFormat swapchainImageFormat, VkFormat depthFormat);
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
