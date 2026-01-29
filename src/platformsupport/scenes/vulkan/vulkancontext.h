/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QStack>
#include <memory>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanBackend;
class VulkanBuffer;
class VulkanFramebuffer;
class VulkanPipelineManager;
class VulkanTexture;
struct DmaBufAttributes;

/**
 * @brief Manages Vulkan rendering context including command pools, descriptor pools, and pipelines.
 *
 * VulkanContext is analogous to EglContext in the OpenGL backend. It manages per-thread
 * Vulkan resources and provides the interface for command buffer allocation, descriptor
 * set management, and pipeline access.
 */
class KWIN_EXPORT VulkanContext
{
public:
    explicit VulkanContext(VulkanBackend *backend);
    ~VulkanContext();

    /**
     * @brief Make this context current for the calling thread.
     */
    bool makeCurrent();

    /**
     * @brief Release this context from the calling thread.
     */
    void doneCurrent();

    /**
     * @brief Check if context is valid and ready for use.
     */
    bool isValid() const;

    /**
     * @brief Get the backend this context belongs to.
     */
    VulkanBackend *backend() const;

    /**
     * @brief Get the command pool for allocating command buffers.
     */
    VkCommandPool commandPool() const;

    /**
     * @brief Get the descriptor pool for allocating descriptor sets.
     */
    VkDescriptorPool descriptorPool() const;

    /**
     * @brief Get the pipeline manager for shader/pipeline access.
     */
    VulkanPipelineManager *pipelineManager() const;

    /**
     * @brief Get the streaming vertex buffer for per-frame geometry.
     */
    VulkanBuffer *streamingBuffer() const;

    /**
     * @brief Allocate a primary command buffer from the command pool.
     */
    VkCommandBuffer allocateCommandBuffer();

    /**
     * @brief Free a command buffer back to the pool.
     */
    void freeCommandBuffer(VkCommandBuffer commandBuffer);

    /**
     * @brief Begin a single-time command buffer for immediate submission.
     */
    VkCommandBuffer beginSingleTimeCommands();

    /**
     * @brief End and submit a single-time command buffer.
     */
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    /**
     * @brief Allocate a descriptor set from the descriptor pool.
     */
    VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);

    /**
     * @brief Reset the descriptor pool, freeing all allocated descriptor sets.
     * Call at the start of each frame before allocating new descriptor sets.
     */
    void resetDescriptorPool();

    /**
     * @brief Import a DMA-BUF as a Vulkan texture (if supported).
     */
    std::unique_ptr<VulkanTexture> importDmaBufAsTexture(const DmaBufAttributes &attributes);

    /**
     * @brief Check if DMA-BUF import is supported.
     */
    bool supportsDmaBufImport() const;

    /**
     * @brief Push a framebuffer onto the FBO stack.
     */
    void pushFramebuffer(VulkanFramebuffer *fbo);

    /**
     * @brief Pop a framebuffer from the FBO stack.
     */
    VulkanFramebuffer *popFramebuffer();

    /**
     * @brief Get the current framebuffer.
     */
    VulkanFramebuffer *currentFramebuffer();

    /**
     * @brief Get the currently active context for this thread.
     */
    static VulkanContext *currentContext();

    /**
     * @brief Get or create a fence for synchronization.
     */
    VkFence getOrCreateFence();

    /**
     * @brief Create an exportable fence (for sync fd export).
     * @return The fence handle, or VK_NULL_HANDLE if creation failed or not supported.
     */
    VkFence createExportableFence();

    /**
     * @brief Export a fence to a sync file descriptor.
     * @param fence The fence to export (must be created with createExportableFence).
     * @return The sync file descriptor, or invalid if export failed.
     */
    FileDescriptor exportFenceToSyncFd(VkFence fence);

    /**
     * @brief Check if external fence fd export is supported.
     */
    bool supportsExternalFenceFd() const;

private:
    bool createCommandPool();
    bool createDescriptorPool();
    void cleanup();

    VulkanBackend *m_backend;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;

    std::unique_ptr<VulkanPipelineManager> m_pipelineManager;
    std::unique_ptr<VulkanBuffer> m_streamingBuffer;

    QStack<VulkanFramebuffer *> m_framebufferStack;

    bool m_supportsDmaBufImport = false;

    // Track descriptor allocations for proactive pool reset
    // With ~90 nodes/frame across 3 monitors, 10000 sets lasts ~110 frames (~1.8s at 60fps)
    // Reset at 80% = 8000 allocations, triggers vkDeviceWaitIdle for safe reset
    uint32_t m_descriptorAllocCount = 0;
    static constexpr uint32_t DESCRIPTOR_POOL_MAX_SETS = 10000;
    static constexpr uint32_t DESCRIPTOR_POOL_RESET_THRESHOLD = 8000; // Reset at 80%

    static VulkanContext *s_currentContext;
};

} // namespace KWin
