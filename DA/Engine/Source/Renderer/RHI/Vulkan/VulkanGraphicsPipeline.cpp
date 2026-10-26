#include "PCH.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanDevice.h"
#include "VulkanShaderModule.h"

#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    std::unique_ptr<VulkanGraphicsPipeline> VulkanGraphicsPipeline::Create(const VulkanDevice& device,
                                                                           const VulkanGraphicsPipelineDesc& desc) {
        std::unique_ptr<VulkanGraphicsPipeline> pipeline(new VulkanGraphicsPipeline(device));
        if (!pipeline->Init(desc)) return nullptr;
        return pipeline;
    }

    bool VulkanGraphicsPipeline::Init(const VulkanGraphicsPipelineDesc& desc) {
        if (desc.ColorFormat == VK_FORMAT_UNDEFINED) {
            DA_ERROR("Graphics pipeline needs a color format.");
            return false;
        }

        // Shader modules only need to live until the pipeline is created.
        auto vert = VulkanShaderModule::Create(m_device, desc.VertexShader);
        auto frag = VulkanShaderModule::Create(m_device, desc.FragmentShader);
        if (!vert || !frag) return false;

        // Layout
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = static_cast<uint32_t>(desc.SetLayouts.size());
        layoutInfo.pSetLayouts = desc.SetLayouts.data();
        layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(desc.PushConstants.size());
        layoutInfo.pPushConstantRanges = desc.PushConstants.data();
        if (VkResult r = vkCreatePipelineLayout(m_device.GetHandle(), &layoutInfo, nullptr, &m_layout);
            r != VK_SUCCESS) {
            DA_ERROR("vkCreatePipelineLayout failed: {}", string_VkResult(r));
            m_layout = VK_NULL_HANDLE;
            return false;
        }

        // Fixed-function state
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vert->GetHandle();
        stages[0].pName = "main";
        stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = frag->GetHandle();
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(desc.VertexBindings.size());
        vertexInput.pVertexBindingDescriptions = desc.VertexBindings.data();
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(desc.VertexAttributes.size());
        vertexInput.pVertexAttributeDescriptions = desc.VertexAttributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = desc.Topology;

        // Counts only: the actual viewport and scissor are dynamic.
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = desc.PolygonMode;
        raster.cullMode = desc.CullMode;
        raster.frontFace = desc.FrontFace;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (desc.BlendEnable) {
            blendAttachment.blendEnable = VK_TRUE;
            blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        constexpr std::array dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();

        // Dynamic rendering: attachment formats go here, not in a render pass.
        VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &desc.ColorFormat;

        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.pNext = &rendering;
        info.stageCount = static_cast<uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = m_layout;
        info.renderPass = VK_NULL_HANDLE;

        if (VkResult r =
                vkCreateGraphicsPipelines(m_device.GetHandle(), VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline);
            r != VK_SUCCESS) {
            DA_ERROR("vkCreateGraphicsPipelines failed: {}", string_VkResult(r));
            m_pipeline = VK_NULL_HANDLE;
            return false;
        }
        return true;
    }

    VulkanGraphicsPipeline::~VulkanGraphicsPipeline() {
        if (m_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device.GetHandle(), m_pipeline, nullptr);
        if (m_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device.GetHandle(), m_layout, nullptr);
    }
} // namespace DA
