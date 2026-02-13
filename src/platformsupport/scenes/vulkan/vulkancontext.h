/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"
#include "utils/filedescriptor.h"

#include <QSize>
#include <QStack>
#include <QVector>
#include <memory>
#include <vulkan/vulkan.h>

struct VmaAllocator_T;
typedef VmaAllocator_T *VmaAllocator;
struct VmaAllocation_T;
typedef VmaAllocation_T *VmaAllocation;

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
     * @brief Import a single DMA-BUF plane as a Vulkan texture.
     * @param attributes The DMA-BUF attributes
     * @param planeIndex The plane index to import (0-3)
     * @param format The Vulkan format for the plane
     * @param size The size of this plane (may differ from overall buffer size for YUV)
     */
    std::unique_ptr<VulkanTexture> importDmaBufPlaneAsTexture(const DmaBufAttributes &attributes,
                                                              int planeIndex,
                                                              VkFormat format,
                                                              const QSize &size);

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

    /**
     * @brief Queue a sampler for deferred destruction.
     * Samplers in use by in-flight command buffers cannot be destroyed immediately.
     * This method queues them for destruction when no longer in use.
     */
    void queueSamplerForDestruction(VkSampler sampler);

    /**
     * @brief Queue a buffer for deferred destruction.
     * Buffers in use by in-flight command buffers cannot be destroyed immediately.
     * This method queues them for destruction when no longer in use.
     */
    void queueBufferForDestruction(VkBuffer buffer, VmaAllocation allocation);

    /**
     * @brief Clean up any pending resources that are no longer in use.
     * Called at the start of each frame to safely destroy resources from previous frames.
     */
    void cleanupPendingResources();

    /**
     * @brief Queue an image view for deferred destruction.
     * Image views must be destroyed before their parent images.
     */
    void queueImageViewForDestruction(VkImageView imageView);

    /**
     * @brief Queue an image for deferred destruction.
     * Images can only be destroyed after all their views are destroyed.
     * For raw Vulkan memory (non-VMA), pass deviceMemory to have it freed after image destruction.
     */
    void queueImageForDestruction(VkImage image, VmaAllocation allocation = nullptr, VkDeviceMemory deviceMemory = VK_NULL_HANDLE);

    /**
     * @brief Queue both image view and image for deferred destruction in correct order.
     * This is the recommended way to destroy image+view pairs.
     */
    void queueImageAndViewForDestruction(VkImageView imageView, VkImage image);

private:
    bool createCommandPool();
    bool createDescriptorPool();
    void cleanup();

    VulkanBackend *m_backend;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
    bool m_isCleaningUp = false; // Set to true during cleanup to destroy immediately

    std::unique_ptr<VulkanPipelineManager> m_pipelineManager;
    std::unique_ptr<VulkanBuffer> m_streamingBuffer;

    QStack<VulkanFramebuffer *> m_framebufferStack;

    bool m_supportsDmaBufImport = false;

    // Track descriptor allocations for debugging
    // Pool size is calculated dynamically based on output count: outputs * 15000
    // With pool resetting each frame, this provides headroom for multi-monitor setups
    uint32_t m_descriptorAllocCount = 0;
    uint32_t m_descriptorPoolMaxSets = 0;
    static constexpr uint32_t DESCRIPTOR_POOL_SETS_PER_OUTPUT = 15000;

    // Deferred sampler destruction queue (samplers in use by in-flight command buffers)
    QVector<std::pair<VkSampler, VkFence>> m_pendingSamplerDestructions;

    // Deferred image destruction queue (images waiting for their views to be destroyed)
    struct PendingImageDestruction
    {
        VkImage image;
        VkFence fence;
        VmaAllocation allocation; // For VMA-managed memory - used with vmaDestroyImage
        VkDeviceMemory deviceMemory; // For raw Vulkan memory (non-VMA) - freed after image
    };
    QVector<PendingImageDestruction> m_pendingImageDestructions;

    // Deferred image view destruction queue (views must be destroyed before images)
    struct PendingImageViewDestruction
    {
        VkImageView imageView;
        VkFence fence;
        VkImage parentImage; // Image this view belongs to (for tracking)
    };
    QVector<PendingImageViewDestruction> m_pendingImageViewDestructions;

    // Deferred buffer destruction queue (buffers in use by in-flight command buffers)
    struct PendingBufferDestruction
    {
        VkBuffer buffer;
        VmaAllocation allocation;
        VkFence fence;
    };
    QVector<PendingBufferDestruction> m_pendingBufferDestructions;

    static VulkanContext *s_currentContext;
};

} // namespace KWin
