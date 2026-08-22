#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkSampler. Kept separate from VulkanImage (rather than
    // combined into one "texture" type) because a sampler describes
    // *how to read* an image (filtering, addressing) and is commonly
    // shared across multiple images in a real engine - keeping it a
    // separate, independently-constructible object is the simpler
    // choice, not a premature "texture system." See
    // docs/ARCHITECTURE.md, "Sampler Settings (M8E)" for the filtering/
    // addressing choices this constructor makes and why.
    //
    // Not copyable or movable: exactly one VkSampler per VulkanSampler,
    // destroyed exactly once, by this object alone.
    class VulkanSampler
    {
    public:
        explicit VulkanSampler(VkDevice device);
        ~VulkanSampler();

        VulkanSampler(const VulkanSampler&) = delete;
        VulkanSampler& operator=(const VulkanSampler&) = delete;
        VulkanSampler(VulkanSampler&&) = delete;
        VulkanSampler& operator=(VulkanSampler&&) = delete;

        [[nodiscard]] VkSampler Get() const { return m_sampler; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
    };
}
