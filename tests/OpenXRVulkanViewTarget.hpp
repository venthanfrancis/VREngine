#pragma once

// M9H: extracted from M9G's tests/openxr_cube_demo.cpp, per that
// demo's own M9H audit - per-view GPU render-target resource ownership
// (a depth image + a set of framebuffers, one per swapchain image) was
// duplicated in a loop, once per eye, and is worth naming as an
// explicit, documented ownership unit even though it has exactly one
// consumer today. See docs/ARCHITECTURE.md, "M9G Demo Audit (M9H)" and
// "OpenXRVulkanViewTarget Placement (M9H)".
//
// Deliberately kept at this leaf (tests/) level, NOT promoted into
// engine/xr or engine/rendering: doing so would require one of those
// modules to gain a new dependency on the other (OpenXRSwapchain is
// XR-private; VulkanImage/VulkanFramebuffers are Rendering-private),
// which every prior XR milestone (M9C, M9F, M9G) deliberately avoided
// - "engine/xr does not include Rendering's private headers" is an
// established, repeatedly-reaffirmed boundary (see
// docs/ARCHITECTURE.md, Section 30's "Vulkan-Renderer-Layer Concern"
// note). This header/source pair crosses that boundary the same way
// the demo itself already legitimately does, per the M9G/M9H briefs'
// own explicit permission for a leaf to coordinate both systems
// directly - it is simply factored out of the demo's main() for
// clarity and reuse across this demo's own frames, not promoted to a
// public engine API.
//
// Ownership (see docs/ARCHITECTURE.md, "Per-View Resource Model (M9H)"):
//   - Borrows (does not own) the OpenXRSwapchain passed to the
//     constructor - same discipline as every other borrowed handle in
//     this codebase (OpenXRSession/OpenXRReferenceSpace, etc).
//   - Owns the depth VulkanImage and the VulkanFramebuffers built from
//     it - both AREngine-owned, destroyed by this object's destructor
//     (reverse declaration order), well before the OpenXRSwapchain or
//     the VkDevice they were built from.
//   - Never touches, creates, or destroys any OpenXR-owned VkImage -
//     only reads the swapchain's own cached VkImageViews
//     (OpenXRSwapchain::GetImageViews(), M9G) to build framebuffers.
//
// One depth image is shared across every one of this view's swapchain
// images (matching VulkanFramebuffers' own established M8F design,
// unchanged by M9H) - correct under the current fully-synchronous,
// one-frame-in-flight synchronization model, where only one image is
// ever being rasterized into at a time; see docs/ARCHITECTURE.md,
// "Depth Resource Model (M9H)" for why this was re-examined and kept,
// not just assumed.
//
// Not copyable or movable: exactly one depth image/framebuffer set per
// OpenXRVulkanViewTarget, destroyed exactly once, by this object alone.

#include "openxr/OpenXRSwapchain.hpp"

#include "vulkan/VulkanFramebuffers.hpp"
#include "vulkan/VulkanImage.hpp"
#include "vulkan/VulkanMesh.hpp"

#include "AREngine/Core/Math/Mat4.hpp"
#include "AREngine/Core/Math/Vec4.hpp"

#include <cstdint>
#include <memory>

namespace AREngine::XR::OpenXR
{
    class OpenXRVulkanViewTarget
    {
    public:
        // `swapchain` must already have its VkImageViews created (true
        // for any fully-constructed OpenXRSwapchain, M9G onward) and
        // must outlive this object. `renderPass` is used only to build
        // the framebuffers - not stored.
        OpenXRVulkanViewTarget(VkPhysicalDevice physicalDevice, VkDevice device, VkRenderPass renderPass,
                                OpenXRSwapchain& swapchain, VkFormat depthFormat);
        ~OpenXRVulkanViewTarget();

        OpenXRVulkanViewTarget(const OpenXRVulkanViewTarget&) = delete;
        OpenXRVulkanViewTarget& operator=(const OpenXRVulkanViewTarget&) = delete;
        OpenXRVulkanViewTarget(OpenXRVulkanViewTarget&&) = delete;
        OpenXRVulkanViewTarget& operator=(OpenXRVulkanViewTarget&&) = delete;

        [[nodiscard]] OpenXRSwapchain& GetSwapchain() const { return m_swapchain; }
        [[nodiscard]] VkExtent2D GetExtent() const { return m_extent; }
        [[nodiscard]] VkFramebuffer GetFramebuffer(std::uint32_t imageIndex) const { return m_framebuffers->Get(imageIndex); }

    private:
        OpenXRSwapchain& m_swapchain; // borrowed, not owned
        VkExtent2D m_extent{};
        std::unique_ptr<Rendering::Vulkan::VulkanImage> m_depthImage; // AREngine-owned
        std::unique_ptr<Rendering::Vulkan::VulkanFramebuffers> m_framebuffers; // AREngine-owned
    };

    // M10.5: split into three narrower functions (Begin/Draw/End) so a
    // view's render pass can draw MULTIPLE objects, each possibly using
    // a DIFFERENT VulkanMesh (e.g. cube + floor quad) - M9G/M9H's single
    // combined RecordOpenXRViewRenderPass (kept below, unchanged in
    // behavior, still used as-is by openxr_cube_demo.cpp) assumed
    // exactly one object/one mesh per view, which M10.5's evidence
    // (multiple distinct meshes drawn into the same view) outgrew. See
    // docs/ARCHITECTURE.md, "Multi-Object Render Split (M10.5)".

    // Begins the render pass for one view (`clearColor` + a depth clear
    // of 1.0/0), and sets dynamic viewport/scissor sized to
    // `viewTarget`'s own extent. Call once per view, before drawing any
    // objects into it. Assumes the pipeline and descriptor set are
    // already bound on `commandBuffer` by the caller - shared across
    // every view/object this frame (one pipeline, one texture, for
    // everything - M9G/M9H/M10.5 all rely on this), and a bound
    // pipeline/descriptor set persists across vkCmdEndRenderPass ->
    // vkCmdBeginRenderPass within the same command buffer (Vulkan spec),
    // so binding once per frame (not per view) remains correct.
    void BeginOpenXRViewRenderPass(
        VkCommandBuffer commandBuffer,
        VkRenderPass renderPass,
        const OpenXRVulkanViewTarget& viewTarget,
        std::uint32_t acquiredImageIndex,
        VkClearColorValue clearColor);

    // Binds `mesh`'s vertex/index buffers (safe to call even if the
    // same mesh was already bound for a previous object - rebinding
    // identical buffers is idempotent, not a correctness issue, and
    // this codebase deliberately does not optimize away the redundant
    // call for a handful of objects - see docs/ARCHITECTURE.md,
    // "Draw Count / No Batching (M10.5)") and issues one indexed draw
    // with `mvp`/`tint` pushed as this object's MvpPushConstants. Call
    // once per object, between BeginOpenXRViewRenderPass and
    // EndOpenXRViewRenderPass.
    //
    // Deliberately takes a plain MVP matrix, not a Frame::ViewInfo -
    // this function is Vulkan-only and does not need to know about
    // engine/frame at all; the caller computes view/projection/model
    // and multiplies them down before calling this.
    void DrawOpenXRViewObject(
        VkCommandBuffer commandBuffer,
        VkPipelineLayout pipelineLayout,
        const Rendering::Vulkan::VulkanMesh& mesh,
        const Core::Math::Mat4& mvp,
        const Core::Math::Vec4& tint);

    // Ends the render pass begun by BeginOpenXRViewRenderPass.
    void EndOpenXRViewRenderPass(VkCommandBuffer commandBuffer);

    // M9G/M9H convenience wrapper for the single-object-per-view case:
    // Begin + one DrawOpenXRViewObject + End. Unchanged in behavior -
    // still exactly what openxr_cube_demo.cpp calls, still assumes the
    // pipeline/descriptor set (and, harmlessly redundantly, the mesh)
    // are already bound by the caller.
    void RecordOpenXRViewRenderPass(
        VkCommandBuffer commandBuffer,
        VkRenderPass renderPass,
        VkPipelineLayout pipelineLayout,
        const Rendering::Vulkan::VulkanMesh& mesh,
        const OpenXRVulkanViewTarget& viewTarget,
        std::uint32_t acquiredImageIndex,
        const Core::Math::Mat4& mvp,
        const Core::Math::Vec4& tint,
        VkClearColorValue clearColor);
}
