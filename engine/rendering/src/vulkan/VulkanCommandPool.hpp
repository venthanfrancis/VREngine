#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <cstdint>

namespace AREngine::Rendering::Vulkan
{
    // Owns a single VkCommandPool for one queue family. Deliberately
    // minimal - just enough RAII to match every other owned Vulkan
    // object in this backend. The command buffers allocated from it,
    // the synchronization objects, and the actual frame-loop
    // orchestration all stay in the M8B presentation demo itself rather
    // than in a class here: their shape will very likely change once
    // M8C introduces real command recording, so locking in a permanent
    // design for them now would be premature. See
    // docs/ARCHITECTURE.md, "Command Infrastructure (M8B)".
    //
    // Not copyable or movable: exactly one VkCommandPool per
    // VulkanCommandPool, destroyed exactly once, by this object alone.
    class VulkanCommandPool
    {
    public:
        VulkanCommandPool(VkDevice device, std::uint32_t queueFamilyIndex);
        ~VulkanCommandPool();

        VulkanCommandPool(const VulkanCommandPool&) = delete;
        VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;
        VulkanCommandPool(VulkanCommandPool&&) = delete;
        VulkanCommandPool& operator=(VulkanCommandPool&&) = delete;

        [[nodiscard]] VkCommandPool Get() const { return m_commandPool; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;
    };
}
