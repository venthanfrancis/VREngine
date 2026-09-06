#include "DrawPlannedInstances.hpp"

#include "vulkan/VulkanMesh.hpp"
#include "vulkan/VulkanPushConstants.hpp"

namespace ARDemo
{
    void DrawPlannedInstances(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const AREngine::Rendering::Vulkan::VulkanRenderResourceContext& context,
        std::span<const PlannedDraw> plan)
    {
        using AREngine::Rendering::MaterialHandle;
        using AREngine::Rendering::MeshHandle;

        for (const PlannedDraw& draw : plan)
        {
            const VkDescriptorSet descriptorSet = context.GetMaterial(MaterialHandle{draw.material.id});
            if (descriptorSet == VK_NULL_HANDLE)
            {
                continue; // unresolvable material id - skip, not fatal (same posture as RenderDevice::SubmitDraw)
            }

            const AREngine::Rendering::Vulkan::VulkanMesh* mesh = context.GetMesh(MeshHandle{draw.mesh.id});
            if (mesh == nullptr)
            {
                continue; // unresolvable mesh id - skip, not fatal (same posture as RenderDevice::SubmitDraw)
            }

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                0, 1, &descriptorSet, 0, nullptr);

            mesh->Bind(commandBuffer);

            const AREngine::Rendering::Vulkan::MvpPushConstants pushConstants{draw.mvp, draw.tint};
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(AREngine::Rendering::Vulkan::MvpPushConstants), &pushConstants);
            mesh->Draw(commandBuffer);
        }
    }
}
