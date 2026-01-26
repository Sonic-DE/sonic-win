/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "scene/itemrenderer.h"

namespace KWin
{

class VulkanBackend;

class KWIN_EXPORT ItemRendererVulkan : public ItemRenderer
{
public:
    ItemRendererVulkan();

    std::unique_ptr<ImageItem> createImageItem(Item *parent = nullptr) override;

    void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport) override;
    void endFrame() override;

    void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region) override;
    void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data) override;

private:
    VulkanBackend *m_backend = nullptr;
};

} // namespace KWin
