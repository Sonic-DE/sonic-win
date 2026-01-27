/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QVector2D>
#include <memory>
#include <optional>
#include <span>
#include <vulkan/vulkan.h>

// Forward declare VMA types to avoid including the full header
struct VmaAllocator_T;
typedef VmaAllocator_T *VmaAllocator;
struct VmaAllocation_T;
typedef VmaAllocation_T *VmaAllocation;

namespace KWin
{

class VulkanContext;
class VulkanBackend;

/**
 * @brief Vertex attribute description for Vulkan vertex input.
 */
struct VulkanVertexAttrib
{
    uint32_t location;
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
};

/**
 * @brief Standard 2D vertex structure matching GLVertex2D.
 */
struct VulkanVertex2D
{
    QVector2D position;
    QVector2D texcoord;
};

/**
 * @brief Vulkan buffer object with VMA-managed memory.
 *
 * This class provides a similar interface to GLVertexBuffer but uses
 * Vulkan Memory Allocator (VMA) for efficient memory management.
 */
class KWIN_EXPORT VulkanBuffer
{
public:
    /**
     * @brief Buffer usage hints.
     */
    enum class UsageHint {
        Vertex, ///< Vertex buffer
        Index, ///< Index buffer
        Uniform, ///< Uniform buffer
        Staging, ///< Staging buffer for CPU->GPU transfers
        Storage, ///< Storage buffer (SSBO)
    };

    /**
     * @brief Memory location hints.
     */
    enum class MemoryHint {
        DeviceLocal, ///< GPU-only memory (fastest for GPU access)
        HostVisible, ///< CPU-visible memory (for streaming data)
        HostCached, ///< CPU-cached memory (for readback)
    };

    ~VulkanBuffer();

    // Non-copyable
    VulkanBuffer(const VulkanBuffer &) = delete;
    VulkanBuffer &operator=(const VulkanBuffer &) = delete;

    // Movable
    VulkanBuffer(VulkanBuffer &&other) noexcept;
    VulkanBuffer &operator=(VulkanBuffer &&other) noexcept;

    /**
     * @brief Create a vertex buffer.
     */
    static std::unique_ptr<VulkanBuffer> createVertexBuffer(VulkanContext *context, VkDeviceSize size, MemoryHint memoryHint = MemoryHint::DeviceLocal);

    /**
     * @brief Create an index buffer.
     */
    static std::unique_ptr<VulkanBuffer> createIndexBuffer(VulkanContext *context, VkDeviceSize size, MemoryHint memoryHint = MemoryHint::DeviceLocal);

    /**
     * @brief Create a uniform buffer.
     */
    static std::unique_ptr<VulkanBuffer> createUniformBuffer(VulkanContext *context, VkDeviceSize size);

    /**
     * @brief Create a staging buffer for CPU->GPU transfers.
     */
    static std::unique_ptr<VulkanBuffer> createStagingBuffer(VulkanContext *context, VkDeviceSize size);

    /**
     * @brief Create a streaming vertex buffer (persistent mapped, host visible).
     */
    static std::unique_ptr<VulkanBuffer> createStreamingBuffer(VulkanContext *context, VkDeviceSize size);

    /**
     * @brief Get the Vulkan buffer handle.
     */
    VkBuffer buffer() const
    {
        return m_buffer;
    }

    /**
     * @brief Get the buffer size in bytes.
     */
    VkDeviceSize size() const
    {
        return m_size;
    }

    /**
     * @brief Check if the buffer is valid.
     */
    bool isValid() const
    {
        return m_buffer != VK_NULL_HANDLE;
    }

    /**
     * @brief Upload data to the buffer.
     *
     * For device-local buffers, this uses a staging buffer internally.
     * For host-visible buffers, this maps the memory directly.
     */
    void upload(const void *data, VkDeviceSize size, VkDeviceSize offset = 0);

    /**
     * @brief Map the buffer for CPU access.
     *
     * Only valid for host-visible buffers.
     * @return Pointer to mapped memory, or nullptr if mapping failed.
     */
    void *map();

    /**
     * @brief Map a range of the buffer.
     */
    template<typename T>
    std::optional<std::span<T>> map(size_t count)
    {
        void *mapped = map();
        if (mapped && count * sizeof(T) <= m_size) {
            return std::span<T>(static_cast<T *>(mapped), count);
        }
        return std::nullopt;
    }

    /**
     * @brief Unmap the buffer.
     */
    void unmap();

    /**
     * @brief Flush mapped memory to make writes visible to GPU.
     */
    void flush(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    /**
     * @brief Invalidate mapped memory to make GPU writes visible to CPU.
     */
    void invalidate(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

    /**
     * @brief Check if buffer is persistently mapped.
     */
    bool isPersistentlyMapped() const
    {
        return m_persistentlyMapped;
    }

    /**
     * @brief Get the mapped pointer (for persistently mapped buffers).
     */
    void *mappedData() const
    {
        return m_mappedData;
    }

    /**
     * @brief Reset the buffer for reuse (streaming buffers).
     */
    void reset();

    /**
     * @brief Begin a new frame (for streaming buffers).
     */
    void beginFrame();

    /**
     * @brief End frame (for streaming buffers).
     */
    void endFrame();

    /**
     * @brief Get the current write offset (for streaming buffers).
     */
    VkDeviceSize currentOffset() const
    {
        return m_currentOffset;
    }

    /**
     * @brief Allocate space in the streaming buffer.
     *
     * @param size Size in bytes to allocate.
     * @param alignment Required alignment.
     * @return Pointer to allocated memory, or nullptr if buffer is full.
     */
    void *allocate(VkDeviceSize size, VkDeviceSize alignment = 256);

    /**
     * @brief Get standard vertex input binding description for VulkanVertex2D.
     */
    static VkVertexInputBindingDescription getVertex2DBindingDescription();

    /**
     * @brief Get standard vertex attribute descriptions for VulkanVertex2D.
     */
    static std::array<VkVertexInputAttributeDescription, 2> getVertex2DAttributeDescriptions();

private:
    VulkanBuffer(VulkanContext *context, VkBuffer buffer, VmaAllocation allocation,
                 VkDeviceSize size, UsageHint usage, bool persistentlyMapped, void *mappedData);

    static std::unique_ptr<VulkanBuffer> create(VulkanContext *context, VkDeviceSize size,
                                                VkBufferUsageFlags usage, MemoryHint memoryHint,
                                                UsageHint usageHint, bool persistentMap = false);

    VulkanContext *m_context;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    VkDeviceSize m_size = 0;
    UsageHint m_usage;
    bool m_persistentlyMapped = false;
    void *m_mappedData = nullptr;
    VkDeviceSize m_currentOffset = 0;
};

/**
 * @brief Global VMA allocator management.
 */
class KWIN_EXPORT VulkanAllocator
{
public:
    static bool initialize(VulkanBackend *backend);
    static void shutdown();
    static VmaAllocator allocator();
    static bool isInitialized();

private:
    static VmaAllocator s_allocator;
    static bool s_initialized;
};

} // namespace KWin
