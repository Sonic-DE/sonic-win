/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkanframebuffer.h"
#include "utils/common.h"
#include "vulkanbackend.h"
#include "vulkancontext.h"
#include "vulkanrenderpass.h"
#include "vulkantexture.h"

#include <QDebug>
#include <QRect>

namespace KWin
{

VulkanFramebuffer::VulkanFramebuffer(VulkanContext *context, VulkanRenderPass *renderPass, const QSize &size)
    : m_context(context)
    , m_renderPass(renderPass)
    , m_size(size)
{
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    cleanup();
}

void VulkanFramebuffer::cleanup()
{
    if (m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_context->backend()->device(), m_framebuffer, nullptr);
        m_framebuffer = VK_NULL_HANDLE;
    }

    m_colorTexture.reset();
    m_depthTexture.reset();
}

std::unique_ptr<VulkanFramebuffer> VulkanFramebuffer::create(VulkanContext *context, VulkanRenderPass *renderPass,
                                                             VkImageView colorAttachment, const QSize &size)
{
    auto fb = std::unique_ptr<VulkanFramebuffer>(new VulkanFramebuffer(context, renderPass, size));

    std::vector<VkImageView> attachments = {colorAttachment};

    if (!fb->createFramebuffer(attachments)) {
        return nullptr;
    }

    return fb;
}

std::unique_ptr<VulkanFramebuffer> VulkanFramebuffer::create(VulkanContext *context, VulkanRenderPass *renderPass,
                                                             VkImageView colorAttachment, VkImageView depthAttachment,
                                                             const QSize &size)
{
    auto fb = std::unique_ptr<VulkanFramebuffer>(new VulkanFramebuffer(context, renderPass, size));

    std::vector<VkImageView> attachments = {colorAttachment, depthAttachment};

    if (!fb->createFramebuffer(attachments)) {
        return nullptr;
    }

    return fb;
}

std::unique_ptr<VulkanFramebuffer> VulkanFramebuffer::createWithTexture(VulkanContext *context, VulkanRenderPass *renderPass,
                                                                        const QSize &size, VkFormat format)
{
    auto fb = std::unique_ptr<VulkanFramebuffer>(new VulkanFramebuffer(context, renderPass, size));

    // Create color texture
    fb->m_colorTexture = VulkanTexture::createRenderTarget(context, size, format);
    if (!fb->m_colorTexture) {
        qCWarning(KWIN_CORE) << "Failed to create color texture for framebuffer";
        return nullptr;
    }

    std::vector<VkImageView> attachments = {fb->m_colorTexture->imageView()};

    // Create depth texture if needed
    if (renderPass->config().hasDepth) {
        fb->m_depthTexture = VulkanTexture::createDepthStencil(context, size);
        if (!fb->m_depthTexture) {
            qCWarning(KWIN_CORE) << "Failed to create depth texture for framebuffer";
            return nullptr;
        }
        attachments.push_back(fb->m_depthTexture->imageView());
    }

    if (!fb->createFramebuffer(attachments)) {
        return nullptr;
    }

    return fb;
}

bool VulkanFramebuffer::createFramebuffer(const std::vector<VkImageView> &attachments)
{
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass->renderPass();
    framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebufferInfo.pAttachments = attachments.data();
    framebufferInfo.width = static_cast<uint32_t>(m_size.width());
    framebufferInfo.height = static_cast<uint32_t>(m_size.height());
    framebufferInfo.layers = 1;

    VkResult result = vkCreateFramebuffer(m_context->backend()->device(), &framebufferInfo,
                                          nullptr, &m_framebuffer);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create framebuffer:" << result;
        return false;
    }

    qCDebug(KWIN_CORE) << "Successfully created framebuffer with" << attachments.size() << "attachments";
    return true;
}

VkRect2D VulkanFramebuffer::renderArea() const
{
    VkRect2D area{};
    area.offset = {0, 0};
    area.extent = {static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height())};
    return area;
}

void VulkanFramebuffer::beginRenderPass(VkCommandBuffer cmd, const VkClearValue *clearValues, uint32_t clearValueCount)
{
    m_renderPass->begin(cmd, m_framebuffer, renderArea(), clearValues, clearValueCount);
}

void VulkanFramebuffer::endRenderPass(VkCommandBuffer cmd)
{
    m_renderPass->end(cmd);
}

void VulkanFramebuffer::bind()
{
    m_context->pushFramebuffer(this);
}

void VulkanFramebuffer::unbind()
{
    m_context->popFramebuffer();
}

void VulkanFramebuffer::blitFrom(VkCommandBuffer cmd, VulkanFramebuffer *source, const QRect &sourceRect,
                                 const QRect &destRect, VkFilter filter)
{
    if (!source || !source->m_colorTexture || !m_colorTexture) {
        qCWarning(KWIN_CORE) << "Cannot blit: source or destination texture missing";
        return;
    }

    VkImage srcImage = source->m_colorTexture->image();
    VkImage dstImage = m_colorTexture->image();

    // Transition source image to transfer source
    VkImageMemoryBarrier srcBarrier{};
    srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    srcBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.image = srcImage;
    srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    srcBarrier.subresourceRange.baseMipLevel = 0;
    srcBarrier.subresourceRange.levelCount = 1;
    srcBarrier.subresourceRange.baseArrayLayer = 0;
    srcBarrier.subresourceRange.layerCount = 1;
    srcBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    // Transition destination image to transfer destination
    VkImageMemoryBarrier dstBarrier = srcBarrier;
    dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstBarrier.image = dstImage;
    dstBarrier.srcAccessMask = 0;
    dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    std::array<VkImageMemoryBarrier, 2> barriers = {srcBarrier, dstBarrier};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(barriers.size()), barriers.data());

    // Perform blit
    VkImageBlit blitRegion{};
    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcOffsets[0] = {sourceRect.x(), sourceRect.y(), 0};
    blitRegion.srcOffsets[1] = {sourceRect.x() + sourceRect.width(), sourceRect.y() + sourceRect.height(), 1};

    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.mipLevel = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstOffsets[0] = {destRect.x(), destRect.y(), 0};
    blitRegion.dstOffsets[1] = {destRect.x() + destRect.width(), destRect.y() + destRect.height(), 1};

    vkCmdBlitImage(cmd,
                   srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blitRegion, filter);

    // Transition images back
    srcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    srcBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    barriers = {srcBarrier, dstBarrier};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(barriers.size()), barriers.data());
}

} // namespace KWin
