#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkBuffer and the VkDeviceMemory backing it. Deliberately
    // small and generic — no VMA, no sub-allocation, one buffer gets
    // one dedicated memory allocation, same as every other owned Vulkan
    // resource in this backend. Never exposed outside Rendering's
    // Vulkan backend — no VkBuffer appears on any public Rendering
    // header. See docs/ARCHITECTURE.md, "Buffer Ownership (M8D)".
    //
    // Not copyable or movable: exactly one VkBuffer/VkDeviceMemory pair
    // per VulkanBuffer, destroyed exactly once, by this object alone.
    class VulkanBuffer
    {
    public:
        VulkanBuffer(VkPhysicalDevice physicalDevice, VkDevice device,
                     VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
        ~VulkanBuffer();

        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;
        VulkanBuffer(VulkanBuffer&&) = delete;
        VulkanBuffer& operator=(VulkanBuffer&&) = delete;

        [[nodiscard]] VkBuffer Get() const { return m_buffer; }
        [[nodiscard]] VkDeviceSize GetSize() const { return m_size; }

        // Maps this buffer's memory, memcpy's `size` bytes from `data`
        // into it, and unmaps again. Only valid for a buffer created
        // with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT — asserts otherwise.
        // See docs/ARCHITECTURE.md, "Memory Type Selection (M8D)".
        void CopyDataIn(const void* data, VkDeviceSize size);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkDeviceSize m_size = 0;
        bool m_hostVisible = false;
    };

    // Uploads `data` (`size` bytes) into a new VK_MEMORY_PROPERTY_
    // DEVICE_LOCAL_BIT buffer usable for `usage`
    // (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT or
    // VK_BUFFER_USAGE_INDEX_BUFFER_BIT — VK_BUFFER_USAGE_TRANSFER_DST_BIT
    // is added automatically), via a temporary host-visible staging
    // buffer and a one-time vkCmdCopyBuffer on `queue`
    // (VulkanOneTimeCommands). The staging buffer is destroyed before
    // this function returns — its job is done once the copy completes.
    // Blocks until the upload completes; see docs/ARCHITECTURE.md,
    // "Upload Strategy (M8D)" and "Synchronous Upload Limitation
    // (M8D)".
    //
    // Returns std::unique_ptr rather than VulkanBuffer by value:
    // VulkanBuffer is deliberately non-movable (same discipline as
    // every other owned Vulkan object here), so a factory function like
    // this one — which does real work between construction and
    // completion, not just "call one constructor" — owns its result
    // through a pointer rather than relying on guaranteed copy elision
    // lining up exactly right.
    [[nodiscard]] std::unique_ptr<VulkanBuffer> CreateDeviceLocalBuffer(
        VkPhysicalDevice physicalDevice, VkDevice device,
        VkCommandPool commandPool, VkQueue queue,
        const void* data, VkDeviceSize size, VkBufferUsageFlags usage);
}
