/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/outputlayer.h"
#include "platformsupport/scenes/vulkan/vulkanbackend.h"
#include "platformsupport/scenes/vulkan/vulkancontext.h"
#include "platformsupport/scenes/vulkan/vulkansurfacetexture_x11.h"
#include "platformsupport/scenes/vulkan/vulkanswapchain.h"
#include "x11eventfilter.h"

#include <vulkan/vulkan.h>
#include <xcb/xcb.h>

#include <memory>

namespace KWin
{

class X11StandaloneBackend;
class X11StandaloneVulkanSurfaceTexture;
class Swapchain;
class SurfacePixmapX11;
class Output;

class VulkanLayer : public OutputLayer
{
public:
    VulkanLayer(KWin::VulkanBackend *backend);

protected:
    std::optional<OutputLayerBeginFrameInfo> doBeginFrame() override;
    bool doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame) override;
    DrmDevice *scanoutDevice() const override;
    QHash<uint32_t, QList<uint64_t>> supportedDrmFormats() const override;

private:
    KWin::VulkanBackend *const m_backend;
};

/**
 * @brief Vulkan Backend using Vulkan over an X overlay window.
 */
class X11StandaloneVulkanBackend : public KWin::VulkanBackend
{
    Q_OBJECT

public:
    X11StandaloneVulkanBackend(X11StandaloneBackend *backend);
    ~X11StandaloneVulkanBackend() override;

    std::unique_ptr<SurfaceTexture> createSurfaceTextureX11(SurfacePixmapX11 *pixmap) override;
    bool present(Output *output, const std::shared_ptr<OutputFrame> &frame) override;
    bool makeCurrent() override;
    void doneCurrent() override;
    KWin::VulkanContext *vulkanContext() const override;
    OverlayWindow *overlayWindow() const override;
    void init() override;
    OutputLayer *primaryLayer(Output *output) override;

    xcb_connection_t *connection() const;
    xcb_window_t window() const
    {
        return m_window;
    }

    /**
     * @brief Get the swapchain
     */
    KWin::VulkanSwapchain *swapchain() const
    {
        return m_swapchain.get();
    }

private:
    bool initInstance();
    bool initPhysicalDevice();
    bool initDevice();
    bool initSurface();
    bool initSwapchain();
    bool initOverlayWindow();

    void screenGeometryChanged();

    std::unique_ptr<OverlayWindow> m_overlayWindow;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    xcb_colormap_t m_colormap = XCB_COLORMAP_NONE;

    std::unique_ptr<KWin::VulkanContext> m_context;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<KWin::VulkanSwapchain> m_swapchain;

    X11StandaloneBackend *m_backend;
    std::unique_ptr<VulkanLayer> m_layer;
    std::shared_ptr<OutputFrame> m_frame;

    friend class X11StandaloneVulkanSurfaceTexture;
};

class X11StandaloneVulkanSurfaceTexture final : public KWin::VulkanSurfaceTextureX11
{
public:
    X11StandaloneVulkanSurfaceTexture(X11StandaloneVulkanBackend *backend, SurfacePixmapX11 *pixmap);

    bool create() override;
    void update(const QRegion &region) override;

private:
    X11StandaloneVulkanBackend *m_backend;
};

} // namespace KWin
