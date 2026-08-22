#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

#include <string>

namespace AREngine::Rendering::Vulkan
{
    // Owns a VkShaderModule loaded from a compiled SPIR-V (.spv) file
    // on disk. Purely a construction detail for VulkanGraphicsPipeline
    // - a shader module has no purpose once the pipeline that consumes
    // it is created, so these are short-lived (constructed, handed to
    // vkCreateGraphicsPipelines, then destroyed) rather than kept
    // around. Never exposed outside Rendering's Vulkan backend. See
    // docs/ARCHITECTURE.md, "Shader Language / Toolchain (M8C)".
    //
    // Not copyable or movable: exactly one VkShaderModule per
    // VulkanShaderModule, destroyed exactly once, by this object alone.
    class VulkanShaderModule
    {
    public:
        VulkanShaderModule(VkDevice device, const std::string& spirvPath);
        ~VulkanShaderModule();

        VulkanShaderModule(const VulkanShaderModule&) = delete;
        VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;
        VulkanShaderModule(VulkanShaderModule&&) = delete;
        VulkanShaderModule& operator=(VulkanShaderModule&&) = delete;

        [[nodiscard]] VkShaderModule Get() const { return m_module; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkShaderModule m_module = VK_NULL_HANDLE;
    };
}
