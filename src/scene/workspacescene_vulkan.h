/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "config-kwin.h"

#if HAVE_VULKAN

#include "platformsupport/scenes/vulkan/vulkanbackend.h"
#include "platformsupport/scenes/vulkan/vulkantexture.h"
#include "scene/decorationitem.h"
#include "scene/shadowitem.h"
#include "scene/workspacescene.h"

#include <memory>

namespace KWin
{

class KWIN_EXPORT WorkspaceSceneVulkan : public WorkspaceScene
{
    Q_OBJECT

public:
    explicit WorkspaceSceneVulkan(VulkanBackend *backend);
    ~WorkspaceSceneVulkan() override;

    std::unique_ptr<DecorationRenderer> createDecorationRenderer(Decoration::DecoratedWindowImpl *impl) override;
    std::unique_ptr<ShadowTextureProvider> createShadowTextureProvider(Shadow *shadow) override;

    bool animationsSupported() const override
    {
        return true; // Vulkan supports animations
    }

    VulkanBackend *backend() const
    {
        return m_backend;
    }

private:
    VulkanBackend *m_backend;
};

/**
 * @brief Vulkan implementation of shadow texture provider
 */
class VulkanShadowTextureProvider : public ShadowTextureProvider
{
public:
    explicit VulkanShadowTextureProvider(Shadow *shadow);
    ~VulkanShadowTextureProvider() override;

    void update() override;

    std::shared_ptr<VulkanTexture> texture() const
    {
        return m_texture;
    }

private:
    std::shared_ptr<VulkanTexture> m_texture;
};

/**
 * @brief Vulkan implementation of decoration renderer
 */
class SceneVulkanDecorationRenderer : public DecorationRenderer
{
    Q_OBJECT
public:
    enum class DecorationPart : int {
        Left,
        Top,
        Right,
        Bottom,
        Count
    };

    explicit SceneVulkanDecorationRenderer(Decoration::DecoratedWindowImpl *client);
    ~SceneVulkanDecorationRenderer() override;

    void render(const QRegion &region) override;

    std::shared_ptr<VulkanTexture> texture() const
    {
        return m_texture;
    }

private:
    void renderPart(const QRectF &rect, const QRectF &partRect, const QPoint &textureOffset, qreal devicePixelRatio, bool rotated = false);
    static const QMargins texturePadForPart(const QRectF &rect, const QRectF &partRect);
    void resizeTexture();
    int toNativeSize(double size) const;

    std::shared_ptr<VulkanTexture> m_texture;
    QImage m_scratchImage;
};

} // namespace KWin

#endif // HAVE_VULKAN
