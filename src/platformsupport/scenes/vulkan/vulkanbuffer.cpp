/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// VMA implementation - must be defined exactly once
#define VMA_IMPLEMENTATION
#include "vma_usage.h"

#include "utils/common.h"
#include "vulkanbackend.h"
#include "vulkanbuffer.h"
#include "vulkancontext.h"

#include <QDebug>
#include <cstring>

namespace KWin
{

// Static VMA allocator
VmaAllocator VulkanAllocator::s_allocator = VK_NULL_HANDLE;
bool VulkanAllocator::s_initialized = false;

bool VulkanAllocator::initialize(VulkanBackend *backend)
{
    if (s_initialized) {
        return true;
    }

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = backend->physicalDevice();
    allocatorInfo.device = backend->device();
    allocatorInfo.instance = backend->instance();

    VkResult result = vmaCreateAllocator(&allocatorInfo, &s_allocator);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create VMA allocator:" << result;
        return false;
    }

    s_initialized = true;
    qCDebug(KWIN_CORE) << "VMA allocator initialized successfully";
    return true;
}

void VulkanAllocator::shutdown()
{
    if (s_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(s_allocator);
        s_allocator = VK_NULL_HANDLE;
    }
    s_initialized = false;
}

VmaAllocator VulkanAllocator::allocator()
{
    return s_allocator;
}

bool VulkanAllocator::isInitialized()
{
    return s_initialized;
}

// VulkanBuffer implementation

VulkanBuffer::VulkanBuffer(VulkanContext *context, VkBuffer buffer, VmaAllocation allocation,
                           VkDeviceSize size, UsageHint usage, bool persistentlyMapped, void *mappedData)
    : m_context(context)
    , m_buffer(buffer)
    , m_allocation(allocation)
    , m_size(size)
    , m_usage(usage)
    , m_persistentlyMapped(persistentlyMapped)
    , m_mappedData(mappedData)
{
}

VulkanBuffer::~VulkanBuffer()
{
    if (m_buffer != VK_NULL_HANDLE && VulkanAllocator::isInitialized()) {
        vmaDestroyBuffer(VulkanAllocator::allocator(), m_buffer, m_allocation);
    }
}

VulkanBuffer::VulkanBuffer(VulkanBuffer &&other) noexcept
    : m_context(other.m_context)
    , m_buffer(other.m_buffer)
    , m_allocation(other.m_allocation)
    , m_size(other.m_size)
    , m_usage(other.m_usage)
    , m_persistentlyMapped(other.m_persistentlyMapped)
    , m_mappedData(other.m_mappedData)
    , m_currentOffset(other.m_currentOffset)
{
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = nullptr;
    other.m_mappedData = nullptr;
}

VulkanBuffer &VulkanBuffer::operator=(VulkanBuffer &&other) noexcept
{
    if (this != &other) {
        if (m_buffer != VK_NULL_HANDLE && VulkanAllocator::isInitialized()) {
            vmaDestroyBuffer(VulkanAllocator::allocator(), m_buffer, m_allocation);
        }

        m_context = other.m_context;
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_size = other.m_size;
        m_usage = other.m_usage;
        m_persistentlyMapped = other.m_persistentlyMapped;
        m_mappedData = other.m_mappedData;
        m_currentOffset = other.m_currentOffset;

        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = nullptr;
        other.m_mappedData = nullptr;
    }
    return *this;
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::create(VulkanContext *context, VkDeviceSize size,
                                                   VkBufferUsageFlags usage, MemoryHint memoryHint,
                                                   UsageHint usageHint, bool persistentMap)
{
    if (!VulkanAllocator::isInitialized()) {
        qCWarning(KWIN_CORE) << "VMA allocator not initialized";
        return nullptr;
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};

    switch (memoryHint) {
    case MemoryHint::DeviceLocal:
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        break;
    case MemoryHint::HostVisible:
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        if (persistentMap) {
            allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        break;
    case MemoryHint::HostCached:
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        if (persistentMap) {
            allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        break;
    }

    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    VkResult result = vmaCreateBuffer(VulkanAllocator::allocator(), &bufferInfo, &allocInfo,
                                      &buffer, &allocation, &allocationInfo);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan buffer:" << result;
        return nullptr;
    }

    void *mappedData = nullptr;
    if (persistentMap && allocationInfo.pMappedData) {
        mappedData = allocationInfo.pMappedData;
    }

    return std::unique_ptr<VulkanBuffer>(new VulkanBuffer(context, buffer, allocation, size,
                                                          usageHint, persistentMap, mappedData));
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::createVertexBuffer(VulkanContext *context, VkDeviceSize size, MemoryHint memoryHint)
{
    return create(context, size,
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  memoryHint, UsageHint::Vertex);
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::createIndexBuffer(VulkanContext *context, VkDeviceSize size, MemoryHint memoryHint)
{
    return create(context, size,
                  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                  memoryHint, UsageHint::Index);
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::createUniformBuffer(VulkanContext *context, VkDeviceSize size)
{
    return create(context, size,
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                  MemoryHint::HostVisible, UsageHint::Uniform, true);
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::createStagingBuffer(VulkanContext *context, VkDeviceSize size)
{
    return create(context, size,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  MemoryHint::HostVisible, UsageHint::Staging);
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::createStreamingBuffer(VulkanContext *context, VkDeviceSize size)
{
    return create(context, size,
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                  MemoryHint::HostVisible, UsageHint::Vertex, true);
}

void VulkanBuffer::upload(const void *data, VkDeviceSize dataSize, VkDeviceSize offset)
{
    if (dataSize + offset > m_size) {
        qCWarning(KWIN_CORE) << "Buffer upload exceeds buffer size";
        return;
    }

    if (m_persistentlyMapped && m_mappedData) {
        // Direct copy for persistently mapped buffers
        std::memcpy(static_cast<char *>(m_mappedData) + offset, data, dataSize);
        flush(offset, dataSize);
    } else {
        // Use staging buffer for device-local memory
        auto staging = createStagingBuffer(m_context, dataSize);
        if (!staging) {
            qCWarning(KWIN_CORE) << "Failed to create staging buffer for upload";
            return;
        }

        void *mapped = staging->map();
        if (mapped) {
            std::memcpy(mapped, data, dataSize);
            staging->unmap();

            // Copy using command buffer
            VkCommandBuffer cmd = m_context->beginSingleTimeCommands();
            if (cmd != VK_NULL_HANDLE) {
                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = offset;
                copyRegion.size = dataSize;
                vkCmdCopyBuffer(cmd, staging->buffer(), m_buffer, 1, &copyRegion);
                m_context->endSingleTimeCommands(cmd);
            }
        }
    }
}

void *VulkanBuffer::map()
{
    if (m_persistentlyMapped) {
        return m_mappedData;
    }

    void *mapped = nullptr;
    VkResult result = vmaMapMemory(VulkanAllocator::allocator(), m_allocation, &mapped);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to map buffer:" << result;
        return nullptr;
    }

    return mapped;
}

void VulkanBuffer::unmap()
{
    if (!m_persistentlyMapped) {
        vmaUnmapMemory(VulkanAllocator::allocator(), m_allocation);
    }
}

void VulkanBuffer::flush(VkDeviceSize offset, VkDeviceSize size)
{
    vmaFlushAllocation(VulkanAllocator::allocator(), m_allocation, offset, size);
}

void VulkanBuffer::invalidate(VkDeviceSize offset, VkDeviceSize size)
{
    vmaInvalidateAllocation(VulkanAllocator::allocator(), m_allocation, offset, size);
}

void VulkanBuffer::reset()
{
    m_currentOffset = 0;
}

void VulkanBuffer::beginFrame()
{
    reset();
}

void VulkanBuffer::endFrame()
{
    // Flush any pending writes
    if (m_persistentlyMapped && m_currentOffset > 0) {
        flush(0, m_currentOffset);
    }
}

void *VulkanBuffer::allocate(VkDeviceSize size, VkDeviceSize alignment)
{
    if (!m_persistentlyMapped || !m_mappedData) {
        return nullptr;
    }

    // Align the current offset
    VkDeviceSize alignedOffset = (m_currentOffset + alignment - 1) & ~(alignment - 1);

    if (alignedOffset + size > m_size) {
        qCWarning(KWIN_CORE) << "Streaming buffer exhausted";
        return nullptr;
    }

    void *ptr = static_cast<char *>(m_mappedData) + alignedOffset;
    m_currentOffset = alignedOffset + size;

    return ptr;
}

VkVertexInputBindingDescription VulkanBuffer::getVertex2DBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(VulkanVertex2D);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> VulkanBuffer::getVertex2DAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(VulkanVertex2D, position);

    // Texture coordinates
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(VulkanVertex2D, texcoord);

    return attributeDescriptions;
}

} // namespace KWin
