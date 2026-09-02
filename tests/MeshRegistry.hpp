#pragma once

// M12: resolves the backend-neutral Scene::MeshId a PlannedDraw carries
// to the actual uploaded VulkanMesh it refers to, and executes the
// resulting draws. Vulkan-only (needs VulkanMesh/MvpPushConstants) - NOT
// built on top of or inside OpenXRVulkanViewTarget.hpp, which
// transitively includes OpenXR headers and is therefore never compiled
// into a VULKAN=ON, OPENXR=OFF target. Kept at this leaf (tests/) level,
// same reasoning as OpenXRVulkanViewTarget.hpp: promoting it into
// engine/rendering would require that module to learn about Scene's
// MeshId, which nothing today needs it to do. See docs/ARCHITECTURE.md,
// "M12 - Renderable Scene Integration Foundation".
//
// Small, explicit, and owned entirely by whichever demo constructs it -
// not a global registry. Registrations are borrowed pointers: the
// VulkanMesh objects themselves must outlive this MeshRegistry.

#include "MaterialRegistry.hpp"
#include "RenderDrawPlanning.hpp"

#include "vulkan/VulkanMesh.hpp"

#include <span>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace ARDemo
{
    class MeshRegistry
    {
    public:
        void Register(AREngine::Scene::MeshId id, const AREngine::Rendering::Vulkan::VulkanMesh* mesh);

        // nullptr if `id` was never registered.
        [[nodiscard]] const AREngine::Rendering::Vulkan::VulkanMesh* Resolve(AREngine::Scene::MeshId id) const;

    private:
        std::unordered_map<AREngine::Scene::MeshId, const AREngine::Rendering::Vulkan::VulkanMesh*> m_meshes;
    };

    // For each PlannedDraw: resolves its material via `materials` (M13)
    // and binds that descriptor set - every draw rebinds, even when
    // consecutive draws share a material, deliberately: no batching/
    // redundant-bind-avoidance logic, per the milestone's own "acceptable
    // to bind per draw" allowance - then resolves its mesh via `meshes`,
    // binds it, pushes MvpPushConstants{mvp, tint}, and draws. Either
    // lookup failing (an unknown MaterialId resolves to VK_NULL_HANDLE;
    // an unknown MeshId resolves to nullptr) skips that one draw, not
    // fatal - same "don't crash on a coordination gap" posture as
    // Rendering::RenderDevice::SubmitDraw returning false for an unknown
    // handle. The same Bind+push+draw shape DrawOpenXRViewObject already
    // establishes for XR, generalized to a whole plan at once so both
    // the desktop scene demo and the XR demo's per-view loop can share
    // it. Assumes the render pass and pipeline are already bound by the
    // caller (descriptor-set binding is this function's own job now,
    // per-draw, since M13 - unlike M12, the caller must NOT bind a
    // descriptor set upfront).
    void DrawPlannedInstances(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const MeshRegistry& meshes,
        const MaterialRegistry& materials,
        std::span<const PlannedDraw> plan);
}
