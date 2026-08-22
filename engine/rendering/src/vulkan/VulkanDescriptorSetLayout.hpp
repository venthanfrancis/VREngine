#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkDescriptorSetLayout describing exactly one binding:
    // binding 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, visible to
    // the fragment stage only - matches triangle.frag's
    // `layout(set = 0, binding = 0) uniform sampler2D uTexture`. M8E is
    // the first milestone that needs any descriptor at all; this is
    // deliberately the smallest possible layout, not a generic
    // descriptor-layout builder. See docs/ARCHITECTURE.md, "Descriptor
    // Set Layout (M8E)".
    //
    // Not copyable or movable: exactly one VkDescriptorSetLayout per
    // VulkanDescriptorSetLayout, destroyed exactly once, by this object
    // alone.
    class VulkanDescriptorSetLayout
    {
    public:
        explicit VulkanDescriptorSetLayout(VkDevice device);
        ~VulkanDescriptorSetLayout();

        VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
        VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;
        VulkanDescriptorSetLayout(VulkanDescriptorSetLayout&&) = delete;
        VulkanDescriptorSetLayout& operator=(VulkanDescriptorSetLayout&&) = delete;

        [[nodiscard]] VkDescriptorSetLayout Get() const { return m_layout; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    };
}
