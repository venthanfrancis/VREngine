#include "MeshRegistry.hpp"

#include "vulkan/VulkanPushConstants.hpp"

namespace ARDemo
{
    void MeshRegistry::Register(AREngine::Scene::MeshId id, const AREngine::Rendering::Vulkan::VulkanMesh* mesh)
    {
        m_meshes[id] = mesh;
    }

    const AREngine::Rendering::Vulkan::VulkanMesh* MeshRegistry::Resolve(AREngine::Scene::MeshId id) const
    {
        const auto it = m_meshes.find(id);
        return it != m_meshes.end() ? it->second : nullptr;
    }

    void DrawPlannedInstances(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const MeshRegistry& meshes,
        std::span<const PlannedDraw> plan)
    {
        for (const PlannedDraw& draw : plan)
        {
            const AREngine::Rendering::Vulkan::VulkanMesh* mesh = meshes.Resolve(draw.mesh);
            if (mesh == nullptr)
            {
                continue; // unresolvable mesh id - skip, not fatal (same posture as RenderDevice::SubmitDraw)
            }

            mesh->Bind(commandBuffer);

            const AREngine::Rendering::Vulkan::MvpPushConstants pushConstants{draw.mvp, draw.tint};
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(AREngine::Rendering::Vulkan::MvpPushConstants), &pushConstants);
            mesh->Draw(commandBuffer);
        }
    }
}
