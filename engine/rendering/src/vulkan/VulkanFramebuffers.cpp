#include "VulkanFramebuffers.hpp"

#include "VulkanResult.hpp"

#include <array>
#include <cstdint>

namespace AREngine::Rendering::Vulkan
{
    VulkanFramebuffers::VulkanFramebuffers(VkDevice device,
                                            VkRenderPass renderPass,
                                            const std::vector<VkImageView>& imageViews,
                                            VkImageView depthImageView,
                                            VkExtent2D extent)
        : m_device(device)
    {
        m_framebuffers.resize(imageViews.size());

        for (std::size_t i = 0; i < imageViews.size(); ++i)
        {
            // Attachment order must match VulkanRenderPass exactly:
            // color (0), depth (1).
            const std::array<VkImageView, 2> attachments{imageViews[i], depthImageView};

            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass;
            createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
            createInfo.pAttachments = attachments.data();
            createInfo.width = extent.width;
            createInfo.height = extent.height;
            createInfo.layers = 1;

            const VkResult result = vkCreateFramebuffer(device, &createInfo, nullptr, &m_framebuffers[i]);
            CheckVkResult(result, "vkCreateFramebuffer");
        }
    }

    VulkanFramebuffers::~VulkanFramebuffers()
    {
        for (VkFramebuffer framebuffer : m_framebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }
        m_framebuffers.clear();
    }
}
