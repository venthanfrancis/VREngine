#pragma once

// Private Vulkan bring-up implementation — see VulkanVersion.hpp.

#include <vulkan/vulkan.h>

namespace AREngine::Rendering::Vulkan
{
    // Owns one VkPipelineLayout and one VkPipeline: the whole of
    // M8C's graphics pipeline. Loads and compiles triangle.vert/
    // triangle.frag (as pre-built SPIR-V - see docs/ARCHITECTURE.md,
    // "Shader Language / Toolchain (M8C)") internally; the shader
    // modules are destroyed again before the constructor returns; a
    // VkPipeline doesn't need them once it's built.
    //
    // Deliberately Vulkan-private, same as VulkanInstance/VulkanDevice/
    // etc: M4 explicitly deferred designing a generic PipelineHandle/
    // pipeline abstraction for the public Rendering API until there was
    // real Vulkan evidence to design it from. One triangle is evidence
    // that a pipeline concept exists, not enough evidence to finalize
    // its generic shape - see docs/ARCHITECTURE.md, "Pipeline Ownership
    // (M8C)".
    //
    // One descriptor set (the combined-image-sampler layout the caller
    // passes in — see VulkanDescriptorSetLayout.hpp), no push constants.
    // One vertex input binding, three attributes (position, color, uv) -
    // matches Vertex exactly (VulkanVertex.hpp/.cpp); M8C's
    // gl_VertexIndex-generated positions are gone as of M8D - see
    // docs/ARCHITECTURE.md, "Vertex Input Layout (M8D)" and "Pipeline
    // Layout Change (M8E)". Viewport and scissor are dynamic state, so
    // this pipeline does not need to be recreated when the swapchain
    // extent changes on resize - only VulkanFramebuffers does.
    //
    // Independent of swapchain extent/image count, same as
    // VulkanRenderPass — does NOT need to be recreated on resize.
    //
    // Not copyable or movable: exactly one VkPipeline (and its layout)
    // per VulkanGraphicsPipeline, destroyed exactly once, by this
    // object alone.
    class VulkanGraphicsPipeline
    {
    public:
        VulkanGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout);
        ~VulkanGraphicsPipeline();

        VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline(VulkanGraphicsPipeline&&) = delete;
        VulkanGraphicsPipeline& operator=(VulkanGraphicsPipeline&&) = delete;

        [[nodiscard]] VkPipeline Get() const { return m_pipeline; }
        [[nodiscard]] VkPipelineLayout GetLayout() const { return m_layout; }

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
}
