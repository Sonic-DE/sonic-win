/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkanrenderpass.h"
#include "utils/common.h"
#include "vulkanbackend.h"
#include "vulkancontext.h"

#include <QDebug>
#include <array>
#include <vector>

namespace KWin
{

VulkanRenderPass::VulkanRenderPass(VulkanContext *context, const Config &config)
    : m_context(context)
    , m_config(config)
{
}

VulkanRenderPass::~VulkanRenderPass()
{
    cleanup();
}

void VulkanRenderPass::cleanup()
{
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_context->backend()->device(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
}

std::unique_ptr<VulkanRenderPass> VulkanRenderPass::createForPresentation(VulkanContext *context, VkFormat colorFormat)
{
    Config config;
    config.colorFormat = colorFormat;
    config.colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    config.colorStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    config.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    config.hasDepth = false;

    return create(context, config);
}

std::unique_ptr<VulkanRenderPass> VulkanRenderPass::createForOffscreen(VulkanContext *context, VkFormat colorFormat,
                                                                       bool withDepth)
{
    Config config;
    config.colorFormat = colorFormat;
    config.colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    config.colorStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    config.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    config.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    config.hasDepth = withDepth;

    if (withDepth) {
        // Find a suitable depth format
        const std::array<VkFormat, 3> depthFormats = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };

        for (VkFormat format : depthFormats) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(context->backend()->physicalDevice(), format, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                config.depthFormat = format;
                break;
            }
        }
    }

    return create(context, config);
}

std::unique_ptr<VulkanRenderPass> VulkanRenderPass::create(VulkanContext *context, const Config &config)
{
    auto renderPass = std::unique_ptr<VulkanRenderPass>(new VulkanRenderPass(context, config));

    if (!renderPass->create()) {
        return nullptr;
    }

    return renderPass;
}

bool VulkanRenderPass::create()
{
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorRefs;
    VkAttachmentReference depthRef{};

    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_config.colorFormat;
    colorAttachment.samples = m_config.samples;
    colorAttachment.loadOp = m_config.colorLoadOp;
    colorAttachment.storeOp = m_config.colorStoreOp;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = m_config.initialLayout;
    colorAttachment.finalLayout = m_config.finalLayout;

    attachments.push_back(colorAttachment);

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs.push_back(colorRef);

    // Depth attachment (optional)
    if (m_config.hasDepth && m_config.depthFormat != VK_FORMAT_UNDEFINED) {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = m_config.depthFormat;
        depthAttachment.samples = m_config.samples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments.push_back(depthAttachment);

        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    if (m_config.hasDepth && m_config.depthFormat != VK_FORMAT_UNDEFINED) {
        subpass.pDepthStencilAttachment = &depthRef;
    }

    // Subpass dependencies
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (m_config.hasDepth) {
        dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    // Create render pass
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(m_context->backend()->device(), &renderPassInfo,
                                         nullptr, &m_renderPass);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create render pass:" << result;
        return false;
    }

    qCDebug(KWIN_CORE) << "Successfully created render pass for presentation";
    return true;
}

void VulkanRenderPass::begin(VkCommandBuffer cmd, VkFramebuffer framebuffer, const VkRect2D &renderArea,
                             const VkClearValue *clearValues, uint32_t clearValueCount)
{
    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = m_renderPass;
    beginInfo.framebuffer = framebuffer;
    beginInfo.renderArea = renderArea;
    beginInfo.clearValueCount = clearValueCount;
    beginInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderPass::end(VkCommandBuffer cmd)
{
    vkCmdEndRenderPass(cmd);
}

} // namespace KWin
