#include "VulkanRenderItemSubmission.hpp"

#include "VulkanMesh.hpp"
#include "VulkanPushConstants.hpp"

namespace AREngine::Rendering::Vulkan
{
    void SubmitRenderItems(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const VulkanRenderResourceContext& context,
        const Core::Math::Mat4& viewProjection,
        std::span<const RenderItem> items)
    {
        for (const RenderItem& item : items)
        {
            const VkDescriptorSet descriptorSet = context.GetMaterial(item.material);
            if (descriptorSet == VK_NULL_HANDLE)
            {
                continue; // unresolvable material handle - skip, not fatal (same posture as RenderDevice::SubmitDraw)
            }

            const VulkanMesh* mesh = context.GetMesh(item.mesh);
            if (mesh == nullptr)
            {
                continue; // unresolvable mesh handle - skip, not fatal (same posture as RenderDevice::SubmitDraw)
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                0, 1, &descriptorSet, 0, nullptr);

            mesh->Bind(commandBuffer);

            const MvpPushConstants pushConstants{viewProjection * item.model, item.tint};
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(MvpPushConstants), &pushConstants);
            mesh->Draw(commandBuffer);
        }
    }
}
