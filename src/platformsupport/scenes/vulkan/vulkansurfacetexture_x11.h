/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "vulkansurfacetexture.h"
#include "vulkantexture.h"

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
 */
class KWIN_EXPORT VulkanSurfaceTextureX11 : public VulkanSurfaceTexture
{
public:
    VulkanSurfaceTextureX11(VulkanBackend *backend, SurfacePixmapX11 *pixmap);
    ~VulkanSurfaceTextureX11() override;

    bool create() override;
    void update(const QRegion &region) override;

    /**
     * @brief Get the Vulkan texture for rendering.
     */
    VulkanTexture *texture() const
    {
        return m_texture.get();
    }

private:
    bool createWithDmaBuf();
    bool createWithCpuUpload();
    void updateWithCpuUpload(const QRegion &region);

    SurfacePixmapX11 *m_pixmap;
    VulkanContext *m_context;
    std::unique_ptr<VulkanTexture> m_texture;
    std::unique_ptr<VulkanBuffer> m_stagingBuffer;
    QSize m_size;
    bool m_useDmaBuf = false;
};

} // namespace KWin
