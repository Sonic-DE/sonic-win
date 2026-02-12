/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "vulkansurfacetexture.h"

#include <QSize>
#include <memory>

namespace KWin
{

class SurfacePixmapX11;
class VulkanContext;
class VulkanBuffer;

/**
 * @brief X11-specific Vulkan surface texture implementation
 *
 * This class imports X11 pixmaps as Vulkan textures. It supports two methods:
 * 1. DMA-BUF import (efficient, requires DRI3 support)
 * 2. CPU fallback via XGetImage (slower, always available)
 *
 * Multi-plane textures are supported for YUV formats (e.g., NV12) used in video playback.
 * Each plane is stored as a separate VulkanTexture in the m_texture.planes list.
 */
class KWIN_EXPORT VulkanSurfaceTextureX11 : public VulkanSurfaceTexture
{
public:
    VulkanSurfaceTextureX11(VulkanBackend *backend, SurfacePixmapX11 *pixmap);
    ~VulkanSurfaceTextureX11() override;

    bool create() override;
    void update(const QRegion &region) override;
    bool isValid() const override;

    /**
     * @brief Get the Vulkan image handle from the first plane.
     */
    VkImage image() const
    {
        return m_texture.planes.isEmpty() ? VK_NULL_HANDLE : m_texture.planes.first()->image();
    }

    /**
     * @brief Get the Vulkan image view handle from the first plane.
     */
    VkImageView imageView() const
    {
        return m_texture.planes.isEmpty() ? VK_NULL_HANDLE : m_texture.planes.first()->imageView();
    }

    /**
     * @brief Get the number of texture planes.
     * Returns 1 for standard RGBA textures, 2 for NV12, 3 for YUV420, etc.
     */
    int planeCount() const
    {
        return m_texture.planes.size();
    }

    /**
     * @brief Get the Vulkan image handle for a specific plane.
     * @param planeIndex The plane index (0 for Y plane, 1 for UV plane in NV12)
     */
    VkImage image(int planeIndex) const
    {
        if (planeIndex < 0 || planeIndex >= m_texture.planes.size()) {
            return VK_NULL_HANDLE;
        }
        return m_texture.planes[planeIndex]->image();
    }

    /**
     * @brief Get the Vulkan image view handle for a specific plane.
     * @param planeIndex The plane index
     */
    VkImageView imageView(int planeIndex) const
    {
        if (planeIndex < 0 || planeIndex >= m_texture.planes.size()) {
            return VK_NULL_HANDLE;
        }
        return m_texture.planes[planeIndex]->imageView();
    }

    /**
     * @brief Get the Vulkan sampler handle for a specific plane.
     * @param planeIndex The plane index
     */
    VkSampler sampler(int planeIndex) const
    {
        if (planeIndex < 0 || planeIndex >= m_texture.planes.size()) {
            return VK_NULL_HANDLE;
        }
        return m_texture.planes[planeIndex]->sampler();
    }

    /**
     * @brief Check if this is a multi-plane (YUV) texture.
     */
    bool isMultiPlane() const
    {
        return m_texture.planes.size() > 1;
    }

private:
    bool createWithDmaBuf();
    bool createWithCpuUpload();
    void updateWithCpuUpload(const QRegion &region);

    /**
     * @brief Import multi-plane DMA-BUF (e.g., NV12).
     * @param attributes The DMA-BUF attributes
     * @return true if all planes were imported successfully
     */
    bool createMultiPlaneWithDmaBuf(const DmaBufAttributes &attributes);

    SurfacePixmapX11 *m_pixmap;
    VulkanContext *m_context;
    std::unique_ptr<VulkanBuffer> m_stagingBuffer;
    QSize m_size;
    bool m_useDmaBuf = false;
};

} // namespace KWin
