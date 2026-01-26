/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "platformsupport/scenes/vulkan/vulkanbackend.h"
#include "scene/decorationitem.h"
#include "scene/shadowitem.h"
#include "scene/workspacescene.h"

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

class VulkanShadowTextureProvider : public ShadowTextureProvider
{
public:
    explicit VulkanShadowTextureProvider(Shadow *shadow);

    void update() override;
};

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

    void render(const QRegion &region) override;

private:
    void resizeImages();
    // TODO: Add Vulkan-specific decoration image storage
};

} // namespace KWin
