/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/output.h"
#include "kwin_export.h"

#include <QImage>
#include <QMatrix4x4>
#include <QRegion>
#include <QSize>
#include <memory>
#include <vulkan/vulkan.h>

// Forward declare VMA types
struct VmaAllocation_T;
typedef VmaAllocation_T *VmaAllocation;

namespace KWin
{

class VulkanContext;
class VulkanBackend;

/**
 * @brief Texture coordinate type for Vulkan textures.
 */
enum class VulkanCoordinateType {
    Normalized, ///< Coordinates in [0, 1] range
    Unnormalized, ///< Coordinates in [0, width] x [0, height] range
};

/**
 * @brief Vulkan texture wrapper with VMA-managed memory.
 *
 * This class provides similar functionality to GLTexture but for Vulkan.
 * It manages VkImage, VkImageView, VkSampler, and the associated memory.
 */
class KWIN_EXPORT VulkanTexture
{
public:
    ~VulkanTexture();

    // Non-copyable
    VulkanTexture(const VulkanTexture &) = delete;
    VulkanTexture &operator=(const VulkanTexture &) = delete;

    // Movable
    VulkanTexture(VulkanTexture &&other) noexcept;
    VulkanTexture &operator=(VulkanTexture &&other) noexcept;

    /**
     * @brief Create a texture from a QImage.
     */
    static std::shared_ptr<VulkanTexture> upload(VulkanContext *context, const QImage &image);

    /**
     * @brief Create an empty texture with the specified size and format.
     */
    static std::shared_ptr<VulkanTexture> allocate(VulkanContext *context, const QSize &size, VkFormat format);

    /**
     * @brief Create a render target texture (for framebuffer attachment).
     */
    static std::shared_ptr<VulkanTexture> createRenderTarget(VulkanContext *context, const QSize &size, VkFormat format);

    /**
     * @brief Create a depth/stencil texture.
     */
    static std::shared_ptr<VulkanTexture> createDepthStencil(VulkanContext *context, const QSize &size);

    /**
     * @brief Create a non-owning wrapper around an existing VkImage (e.g., swapchain image).
     */
    static std::shared_ptr<VulkanTexture> createNonOwningWrapper(VulkanContext *context, VkImage image,
                                                                 VkFormat format, const QSize &size);

    /**
     * @brief Check if the texture is valid.
     */
    bool isValid() const
    {
        return m_image != VK_NULL_HANDLE && m_imageView != VK_NULL_HANDLE;
    }

    /**
     * @brief Get the Vulkan image handle.
     */
    VkImage image() const
    {
        return m_image;
    }

    /**
     * @brief Get the Vulkan image view handle.
     */
    VkImageView imageView() const
    {
        return m_imageView;
    }

    /**
     * @brief Get the Vulkan sampler handle.
     */
    VkSampler sampler() const
    {
        return m_sampler;
    }

    /**
     * @brief Get the texture format.
     */
    VkFormat format() const
    {
        return m_format;
    }

    /**
     * @brief Get the texture size.
     */
    QSize size() const
    {
        return m_size;
    }

    /**
     * @brief Get the texture width.
     */
    int width() const
    {
        return m_size.width();
    }

    /**
     * @brief Get the texture height.
     */
    int height() const
    {
        return m_size.height();
    }

    /**
     * @brief Check if the texture has an alpha channel.
     */
    bool hasAlphaChannel() const;

    /**
     * @brief Update a region of the texture from an image.
     */
    void update(const QImage &image, const QPoint &offset = QPoint(0, 0));

    /**
     * @brief Update a region of the texture from an image.
     */
    void update(const QImage &image, const QRegion &region);

    /**
     * @brief Get the texture coordinate transformation matrix.
     */
    QMatrix4x4 matrix(VulkanCoordinateType type) const;

    /**
     * @brief Get the texture coordinate transformation matrix (normalized coordinates).
     */
    QMatrix4x4 matrix() const
    {
        return matrix(VulkanCoordinateType::Normalized);
    }

    /**
     * @brief Set the content transform (for rotation/flip).
     */
    void setContentTransform(OutputTransform transform);

    /**
     * @brief Get the content transform.
     */
    OutputTransform contentTransform() const
    {
        return m_contentTransform;
    }

    /**
     * @brief Set the texture filter mode.
     */
    void setFilter(VkFilter filter);

    /**
     * @brief Get the texture filter mode.
     */
    VkFilter filter() const
    {
        return m_filter;
    }

    /**
     * @brief Set the texture wrap mode.
     */
    void setWrapMode(VkSamplerAddressMode mode);

    /**
     * @brief Get the texture wrap mode.
     */
    VkSamplerAddressMode wrapMode() const
    {
        return m_wrapMode;
    }

    /**
     * @brief Transition the image layout using a command buffer.
     */
    void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

    /**
     * @brief Get the current image layout.
     */
    VkImageLayout currentLayout() const
    {
        return m_currentLayout;
    }

    /**
     * @brief Set the current image layout (after external transition).
     */
    void setCurrentLayout(VkImageLayout layout)
    {
        m_currentLayout = layout;
    }

    /**
     * @brief Check if this texture owns its image (vs wrapping an external one).
     */
    bool ownsImage() const
    {
        return m_ownsImage;
    }

    /**
     * @brief Convert QImage format to Vulkan format.
     */
    static VkFormat qImageFormatToVkFormat(QImage::Format format);

    /**
     * @brief Find a suitable depth format supported by the device.
     */
    static VkFormat findDepthFormat(VulkanBackend *backend);

private:
    friend class VulkanContext; // For DMA-BUF import

    VulkanTexture(VulkanContext *context);

    bool createImage(const QSize &size, VkFormat format, VkImageUsageFlags usage, VkImageTiling tiling);
    bool createImageView(VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
    bool createSampler();
    void updateSampler();
    void cleanup();

    VulkanContext *m_context;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;

    VkFormat m_format = VK_FORMAT_UNDEFINED;
    QSize m_size;
    VkImageLayout m_currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    OutputTransform m_contentTransform;
    VkFilter m_filter = VK_FILTER_LINEAR;
    VkSamplerAddressMode m_wrapMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    bool m_ownsImage = true;

    // Cached transformation matrix
    mutable QMatrix4x4 m_cachedMatrix;
    mutable VulkanCoordinateType m_cachedMatrixType = VulkanCoordinateType::Normalized;
    mutable bool m_matrixDirty = true;
};

} // namespace KWin
