/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QRect>
#include <QSize>
#include <memory>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanContext;
class VulkanRenderPass;
class VulkanTexture;

/**
 * @brief Vulkan framebuffer wrapper.
 *
 * Similar to GLFramebuffer, this encapsulates a VkFramebuffer with its attachments.
 */
class KWIN_EXPORT VulkanFramebuffer
{
public:
    ~VulkanFramebuffer();

    // Non-copyable
    VulkanFramebuffer(const VulkanFramebuffer &) = delete;
    VulkanFramebuffer &operator=(const VulkanFramebuffer &) = delete;

    /**
     * @brief Create a framebuffer with a color attachment only.
     */
    static std::unique_ptr<VulkanFramebuffer> create(VulkanContext *context, VulkanRenderPass *renderPass,
                                                     VkImageView colorAttachment, const QSize &size);

    /**
     * @brief Create a framebuffer with color and depth attachments.
     */
    static std::unique_ptr<VulkanFramebuffer> create(VulkanContext *context, VulkanRenderPass *renderPass,
                                                     VkImageView colorAttachment, VkImageView depthAttachment,
                                                     const QSize &size);

    /**
     * @brief Create a framebuffer that owns its color texture.
     */
    static std::unique_ptr<VulkanFramebuffer> createWithTexture(VulkanContext *context, VulkanRenderPass *renderPass,
                                                                const QSize &size, VkFormat format);

    /**
     * @brief Check if the framebuffer is valid.
     */
    bool isValid() const
    {
        return m_framebuffer != VK_NULL_HANDLE;
    }

    /**
     * @brief Get the Vulkan framebuffer handle.
     */
    VkFramebuffer framebuffer() const
    {
        return m_framebuffer;
    }

    /**
     * @brief Get the associated render pass.
     */
    VulkanRenderPass *renderPass() const
    {
        return m_renderPass;
    }

    /**
     * @brief Get the framebuffer size.
     */
    QSize size() const
    {
        return m_size;
    }

    /**
     * @brief Get the framebuffer width.
     */
    int width() const
    {
        return m_size.width();
    }

    /**
     * @brief Get the framebuffer height.
     */
    int height() const
    {
        return m_size.height();
    }

    /**
     * @brief Get the color attachment texture (if owned).
     */
    std::shared_ptr<VulkanTexture> colorTexture() const
    {
        return m_colorTexture;
    }

    /**
     * @brief Get the depth attachment texture (if owned).
     */
    std::shared_ptr<VulkanTexture> depthTexture() const
    {
        return m_depthTexture;
    }

    /**
     * @brief Get the render area for this framebuffer.
     */
    VkRect2D renderArea() const;

    /**
     * @brief Begin rendering to this framebuffer.
     */
    void beginRenderPass(VkCommandBuffer cmd, const VkClearValue *clearValues, uint32_t clearValueCount);

    /**
     * @brief End rendering to this framebuffer.
     */
    void endRenderPass(VkCommandBuffer cmd);

    /**
     * @brief Push this framebuffer onto the context's FBO stack.
     */
    void bind();

    /**
     * @brief Pop this framebuffer from the context's FBO stack.
     */
    void unbind();

    /**
     * @brief Copy from another framebuffer using a blit operation.
     */
    void blitFrom(VkCommandBuffer cmd, VulkanFramebuffer *source, const QRect &sourceRect,
                  const QRect &destRect, VkFilter filter = VK_FILTER_LINEAR);

private:
    VulkanFramebuffer(VulkanContext *context, VulkanRenderPass *renderPass, const QSize &size);

    bool createFramebuffer(const std::vector<VkImageView> &attachments);
    void cleanup();

    VulkanContext *m_context;
    VulkanRenderPass *m_renderPass;
    QSize m_size;

    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

    // Owned textures (if created with createWithTexture)
    std::shared_ptr<VulkanTexture> m_colorTexture;
    std::shared_ptr<VulkanTexture> m_depthTexture;
};

} // namespace KWin
