/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "vulkansurfacetexture.h"

namespace KWin
{

class SurfacePixmapX11;

/**
 * @brief X11-specific Vulkan surface texture implementation
 */
class KWIN_EXPORT VulkanSurfaceTextureX11 : public VulkanSurfaceTexture
{
public:
    VulkanSurfaceTextureX11(VulkanBackend *backend, SurfacePixmapX11 *pixmap);
    ~VulkanSurfaceTextureX11() override;

    bool create() override;
    void update(const QRegion &region) override;

private:
    SurfacePixmapX11 *m_pixmap;
};

} // namespace KWin
