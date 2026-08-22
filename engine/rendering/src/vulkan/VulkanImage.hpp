#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkImage, the VkDeviceMemory backing it, and one
    // VkImageView over it — the minimum needed to sample a 2D texture
    // in a fragment shader. One mip level, one array layer, no
    // mipmapping. Deliberately small and generic, same "one dedicated
    // allocation, no VMA" discipline as VulkanBuffer. Never exposed
    // outside Rendering's Vulkan backend — no VkImage/VkImageView
    // appears on any public Rendering header. See
    // docs/ARCHITECTURE.md, "Vulkan Image Ownership (M8E)".
    //
    // The VkSampler is deliberately NOT owned here — see
    // VulkanSampler.hpp for why keeping it separate is the simpler
    // choice at this scale.
    //
    // Not copyable or movable: exactly one VkImage/VkDeviceMemory/
    // VkImageView triple per VulkanImage, destroyed exactly once, by
    // this object alone.
    class VulkanImage
    {
    public:
        VulkanImage(VkPhysicalDevice physicalDevice, VkDevice device,
                     std::uint32_t width, std::uint32_t height, VkFormat format,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
        ~VulkanImage();

        VulkanImage(const VulkanImage&) = delete;
        VulkanImage& operator=(const VulkanImage&) = delete;
        VulkanImage(VulkanImage&&) = delete;
        VulkanImage& operator=(VulkanImage&&) = delete;

        [[nodiscard]] VkImage Get() const { return m_image; }
        [[nodiscard]] VkImageView GetView() const { return m_view; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkImageView m_view = VK_NULL_HANDLE;
    };

    // Uploads `pixels` (tightly packed, `width * height * 4` bytes,
    // RGBA8) into a new VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    // VK_FORMAT_R8G8B8A8_SRGB VulkanImage, via a temporary host-visible
    // staging buffer, a UNDEFINED -> TRANSFER_DST_OPTIMAL layout
    // transition, a vkCmdCopyBufferToImage, and a TRANSFER_DST_OPTIMAL
    // -> SHADER_READ_ONLY_OPTIMAL transition — all on one synchronous
    // one-time command buffer (VulkanOneTimeCommands), same model as
    // VulkanBuffer's CreateDeviceLocalBuffer. See
    // docs/ARCHITECTURE.md, "Image Memory / Upload Path (M8E)".
    //
    // Returns std::unique_ptr for the same reason CreateDeviceLocalBuffer
    // does: VulkanImage is non-movable, and this factory does real work
    // between construction and completion.
    [[nodiscard]] std::unique_ptr<VulkanImage> CreateTextureFromPixels(
        VkPhysicalDevice physicalDevice, VkDevice device,
        VkCommandPool commandPool, VkQueue queue,
        std::uint32_t width, std::uint32_t height, const std::uint8_t* pixels);
}
