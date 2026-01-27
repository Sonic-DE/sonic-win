/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <memory>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanContext;
class VulkanBackend;

/**
 * @brief Vulkan render pass wrapper.
 *
 * Encapsulates a VkRenderPass with configuration for different rendering scenarios.
 */
class KWIN_EXPORT VulkanRenderPass
{
public:
    /**
     * @brief Render pass configuration options.
     */
    struct Config
    {
        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentStoreOp colorStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        bool hasDepth = false;
    };

    ~VulkanRenderPass();

    // Non-copyable
    VulkanRenderPass(const VulkanRenderPass &) = delete;
    VulkanRenderPass &operator=(const VulkanRenderPass &) = delete;

    /**
     * @brief Create a render pass for presentation (swapchain).
     */
    static std::unique_ptr<VulkanRenderPass> createForPresentation(VulkanContext *context, VkFormat colorFormat);

    /**
     * @brief Create a render pass for offscreen rendering.
     */
    static std::unique_ptr<VulkanRenderPass> createForOffscreen(VulkanContext *context, VkFormat colorFormat,
                                                                bool withDepth = false);

    /**
     * @brief Create a render pass with custom configuration.
     */
    static std::unique_ptr<VulkanRenderPass> create(VulkanContext *context, const Config &config);

    /**
     * @brief Check if the render pass is valid.
     */
    bool isValid() const
    {
        return m_renderPass != VK_NULL_HANDLE;
    }

    /**
     * @brief Get the Vulkan render pass handle.
     */
    VkRenderPass renderPass() const
    {
        return m_renderPass;
    }

    /**
     * @brief Get the configuration used to create this render pass.
     */
    const Config &config() const
    {
        return m_config;
    }

    /**
     * @brief Begin a render pass on a command buffer.
     */
    void begin(VkCommandBuffer cmd, VkFramebuffer framebuffer, const VkRect2D &renderArea,
               const VkClearValue *clearValues, uint32_t clearValueCount);

    /**
     * @brief End a render pass on a command buffer.
     */
    void end(VkCommandBuffer cmd);

private:
    VulkanRenderPass(VulkanContext *context, const Config &config);

    bool create();
    void cleanup();

    VulkanContext *m_context;
    Config m_config;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};

} // namespace KWin
