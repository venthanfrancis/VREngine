#pragma once

// M16: executes a plan of PlannedDraws against the engine-owned
// VulkanRenderResourceContext - previously against a demo-local
// MeshRegistry + MaterialRegistry pair (this free function used to
// live inside tests/MeshRegistry.hpp/.cpp, now deleted; it was never
// conceptually tied to MeshRegistry specifically, so it gets its own
// small home now that both lookups it needs come from one object). See
// docs/ARCHITECTURE.md, "M16 - Render Resource Context Foundation".
//
// Kept at this leaf (tests/) level, same reasoning as
// OpenXRVulkanViewTarget.hpp: this needs VulkanMesh/MvpPushConstants
// (Vulkan-only) - NOT built on top of or inside
// OpenXRVulkanViewTarget.hpp, which transitively includes OpenXR
// headers and is therefore never compiled into a VULKAN=ON,
// OPENXR=OFF target. Deliberately NOT merged into
// tests/RenderDrawPlanning.hpp/.cpp, which is Vulkan-free by design
// (built unconditionally for its own pure-logic test target) - merging
// would wrongly force that target to require Vulkan.

#include "RenderDrawPlanning.hpp"

#include "vulkan/VulkanRenderResourceContext.hpp"

#include <span>
#include <vulkan/vulkan.h>

namespace ARDemo
{
    // For each PlannedDraw: resolves its material via `context` and
    // binds that descriptor set - every draw rebinds, even when
    // consecutive draws share a material, deliberately: no batching/
    // redundant-bind-avoidance logic, per M13's own "acceptable to bind
    // per draw" allowance - then resolves its mesh via `context`, binds
    // it, pushes MvpPushConstants{mvp, tint}, and draws. Either lookup
    // failing (an unknown MaterialId resolves to VK_NULL_HANDLE; an
    // unknown MeshId resolves to nullptr) skips that one draw, not
    // fatal - same "don't crash on a coordination gap" posture as
    // Rendering::RenderDevice::SubmitDraw returning false for an
    // unknown handle. Assumes the render pass and pipeline are already
    // bound by the caller (descriptor-set binding is this function's
    // own job, per-draw).
    void DrawPlannedInstances(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const AREngine::Rendering::Vulkan::VulkanRenderResourceContext& context,
        std::span<const PlannedDraw> plan);
}
