#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.
//
// M17: the Rendering-owned draw-execution step, promoted out of the now-
// deleted tests/DrawPlannedInstances.hpp/.cpp. Single free function,
// mirroring VulkanClipSpace.hpp's own "noun-phrase file, one free
// function inside" shape (ApplyVulkanYFlip). Named for what it actually
// handles (RenderItem submission) - deliberately NOT "...Scene..." -
// "Scene" appears nowhere under engine/rendering/src/vulkan/ today,
// and this function has zero knowledge of Scene. See
// docs/ARCHITECTURE.md, "M17 - Scene Render Submission Foundation".

#include "AREngine/Rendering/RenderItem.hpp"
#include "VulkanRenderResourceContext.hpp"

#include <span>
#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // For each item in `items`: resolves its material via `context` and
    // binds that descriptor set - every item rebinds, even when
    // consecutive items share a material, deliberately (no batching/
    // redundant-bind-avoidance, matching M13's own "acceptable to bind
    // per draw" allowance) - then resolves its mesh via `context`,
    // binds it, computes `mvp = viewProjection * item.model` on the
    // fly, pushes MvpPushConstants{mvp, item.tint}, and draws. Either
    // lookup failing (an unknown MaterialHandle resolves to
    // VK_NULL_HANDLE; an unknown MeshHandle resolves to nullptr) skips
    // that one item, not fatal - same "don't crash on a coordination
    // gap" posture as Rendering::RenderDevice::SubmitDraw returning
    // false for an unknown handle.
    //
    // Takes exactly ONE already-combined view's matrix
    // (ApplyVulkanYFlip(projection) * view) - the caller invokes this
    // once per view (a desktop demo calls it once; an XR demo calls it
    // once per eye, inside its own Begin/EndRenderPass pair). This is
    // the whole "N items x M views = M calls" design: there is no
    // per-view pre-expansion of `items` anywhere - see
    // docs/ARCHITECTURE.md for why that's a deliberate simplification,
    // not an oversight.
    //
    // Assumes the render pass and pipeline are already bound by the
    // caller (descriptor-set binding is this function's own job,
    // per-item).
    void SubmitRenderItems(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const VulkanRenderResourceContext& context,
        const Core::Math::Mat4& viewProjection,
        std::span<const RenderItem> items);
}
