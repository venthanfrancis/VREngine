#include "VulkanFramebuffers.hpp"

#include "VulkanResult.hpp"

namespace AREngine::Rendering::Vulkan
{
    VulkanFramebuffers::VulkanFramebuffers(VkDevice device,
                                            VkRenderPass renderPass,
                                            const std::vector<VkImageView>& imageViews,
                                            VkExtent2D extent)
        : m_device(device)
    {
        m_framebuffers.resize(imageViews.size());

        for (std::size_t i = 0; i < imageViews.size(); ++i)
        {
            VkFramebufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass;
            createInfo.attachmentCount = 1;
            createInfo.pAttachments = &imageViews[i];
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
