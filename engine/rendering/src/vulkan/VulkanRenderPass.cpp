#include "VulkanRenderPass.hpp"

#include "VulkanResult.hpp"

#include <array>
#include <cstdint>

namespace AREngine::Rendering::Vulkan
{
    VulkanRenderPass::VulkanRenderPass(VkDevice device, VkFormat swapchainImageFormat, VkFormat depthFormat,
                                        VkImageLayout colorFinalLayout)
        : m_device(device)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // UNDEFINED: each frame clears the whole attachment
        // (loadOp=CLEAR), so the image's previous contents - and thus
        // its previous layout - genuinely don't matter going in.
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = colorFinalLayout;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth: cleared every frame (loadOp=CLEAR - see
        // docs/ARCHITECTURE.md, "Depth Compare / Clear Values (M8F)"
        // for why 1.0 is the clear value paired with COMPARE_OP_LESS),
        // never stored (storeOp=DONT_CARE - nothing needs the previous
        // frame's depth contents once this frame is drawn). No stencil
        // load/store either way, regardless of whether the chosen
        // format happens to carry a stencil component.
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = depthFormat;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        // Without this, the implicit layout transitions to
        // COLOR_ATTACHMENT_OPTIMAL/DEPTH_STENCIL_ATTACHMENT_OPTIMAL are
        // allowed to happen as early as VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        // - before vkAcquireNextImageKHR's semaphore has actually
        // signaled that the color image is ours to use, and before the
        // depth image is actually clear. Forcing both transitions to
        // wait at COLOR_ATTACHMENT_OUTPUT / EARLY_FRAGMENT_TESTS (the
        // stage depth clear/test/write happens at) makes the
        // dependency correct instead of accidentally-usually-correct.
        // COLOR_ATTACHMENT_OUTPUT is also the stage the frame's submit
        // sets as its wait-semaphore stage - see
        // tests/vulkan_present_demo.cpp.
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                  | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        const std::array<VkAttachmentDescription, 2> attachments{colorAttachment, depthAttachment};

        VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;

        const VkResult result = vkCreateRenderPass(device, &createInfo, nullptr, &m_renderPass);
        CheckVkResult(result, "vkCreateRenderPass");
    }

    VulkanRenderPass::~VulkanRenderPass()
    {
        if (m_renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
    }
}
