/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "scene/surfaceitem.h"
#include "vulkantexture.h"
#include <vulkan/vulkan.h>

#include <QList>
#include <QVarLengthArray>
#include <memory>

namespace KWin
{

class VulkanBackend;

/**
 * @brief Container for multi-plane Vulkan surface textures.
 *
 * Similar to OpenGLSurfaceContents, this class supports multi-planar textures
 * for formats like YUV/NV12 used in video playback.
 */
class KWIN_EXPORT VulkanSurfaceContents
{
public:
    VulkanSurfaceContents()
    {
    }

    VulkanSurfaceContents(const std::shared_ptr<VulkanTexture> &contents)
        : planes({contents})
    {
    }

    VulkanSurfaceContents(const QList<std::shared_ptr<VulkanTexture>> &planes)
        : planes(planes)
    {
    }

    void reset()
    {
        planes.clear();
    }

    bool isValid() const
    {
        return !planes.isEmpty();
    }

    /**
     * @brief Convert to var length array for compatibility with render code.
     */
    QVarLengthArray<VulkanTexture *, 4> toVarLengthArray() const
    {
        Q_ASSERT(planes.size() <= 4);
        QVarLengthArray<VulkanTexture *, 4> ret;
        for (const auto &plane : planes) {
            ret.append(plane.get());
        }
        return ret;
    }

    /**
     * @brief Get first plane texture, or nullptr if no planes.
     */
    VulkanTexture *firstPlane() const
    {
        return planes.isEmpty() ? nullptr : planes.constFirst().get();
    }

    QList<std::shared_ptr<VulkanTexture>> planes;
};

/**
 * @brief Texture wrapper for Vulkan surface textures
 */
class KWIN_EXPORT VulkanSurfaceTexture : public SurfaceTexture
{
public:
    explicit VulkanSurfaceTexture(VulkanBackend *backend);
    ~VulkanSurfaceTexture() override;

    bool isValid() const override;

    VulkanBackend *backend() const;
    VulkanSurfaceContents texture() const
    {
        return m_texture;
    }

protected:
    VulkanBackend *m_backend;
    VulkanSurfaceContents m_texture;
};

} // namespace KWin
