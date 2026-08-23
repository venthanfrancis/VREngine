#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include "AREngine/Rendering/MeshData.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

namespace AREngine::Rendering::Vulkan
{
    class VulkanBuffer;

    // Owns the two GPU buffers (vertex + index) one uploaded MeshData
    // becomes, plus the index count needed to draw it. The Vulkan-side
    // half of the M8H split:
    //
    //   MeshData (CPU, backend-independent)
    //       |  upload (CreateVulkanMesh)
    //       v
    //   VulkanMesh (GPU, Vulkan-private)
    //       |
    //       v
    //   Bind() + Draw()
    //
    // No VkBuffer is exposed on any public Rendering header — this
    // class, like VulkanBuffer/VulkanImage before it, is reached only
    // through Rendering's private src/vulkan/ implementation. See
    // docs/ARCHITECTURE.md, "VulkanMesh Ownership (M8H)".
    //
    // Deliberately reusable across many draws with different Model
    // transforms: nothing here is per-instance or per-frame state, so
    // one VulkanMesh's Bind() can be called once and its Draw() called
    // many times (with a push-constant update between each) to render
    // many instances of the same geometry — see "Multiple Instances
    // (M8H)".
    //
    // Not copyable or movable: exactly one vertex/index buffer pair per
    // VulkanMesh, destroyed exactly once, by this object alone (same
    // discipline as every other owned Vulkan resource in this backend).
    class VulkanMesh
    {
    public:
        // Validates `meshData` (AR_ASSERT_MSG — invalid geometry is a
        // programmer error, not a runtime condition to recover from),
        // then uploads its vertices and indices via the same
        // staging-buffer path every other GPU buffer in this backend
        // uses (CreateDeviceLocalBuffer). Synchronous, like every other
        // upload here — see docs/ARCHITECTURE.md, "Upload Flow (M8H)".
        VulkanMesh(VkPhysicalDevice physicalDevice, VkDevice device,
                   VkCommandPool commandPool, VkQueue queue, const MeshData& meshData);
        ~VulkanMesh();

        VulkanMesh(const VulkanMesh&) = delete;
        VulkanMesh& operator=(const VulkanMesh&) = delete;
        VulkanMesh(VulkanMesh&&) = delete;
        VulkanMesh& operator=(VulkanMesh&&) = delete;

        // Binds this mesh's vertex buffer (binding 0) and index buffer.
        // Call once before one or more Draw() calls - rebinding the
        // same mesh between instances that share it would be redundant
        // work, not a correctness requirement.
        void Bind(VkCommandBuffer commandBuffer) const;

        // Issues one vkCmdDrawIndexed covering this mesh's full index
        // range. Callers push whatever per-instance push constants
        // (Model/MVP, tint) they need between Bind() and each Draw().
        void Draw(VkCommandBuffer commandBuffer) const;

    private:
        std::unique_ptr<VulkanBuffer> m_vertexBuffer;
        std::unique_ptr<VulkanBuffer> m_indexBuffer;
        std::uint32_t m_indexCount = 0;
    };

    // Thin factory mirroring CreateDeviceLocalBuffer/CreateTextureFromPixels's
    // shape: validate -> upload -> return an owned resource by pointer
    // (VulkanMesh is non-movable, same reasoning as those). See
    // docs/ARCHITECTURE.md, "Upload Flow (M8H)".
    [[nodiscard]] std::unique_ptr<VulkanMesh> CreateVulkanMesh(
        VkPhysicalDevice physicalDevice, VkDevice device,
        VkCommandPool commandPool, VkQueue queue, const MeshData& meshData);
}
