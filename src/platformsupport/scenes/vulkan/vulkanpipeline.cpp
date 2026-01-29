/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkanpipeline.h"
#include "utils/common.h"
#include "vulkanbackend.h"
#include "vulkanbuffer.h"
#include "vulkancontext.h"

#include <QDebug>
#include <array>

namespace KWin
{

VulkanPipeline::VulkanPipeline(VulkanContext *context, ShaderTraits traits)
    : m_context(context)
    , m_traits(traits)
{
}

VulkanPipeline::~VulkanPipeline()
{
    cleanup();
}

void VulkanPipeline::cleanup()
{
    VkDevice device = m_context->backend()->device();
    if (device == VK_NULL_HANDLE) {
        return;
    }

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

std::unique_ptr<VulkanPipeline> VulkanPipeline::create(VulkanContext *context, VkRenderPass renderPass,
                                                       ShaderTraits traits,
                                                       const QByteArray &vertexShaderSpirv,
                                                       const QByteArray &fragmentShaderSpirv)
{
    auto pipeline = std::unique_ptr<VulkanPipeline>(new VulkanPipeline(context, traits));

    if (!pipeline->createDescriptorSetLayout()) {
        return nullptr;
    }

    if (!pipeline->createPipelineLayout()) {
        return nullptr;
    }

    if (!pipeline->createPipeline(renderPass, vertexShaderSpirv, fragmentShaderSpirv)) {
        return nullptr;
    }

    return pipeline;
}

bool VulkanPipeline::createDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // Binding 0: Texture sampler
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Uniform buffer for fragment shader parameters
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(m_context->backend()->device(), &layoutInfo,
                                                  nullptr, &m_descriptorSetLayout);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create descriptor set layout:" << result;
        return false;
    }

    return true;
}

bool VulkanPipeline::createPipelineLayout()
{
    // Push constant range for MVP and texture matrices
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(VulkanPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkResult result = vkCreatePipelineLayout(m_context->backend()->device(), &pipelineLayoutInfo,
                                             nullptr, &m_layout);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create pipeline layout:" << result;
        return false;
    }

    return true;
}

VkShaderModule VulkanPipeline::createShaderModule(const QByteArray &spirv)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(spirv.constData());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(m_context->backend()->device(), &createInfo,
                                           nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create shader module:" << result;
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

bool VulkanPipeline::createPipeline(VkRenderPass renderPass,
                                    const QByteArray &vertexShaderSpirv,
                                    const QByteArray &fragmentShaderSpirv)
{
    // Create shader modules
    VkShaderModule vertModule = createShaderModule(vertexShaderSpirv);
    VkShaderModule fragModule = createShaderModule(fragmentShaderSpirv);

    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE) {
        if (vertModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_context->backend()->device(), vertModule, nullptr);
        }
        if (fragModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_context->backend()->device(), fragModule, nullptr);
        }
        return false;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertModule;
    vertShaderStageInfo.pName = "main";

    // Specialization constants for fragment shader
    std::array<VkSpecializationMapEntry, 7> specEntries{};
    std::array<VkBool32, 7> specData{};

    specData[0] = (m_traits & ShaderTrait::MapTexture) ? VK_TRUE : VK_FALSE;
    specData[1] = (m_traits & ShaderTrait::UniformColor) ? VK_TRUE : VK_FALSE;
    specData[2] = (m_traits & ShaderTrait::Modulate) ? VK_TRUE : VK_FALSE;
    specData[3] = (m_traits & ShaderTrait::AdjustSaturation) ? VK_TRUE : VK_FALSE;
    specData[4] = (m_traits & ShaderTrait::TransformColorspace) ? VK_TRUE : VK_FALSE;
    specData[5] = (m_traits & ShaderTrait::RoundedCorners) ? VK_TRUE : VK_FALSE;
    specData[6] = (m_traits & ShaderTrait::Border) ? VK_TRUE : VK_FALSE;

    for (uint32_t i = 0; i < 7; ++i) {
        specEntries[i].constantID = i;
        specEntries[i].offset = i * sizeof(VkBool32);
        specEntries[i].size = sizeof(VkBool32);
    }

    VkSpecializationInfo specInfo{};
    specInfo.mapEntryCount = static_cast<uint32_t>(specEntries.size());
    specInfo.pMapEntries = specEntries.data();
    specInfo.dataSize = specData.size() * sizeof(VkBool32);
    specInfo.pData = specData.data();

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = &specInfo;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    auto bindingDescription = VulkanBuffer::getVertex2DBindingDescription();
    auto attributeDescriptions = VulkanBuffer::getVertex2DAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending (premultiplied alpha)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkResult result = vkCreateGraphicsPipelines(m_context->backend()->device(), VK_NULL_HANDLE,
                                                1, &pipelineInfo, nullptr, &m_pipeline);

    // Cleanup shader modules
    vkDestroyShaderModule(m_context->backend()->device(), vertModule, nullptr);
    vkDestroyShaderModule(m_context->backend()->device(), fragModule, nullptr);

    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create graphics pipeline:" << result;
        return false;
    }

    return true;
}

void VulkanPipeline::bind(VkCommandBuffer cmd) const
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
}

} // namespace KWin
