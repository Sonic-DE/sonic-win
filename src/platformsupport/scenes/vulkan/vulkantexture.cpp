/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkantexture.h"
#include "utils/common.h"
#include "vma_usage.h"
#include "vulkanbackend.h"
#include "vulkanbuffer.h"
#include "vulkancontext.h"

#include <QDebug>
#include <cstring>

namespace KWin
{

VulkanTexture::VulkanTexture(VulkanContext *context)
    : m_context(context)
{
}

VulkanTexture::~VulkanTexture()
{
    cleanup();
}

VulkanTexture::VulkanTexture(VulkanTexture &&other) noexcept
    : m_context(other.m_context)
    , m_image(other.m_image)
    , m_imageView(other.m_imageView)
    , m_sampler(other.m_sampler)
    , m_allocation(other.m_allocation)
    , m_deviceMemory(other.m_deviceMemory)
    , m_format(other.m_format)
    , m_size(other.m_size)
    , m_currentLayout(other.m_currentLayout)
    , m_contentTransform(other.m_contentTransform)
    , m_filter(other.m_filter)
    , m_wrapMode(other.m_wrapMode)
    , m_ownsImage(other.m_ownsImage)
{
    other.m_image = VK_NULL_HANDLE;
    other.m_imageView = VK_NULL_HANDLE;
    other.m_sampler = VK_NULL_HANDLE;
    other.m_allocation = nullptr;
    other.m_deviceMemory = VK_NULL_HANDLE;
}

VulkanTexture &VulkanTexture::operator=(VulkanTexture &&other) noexcept
{
    if (this != &other) {
        cleanup();

        m_context = other.m_context;
        m_image = other.m_image;
        m_imageView = other.m_imageView;
        m_sampler = other.m_sampler;
        m_allocation = other.m_allocation;
        m_deviceMemory = other.m_deviceMemory;
        m_format = other.m_format;
        m_size = other.m_size;
        m_currentLayout = other.m_currentLayout;
        m_contentTransform = other.m_contentTransform;
        m_filter = other.m_filter;
        m_wrapMode = other.m_wrapMode;
        m_ownsImage = other.m_ownsImage;

        other.m_image = VK_NULL_HANDLE;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_sampler = VK_NULL_HANDLE;
        other.m_allocation = nullptr;
        other.m_deviceMemory = VK_NULL_HANDLE;
    }
    return *this;
}

void VulkanTexture::cleanup()
{
    VkDevice device = m_context->backend()->device();
    if (device == VK_NULL_HANDLE) {
        return;
    }

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }

    if (m_ownsImage && m_image != VK_NULL_HANDLE) {
        // Handle VMA-managed memory
        if (m_allocation != nullptr && VulkanAllocator::isInitialized()) {
            vmaDestroyImage(VulkanAllocator::allocator(), m_image, m_allocation);
            m_allocation = nullptr;
        } else if (m_deviceMemory != VK_NULL_HANDLE) {
            // Handle raw Vulkan memory (e.g., DMA-BUF imports)
            vkDestroyImage(device, m_image, nullptr);
            vkFreeMemory(device, m_deviceMemory, nullptr);
            m_deviceMemory = VK_NULL_HANDLE;
        } else {
            // Image without memory allocation (shouldn't happen for owned images)
            vkDestroyImage(device, m_image, nullptr);
        }
        m_image = VK_NULL_HANDLE;
    }
}

bool VulkanTexture::createImage(const QSize &size, VkFormat format, VkImageUsageFlags usage, VkImageTiling tiling)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(size.width());
    imageInfo.extent.height = static_cast<uint32_t>(size.height());
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkResult result = vmaCreateImage(VulkanAllocator::allocator(), &imageInfo, &allocInfo,
                                     &m_image, &m_allocation, nullptr);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan image:" << result;
        return false;
    }

    m_format = format;
    m_size = size;
    m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    m_ownsImage = true;

    return true;
}

bool VulkanTexture::createImageView(VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    // Component swizzle for BGRA formats
    if (m_format == VK_FORMAT_B8G8R8A8_UNORM || m_format == VK_FORMAT_B8G8R8A8_SRGB) {
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    }

    VkResult result = vkCreateImageView(m_context->backend()->device(), &viewInfo, nullptr, &m_imageView);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan image view:" << result;
        return false;
    }

    return true;
}

bool VulkanTexture::createSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = m_filter;
    samplerInfo.minFilter = m_filter;
    samplerInfo.addressModeU = m_wrapMode;
    samplerInfo.addressModeV = m_wrapMode;
    samplerInfo.addressModeW = m_wrapMode;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    VkResult result = vkCreateSampler(m_context->backend()->device(), &samplerInfo, nullptr, &m_sampler);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan sampler:" << result;
        return false;
    }

    return true;
}

void VulkanTexture::updateSampler()
{
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_context->backend()->device(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    createSampler();
}

std::unique_ptr<VulkanTexture> VulkanTexture::upload(VulkanContext *context, const QImage &image)
{
    if (image.isNull()) {
        return nullptr;
    }

    VkFormat format = qImageFormatToVkFormat(image.format());
    if (format == VK_FORMAT_UNDEFINED) {
        // Convert to a supported format
        QImage converted = image.convertToFormat(QImage::Format_RGBA8888);
        format = VK_FORMAT_R8G8B8A8_UNORM;
        return upload(context, converted);
    }

    auto texture = std::unique_ptr<VulkanTexture>(new VulkanTexture(context));

    if (!texture->createImage(image.size(), format,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_IMAGE_TILING_OPTIMAL)) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan texture image for upload";
        return nullptr;
    }

    if (!texture->createImageView()) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan texture image view for upload";
        return nullptr;
    }

    if (!texture->createSampler()) {
        qCWarning(KWIN_CORE) << "Failed to create Vulkan texture sampler for upload";
        return nullptr;
    }

    // Upload image data via staging buffer
    VkDeviceSize imageSize = image.sizeInBytes();
    auto staging = VulkanBuffer::createStagingBuffer(context, imageSize);
    if (!staging) {
        return nullptr;
    }

    void *mapped = staging->map();
    if (!mapped) {
        return nullptr;
    }

    std::memcpy(mapped, image.constBits(), imageSize);
    staging->unmap();

    // Transition and copy
    VkCommandBuffer cmd = context->beginSingleTimeCommands();
    if (cmd == VK_NULL_HANDLE) {
        return nullptr;
    }

    // Transition to transfer destination
    texture->transitionLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(image.width()), static_cast<uint32_t>(image.height()), 1};

    vkCmdCopyBufferToImage(cmd, staging->buffer(), texture->m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to shader read
    texture->transitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    context->endSingleTimeCommands(cmd);

    return texture;
}

std::unique_ptr<VulkanTexture> VulkanTexture::allocate(VulkanContext *context, const QSize &size, VkFormat format)
{
    auto texture = std::unique_ptr<VulkanTexture>(new VulkanTexture(context));

    if (!texture->createImage(size, format,
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              VK_IMAGE_TILING_OPTIMAL)) {
        return nullptr;
    }

    if (!texture->createImageView()) {
        return nullptr;
    }

    if (!texture->createSampler()) {
        return nullptr;
    }

    return texture;
}

std::unique_ptr<VulkanTexture> VulkanTexture::createRenderTarget(VulkanContext *context, const QSize &size, VkFormat format)
{
    auto texture = std::unique_ptr<VulkanTexture>(new VulkanTexture(context));

    if (!texture->createImage(size, format,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              VK_IMAGE_TILING_OPTIMAL)) {
        return nullptr;
    }

    if (!texture->createImageView()) {
        return nullptr;
    }

    if (!texture->createSampler()) {
        return nullptr;
    }

    return texture;
}

std::unique_ptr<VulkanTexture> VulkanTexture::createDepthStencil(VulkanContext *context, const QSize &size)
{
    VkFormat depthFormat = findDepthFormat(context->backend());
    if (depthFormat == VK_FORMAT_UNDEFINED) {
        qCWarning(KWIN_CORE) << "No suitable depth format found";
        return nullptr;
    }

    auto texture = std::unique_ptr<VulkanTexture>(new VulkanTexture(context));

    if (!texture->createImage(size, depthFormat,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              VK_IMAGE_TILING_OPTIMAL)) {
        return nullptr;
    }

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (depthFormat == VK_FORMAT_D24_UNORM_S8_UINT || depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (!texture->createImageView(aspectFlags)) {
        return nullptr;
    }

    return texture;
}

std::unique_ptr<VulkanTexture> VulkanTexture::createNonOwningWrapper(VulkanContext *context, VkImage image,
                                                                     VkFormat format, const QSize &size)
{
    auto texture = std::unique_ptr<VulkanTexture>(new VulkanTexture(context));

    texture->m_image = image;
    texture->m_format = format;
    texture->m_size = size;
    texture->m_ownsImage = false;

    if (!texture->createImageView()) {
        return nullptr;
    }

    if (!texture->createSampler()) {
        return nullptr;
    }

    return texture;
}

bool VulkanTexture::hasAlphaChannel() const
{
    switch (m_format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return true;
    default:
        return false;
    }
}

void VulkanTexture::update(const QImage &image, const QPoint &offset)
{
    QRegion region(QRect(offset, image.size()));
    update(image, region);
}

void VulkanTexture::update(const QImage &image, const QRegion &region)
{
    if (image.isNull() || region.isEmpty()) {
        return;
    }

    // For simplicity, use the bounding rect
    QRect bounds = region.boundingRect();

    VkDeviceSize imageSize = image.sizeInBytes();
    auto staging = VulkanBuffer::createStagingBuffer(m_context, imageSize);
    if (!staging) {
        qCWarning(KWIN_CORE) << "Failed to create staging buffer for texture update";
        return;
    }

    void *mapped = staging->map();
    if (!mapped) {
        qCWarning(KWIN_CORE) << "Failed to map staging buffer for texture update";
        return;
    }

    std::memcpy(mapped, image.constBits(), imageSize);
    staging->unmap();

    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();
    if (cmd == VK_NULL_HANDLE) {
        qCWarning(KWIN_CORE) << "Failed to begin single time commands for texture update";
        return;
    }

    // Transition to transfer destination
    VkImageLayout oldLayout = m_currentLayout;
    transitionLayout(cmd, oldLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy buffer to image
    VkBufferImageCopy region_copy{};
    region_copy.bufferOffset = 0;
    region_copy.bufferRowLength = 0;
    region_copy.bufferImageHeight = 0;
    region_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region_copy.imageSubresource.mipLevel = 0;
    region_copy.imageSubresource.baseArrayLayer = 0;
    region_copy.imageSubresource.layerCount = 1;
    region_copy.imageOffset = {bounds.x(), bounds.y(), 0};
    region_copy.imageExtent = {static_cast<uint32_t>(bounds.width()),
                               static_cast<uint32_t>(bounds.height()), 1};

    vkCmdCopyBufferToImage(cmd, staging->buffer(), m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region_copy);

    // Transition back to shader read
    transitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    m_context->endSingleTimeCommands(cmd);
}

QMatrix4x4 VulkanTexture::matrix(VulkanCoordinateType type) const
{
    if (!m_matrixDirty && m_cachedMatrixType == type) {
        return m_cachedMatrix;
    }

    QMatrix4x4 matrix;

    if (type == VulkanCoordinateType::Unnormalized) {
        matrix.scale(m_size.width(), m_size.height());
    }

    // Apply content transform
    // Vulkan has Y-axis pointing down, so we may need to flip
    switch (m_contentTransform.kind()) {
    case OutputTransform::Normal:
        break;
    case OutputTransform::FlipY:
        matrix.translate(0, 1);
        matrix.scale(1, -1);
        break;
    case OutputTransform::Rotate90:
        matrix.rotate(90, 0, 0, 1);
        break;
    case OutputTransform::Rotate180:
        matrix.rotate(180, 0, 0, 1);
        break;
    case OutputTransform::Rotate270:
        matrix.rotate(270, 0, 0, 1);
        break;
    default:
        break;
    }

    m_cachedMatrix = matrix;
    m_cachedMatrixType = type;
    m_matrixDirty = false;

    return matrix;
}

void VulkanTexture::setContentTransform(OutputTransform transform)
{
    if (m_contentTransform != transform) {
        m_contentTransform = transform;
        m_matrixDirty = true;
    }
}

void VulkanTexture::setFilter(VkFilter filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        updateSampler();
    }
}

void VulkanTexture::setWrapMode(VkSamplerAddressMode mode)
{
    if (m_wrapMode != mode) {
        m_wrapMode = mode;
        updateSampler();
    }
}

void VulkanTexture::transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout,
                                     VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Determine access masks based on layouts
    switch (oldLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
        barrier.srcAccessMask = 0;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    default:
        barrier.srcAccessMask = 0;
        break;
    }

    switch (newLayout) {
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        barrier.dstAccessMask = 0;
        break;
    default:
        barrier.dstAccessMask = 0;
        break;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_currentLayout = newLayout;
}

VkFormat VulkanTexture::qImageFormatToVkFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QImage::Format_RGBX8888:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QImage::Format_RGB888:
        return VK_FORMAT_R8G8B8_UNORM;
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case QImage::Format_RGB32:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case QImage::Format_Grayscale8:
        return VK_FORMAT_R8_UNORM;
    case QImage::Format_Grayscale16:
        return VK_FORMAT_R16_UNORM;
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case QImage::Format_RGBX64:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case QImage::Format_RGBA32FPx4:
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

VkFormat VulkanTexture::findDepthFormat(VulkanBackend *backend)
{
    const std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(backend->physicalDevice(), format, &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

} // namespace KWin
