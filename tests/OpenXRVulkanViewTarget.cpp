#include "OpenXRVulkanViewTarget.hpp"

#include "vulkan/VulkanPushConstants.hpp"

#include <array>

namespace AREngine::XR::OpenXR
{
    OpenXRVulkanViewTarget::OpenXRVulkanViewTarget(VkPhysicalDevice physicalDevice, VkDevice device, VkRenderPass renderPass,
                                                     OpenXRSwapchain& swapchain, VkFormat depthFormat)
        : m_swapchain(swapchain)
        , m_extent{swapchain.GetWidth(), swapchain.GetHeight()}
    {
        m_depthImage = std::make_unique<Rendering::Vulkan::VulkanImage>(
            physicalDevice, device, m_extent.width, m_extent.height,
            depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);
        m_framebuffers = std::make_unique<Rendering::Vulkan::VulkanFramebuffers>(
            device, renderPass, swapchain.GetImageViews(), m_depthImage->GetView(), m_extent);
    }

    OpenXRVulkanViewTarget::~OpenXRVulkanViewTarget() = default;

    void BeginOpenXRViewRenderPass(
        VkCommandBuffer commandBuffer,
        VkRenderPass renderPass,
        const OpenXRVulkanViewTarget& viewTarget,
        std::uint32_t acquiredImageIndex,
        VkClearColorValue clearColor)
    {
        const VkExtent2D extent = viewTarget.GetExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = clearColor;
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassBegin{};
        renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBegin.renderPass = renderPass;
        renderPassBegin.framebuffer = viewTarget.GetFramebuffer(acquiredImageIndex);
        renderPassBegin.renderArea.offset = {0, 0};
        renderPassBegin.renderArea.extent = extent;
        renderPassBegin.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
        renderPassBegin.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    void DrawOpenXRViewObject(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const Rendering::Vulkan::VulkanMesh& mesh,
        const Core::Math::Mat4& mvp,
        const Core::Math::Vec4& tint)
    {
        // Rebinding is always correct (idempotent if this object shares
        // the previous one's mesh) and is what makes multiple distinct
        // meshes per view possible - see this function's own header
        // comment.
        mesh.Bind(commandBuffer);

        const Rendering::Vulkan::MvpPushConstants pushConstants{mvp, tint};
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(Rendering::Vulkan::MvpPushConstants), &pushConstants);
        mesh.Draw(commandBuffer);
    }

    void EndOpenXRViewRenderPass(VkCommandBuffer commandBuffer)
    {
        vkCmdEndRenderPass(commandBuffer);
    }

    void RecordOpenXRViewRenderPass(
        VkCommandBuffer commandBuffer,
        VkRenderPass renderPass,
        VkPipelineLayout pipelineLayout,
        const Rendering::Vulkan::VulkanMesh& mesh,
        const OpenXRVulkanViewTarget& viewTarget,
        std::uint32_t acquiredImageIndex,
        const Core::Math::Mat4& mvp,
        const Core::Math::Vec4& tint,
        VkClearColorValue clearColor)
    {
        BeginOpenXRViewRenderPass(commandBuffer, renderPass, viewTarget, acquiredImageIndex, clearColor);
        DrawOpenXRViewObject(commandBuffer, pipelineLayout, mesh, mvp, tint);
        EndOpenXRViewRenderPass(commandBuffer);
    }
}
