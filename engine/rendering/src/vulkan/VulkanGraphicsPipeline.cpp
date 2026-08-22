#include "VulkanGraphicsPipeline.hpp"

#include "VulkanResult.hpp"
#include "VulkanShaderModule.hpp"

#include <array>
#include <cstdint>
#include <string>

#ifndef ARENGINE_SHADER_DIR
#error "ARENGINE_SHADER_DIR must be defined by the build system (see engine/rendering/CMakeLists.txt) - it's where CMake compiles triangle.vert/triangle.frag to SPIR-V"
#endif

namespace AREngine::Rendering::Vulkan
{
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VkDevice device, VkRenderPass renderPass)
        : m_device(device)
    {
        VulkanShaderModule vertModule(device, std::string(ARENGINE_SHADER_DIR) + "/triangle.vert.spv");
        VulkanShaderModule fragModule(device, std::string(ARENGINE_SHADER_DIR) + "/triangle.frag.spv");

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertModule.Get();
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragModule.Get();
        fragStage.pName = "main";

        const std::array<VkPipelineShaderStageCreateInfo, 2> stages{vertStage, fragStage};

        // No vertex bindings/attributes: triangle.vert generates its 3
        // positions from gl_VertexIndex, so the pipeline is never given
        // any vertex data to fetch. See docs/ARCHITECTURE.md, "Why
        // gl_VertexIndex Instead Of A Vertex Buffer (M8C)".
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // Viewport/scissor are dynamic state (below), so only their
        // counts matter here - the actual values are set per frame via
        // vkCmdSetViewport/vkCmdSetScissor, which is why this pipeline
        // doesn't need to be recreated when the swapchain extent
        // changes on resize. See docs/ARCHITECTURE.md, "Viewport /
        // Scissor Strategy (M8C)".
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_NONE;
        rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                             | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        const std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // Empty: M8C needs no descriptor sets and no push constants -
        // see docs/ARCHITECTURE.md, "Pipeline Layout (M8C)".
        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        VkResult layoutResult = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &m_layout);
        CheckVkResult(layoutResult, "vkCreatePipelineLayout");

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = static_cast<std::uint32_t>(stages.size());
        pipelineCreateInfo.pStages = stages.data();
        pipelineCreateInfo.pVertexInputState = &vertexInput;
        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pRasterizationState = &rasterization;
        pipelineCreateInfo.pMultisampleState = &multisampling;
        pipelineCreateInfo.pColorBlendState = &colorBlending;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.layout = m_layout;
        pipelineCreateInfo.renderPass = renderPass;
        pipelineCreateInfo.subpass = 0;

        const VkResult pipelineResult = vkCreateGraphicsPipelines(
            device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_pipeline);
        CheckVkResult(pipelineResult, "vkCreateGraphicsPipelines");

        // vertModule/fragModule are destroyed here, automatically, once
        // this constructor returns - the pipeline has already consumed
        // them by this point.
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
    {
        if (m_pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(m_device, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }
        if (m_layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(m_device, m_layout, nullptr);
            m_layout = VK_NULL_HANDLE;
        }
    }
}
