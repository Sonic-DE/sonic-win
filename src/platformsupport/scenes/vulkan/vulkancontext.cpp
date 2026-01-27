/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkancontext.h"
#include "core/graphicsbuffer.h"
#include "utils/common.h"
#include "vulkanbackend.h"
#include "vulkanbuffer.h"
#include "vulkanframebuffer.h"
#include "vulkanpipelinemanager.h"
#include "vulkantexture.h"

#include <QDebug>

namespace KWin
{

thread_local VulkanContext *VulkanContext::s_currentContext = nullptr;

VulkanContext::VulkanContext(VulkanBackend *backend)
    : m_backend(backend)
{
    // Initialize VMA allocator
    if (!VulkanAllocator::initialize(backend)) {
        qCWarning(KWIN_CORE) << "Failed to initialize VMA allocator";
        return;
    }

    if (!createCommandPool()) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan command pool";
        return;
    }

    if (!createDescriptorPool()) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan descriptor pool";
        cleanup();
        return;
    }

    // Create pipeline manager
    m_pipelineManager = std::make_unique<VulkanPipelineManager>(this);

    // Create streaming vertex buffer (4MB initial size, can grow)
    m_streamingBuffer = VulkanBuffer::createStreamingBuffer(this, 4 * 1024 * 1024);

    // Check for DMA-BUF import support
    // This requires VK_EXT_external_memory_dma_buf extension
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    vkGetPhysicalDeviceProperties2(m_backend->physicalDevice(), &props2);

    // Check if external memory extensions are available
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(m_backend->physicalDevice(), nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(m_backend->physicalDevice(), nullptr, &extensionCount, extensions.data());

    for (const auto &ext : extensions) {
        if (strcmp(ext.extensionName, VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) == 0) {
            m_supportsDmaBufImport = true;
            break;
        }
    }

    qCDebug(KWIN_CORE) << "VulkanContext created, DMA-BUF import:" << m_supportsDmaBufImport;
}

VulkanContext::~VulkanContext()
{
    if (s_currentContext == this) {
        doneCurrent();
    }
    cleanup();
}

bool VulkanContext::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_backend->graphicsQueueFamily();

    VkResult result = vkCreateCommandPool(m_backend->device(), &poolInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create command pool:" << result;
        return false;
    }

    return true;
}

bool VulkanContext::createDescriptorPool()
{
    // Create a descriptor pool with enough descriptors for typical usage
    // These numbers can be tuned based on actual usage patterns
    std::array<VkDescriptorPoolSize, 3> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
    }};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1000;

    VkResult result = vkCreateDescriptorPool(m_backend->device(), &poolInfo, nullptr, &m_descriptorPool);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create descriptor pool:" << result;
        return false;
    }

    return true;
}

void VulkanContext::cleanup()
{
    VkDevice device = m_backend->device();
    if (device == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(device);

    m_streamingBuffer.reset();
    m_pipelineManager.reset();

    if (m_fence != VK_NULL_HANDLE) {
        vkDestroyFence(device, m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
}

bool VulkanContext::makeCurrent()
{
    s_currentContext = this;
    return true;
}

void VulkanContext::doneCurrent()
{
    if (s_currentContext == this) {
        s_currentContext = nullptr;
    }
}

bool VulkanContext::isValid() const
{
    return m_commandPool != VK_NULL_HANDLE && m_descriptorPool != VK_NULL_HANDLE;
}

VulkanBackend *VulkanContext::backend() const
{
    return m_backend;
}

VkCommandPool VulkanContext::commandPool() const
{
    return m_commandPool;
}

VkDescriptorPool VulkanContext::descriptorPool() const
{
    return m_descriptorPool;
}

VulkanPipelineManager *VulkanContext::pipelineManager() const
{
    return m_pipelineManager.get();
}

VulkanBuffer *VulkanContext::streamingBuffer() const
{
    return m_streamingBuffer.get();
}

VkCommandBuffer VulkanContext::allocateCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VkResult result = vkAllocateCommandBuffers(m_backend->device(), &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to allocate command buffer:" << result;
        return VK_NULL_HANDLE;
    }

    return commandBuffer;
}

void VulkanContext::freeCommandBuffer(VkCommandBuffer commandBuffer)
{
    if (commandBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(m_backend->device(), m_commandPool, 1, &commandBuffer);
    }
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands()
{
    VkCommandBuffer commandBuffer = allocateCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(m_backend->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_backend->graphicsQueue());

    freeCommandBuffer(commandBuffer);
}

VkDescriptorSet VulkanContext::allocateDescriptorSet(VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(m_backend->device(), &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to allocate descriptor set:" << result;
        return VK_NULL_HANDLE;
    }

    return descriptorSet;
}

std::shared_ptr<VulkanTexture> VulkanContext::importDmaBufAsTexture(const DmaBufAttributes &attributes)
{
    if (!m_supportsDmaBufImport) {
        qCWarning(KWIN_CORE) << "DMA-BUF import not supported";
        return nullptr;
    }

    // Create VkImage with external memory
    VkExternalMemoryImageCreateInfo externalMemoryInfo{};
    externalMemoryInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &externalMemoryInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = static_cast<VkFormat>(attributes.format);
    imageInfo.extent.width = attributes.width;
    imageInfo.extent.height = attributes.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    VkResult result = vkCreateImage(m_backend->device(), &imageInfo, nullptr, &image);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create image for DMA-BUF import:" << result;
        return nullptr;
    }

    // Get memory requirements
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_backend->device(), image, &memReqs);

    // Find appropriate memory type for DMA-BUF import
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_backend->physicalDevice(), &memProperties);

    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memReqs.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memoryTypeIndex = i;
            break;
        }
    }

    // Fallback to any compatible memory type if device-local not found
    if (memoryTypeIndex == UINT32_MAX) {
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if (memReqs.memoryTypeBits & (1 << i)) {
                memoryTypeIndex = i;
                break;
            }
        }
    }

    if (memoryTypeIndex == UINT32_MAX) {
        qCWarning(KWIN_CORE) << "Failed to find suitable memory type for DMA-BUF import";
        vkDestroyImage(m_backend->device(), image, nullptr);
        return nullptr;
    }

    // Import memory with VkImportMemoryFdInfoKHR
    VkImportMemoryFdInfoKHR importFdInfo{};
    importFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    importFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    importFdInfo.fd = attributes.fd[0].get(); // Use the first plane's fd

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &importFdInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory memory;
    result = vkAllocateMemory(m_backend->device(), &allocInfo, nullptr, &memory);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to allocate memory for DMA-BUF import:" << result;
        vkDestroyImage(m_backend->device(), image, nullptr);
        return nullptr;
    }

    // Bind memory to image
    result = vkBindImageMemory(m_backend->device(), image, memory, 0);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to bind memory to image:" << result;
        vkFreeMemory(m_backend->device(), memory, nullptr);
        vkDestroyImage(m_backend->device(), image, nullptr);
        return nullptr;
    }

    // Create image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = imageInfo.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    result = vkCreateImageView(m_backend->device(), &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create image view:" << result;
        vkFreeMemory(m_backend->device(), memory, nullptr);
        vkDestroyImage(m_backend->device(), image, nullptr);
        return nullptr;
    }

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler;
    result = vkCreateSampler(m_backend->device(), &samplerInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create sampler:" << result;
        vkDestroyImageView(m_backend->device(), imageView, nullptr);
        vkFreeMemory(m_backend->device(), memory, nullptr);
        vkDestroyImage(m_backend->device(), image, nullptr);
        return nullptr;
    }

    // Create VulkanTexture wrapper
    auto texture = std::shared_ptr<VulkanTexture>(new VulkanTexture(this));
    texture->m_image = image;
    texture->m_imageView = imageView;
    texture->m_sampler = sampler;
    texture->m_format = imageInfo.format;
    texture->m_size = QSize(attributes.width, attributes.height);
    texture->m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture->m_ownsImage = true;

    return texture;
}

bool VulkanContext::supportsDmaBufImport() const
{
    return m_supportsDmaBufImport;
}

void VulkanContext::pushFramebuffer(VulkanFramebuffer *fbo)
{
    m_framebufferStack.push(fbo);
}

VulkanFramebuffer *VulkanContext::popFramebuffer()
{
    if (m_framebufferStack.isEmpty()) {
        return nullptr;
    }
    return m_framebufferStack.pop();
}

VulkanFramebuffer *VulkanContext::currentFramebuffer() const
{
    if (m_framebufferStack.isEmpty()) {
        return nullptr;
    }
    return m_framebufferStack.top();
}

VulkanContext *VulkanContext::currentContext()
{
    return s_currentContext;
}

VkFence VulkanContext::getOrCreateFence()
{
    if (m_fence == VK_NULL_HANDLE) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = 0;

        VkResult result = vkCreateFence(m_backend->device(), &fenceInfo, nullptr, &m_fence);
        if (result != VK_SUCCESS) {
            qCWarning(KWIN_CORE) << "Failed to create fence:" << result;
            return VK_NULL_HANDLE;
        }
    }
    return m_fence;
}

VkFence VulkanContext::createExportableFence()
{
    if (!m_backend->supportsExternalFenceFd()) {
        return VK_NULL_HANDLE;
    }

    // Set up export info for sync fd
    VkExportFenceCreateInfo exportInfo{};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = &exportInfo;
    fenceInfo.flags = 0;

    VkFence fence = VK_NULL_HANDLE;
    VkResult result = vkCreateFence(m_backend->device(), &fenceInfo, nullptr, &fence);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create exportable fence:" << result;
        return VK_NULL_HANDLE;
    }

    return fence;
}

FileDescriptor VulkanContext::exportFenceToSyncFd(VkFence fence)
{
    if (!m_backend->supportsExternalFenceFd() || fence == VK_NULL_HANDLE) {
        return FileDescriptor();
    }

    VkFenceGetFdInfoKHR getFdInfo{};
    getFdInfo.sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR;
    getFdInfo.fence = fence;
    getFdInfo.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;

    int fd = -1;
    VkResult result = m_backend->vkGetFenceFdKHR()(m_backend->device(), &getFdInfo, &fd);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to export fence to sync fd:" << result;
        return FileDescriptor();
    }

    return FileDescriptor(fd);
}

bool VulkanContext::supportsExternalFenceFd() const
{
    return m_backend->supportsExternalFenceFd();
}

} // namespace KWin
