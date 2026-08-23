#include "VulkanGraphicsPipeline.hpp"

#include "VulkanPushConstants.hpp"
#include "VulkanResult.hpp"
#include "VulkanShaderModule.hpp"
#include "VulkanVertex.hpp"

#include <array>
#include <cstdint>
#include <string>

#ifndef ARENGINE_SHADER_DIR
#error "ARENGINE_SHADER_DIR must be defined by the build system (see engine/rendering/CMakeLists.txt) - it's where CMake compiles triangle.vert/triangle.frag to SPIR-V"
#endif

namespace AREngine::Rendering::Vulkan
{
    VulkanGraphicsPipeline::VulkanGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout)
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

        // One binding, three attributes (position, color, uv) - matches
        // Rendering::MeshVertex exactly (VulkanVertex.hpp/.cpp) and
        // triangle.vert's `layout(location = 0/1/2) in ...`
        // declarations. See docs/ARCHITECTURE.md, "Vertex Input Layout
        // (M8D)" and "Vertex Format Review (M8H)".
        const VkVertexInputBindingDescription bindingDescription = GetVertexBindingDescription();
        const std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = GetVertexAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDescription;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescriptions.size());
        vertexInput.pVertexAttributeDescriptions = attributeDescriptions.data();

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

        // Back-face culling enabled as of M8H (was NONE through M8G) -
        // see docs/ARCHITECTURE.md, "Back-Face Culling (M8H)" for the
        // full winding-convention derivation. frontFace stays CLOCKWISE,
        // unchanged since M8C: every procedural mesh
        // (ProceduralMesh.cpp) winds its faces counter-clockwise as
        // seen from outside/in front, which the Vulkan Y-flip
        // (ApplyVulkanYFlip, applied to the projection matrix) turns
        // into clockwise in actual screen space - exactly this
        // frontFace value.
        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Standard depth test: closer fragments (smaller Z) win, and
        // every passing fragment writes its own depth so later,
        // farther-away fragments at the same pixel correctly lose to
        // it. See docs/ARCHITECTURE.md, "Depth Compare / Clear Values
        // (M8F)" for why LESS pairs with a clear value of 1.0. No
        // depth bounds test, no stencil test - neither is needed here.
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

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

        // One descriptor set layout (the combined-image-sampler layout
        // from VulkanDescriptorSetLayout) and one push constant range
        // (MvpPushConstants - the MVP matrix and a tint color, visible
        // to both stages since the vertex shader reads mvp and the
        // fragment shader reads tint). See docs/ARCHITECTURE.md,
        // "Pipeline Layout (M8C)", "Pipeline Layout Change (M8E)", and
        // "Transform Upload Method (M8F)".
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(MvpPushConstants);

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &descriptorSetLayout;
        layoutCreateInfo.pushConstantRangeCount = 1;
        layoutCreateInfo.pPushConstantRanges = &pushConstantRange;

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
        pipelineCreateInfo.pDepthStencilState = &depthStencil;
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
