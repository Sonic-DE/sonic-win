/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "x11_standalone_vulkan_backend.h"
#include "compositor.h"
#include "core/outputbackend.h"
#include "core/overlaywindow.h"
#include "platformsupport/scenes/vulkan/vulkanpipelinemanager.h"
#include "platformsupport/scenes/vulkan/vulkanrenderpass.h"
#include "platformsupport/scenes/vulkan/vulkanrendertarget.h"
#include "platformsupport/scenes/vulkan/vulkansurfacetexture_x11.h"
#include "scene/surfaceitem_x11.h"
#include "utils/xcbutils.h"
#include "workspace.h"
#include "x11_standalone_backend.h"
#include "x11_standalone_logging.h"
#include "x11_standalone_overlaywindow.h"

#include <QOpenGLContext>
#include <private/qtx11extras_p.h>

#include <vulkan/vulkan_xcb.h>

#include <algorithm>
#include <chrono>
#include <unistd.h>

namespace KWin
{

VulkanLayer::VulkanLayer(KWin::VulkanBackend *backend)
    : OutputLayer(nullptr)
    , m_backend(backend)
{
}

std::optional<OutputLayerBeginFrameInfo> VulkanLayer::doBeginFrame()
{
    // Get the context
    if (!m_backend->vulkanContext()) {
        qCWarning(KWIN_X11STANDALONE) << "VulkanLayer::doBeginFrame() - no context";
        return std::nullopt;
    }

    // Cast backend to X11StandaloneVulkanBackend to access swapchain
    auto *x11Backend = static_cast<X11StandaloneVulkanBackend *>(m_backend);
    auto *swapchain = x11Backend->swapchain();
    if (!swapchain || !swapchain->isValid()) {
        qCWarning(KWIN_X11STANDALONE) << "VulkanLayer::doBeginFrame() - no swapchain";
        return std::nullopt;
    }

    // Wait for the previous frame's fence to complete (CPU wait is still needed
    // to ensure the command buffer from the previous frame is not in use)
    swapchain->waitForFence();
    swapchain->resetFence();

    // Acquire the next swapchain image
    // This signals imageAvailableSemaphore when the image is ready
    uint32_t imageIndex = swapchain->acquireNextImage();
    if (imageIndex == UINT32_MAX) {
        // Swapchain needs recreation (resize, etc.)
        if (swapchain->needsRecreation()) {
            qCDebug(KWIN_X11STANDALONE) << "Swapchain needs recreation";
            // Try to recreate - for now just fail this frame
            return std::nullopt;
        }
        qCWarning(KWIN_X11STANDALONE) << "VulkanLayer::doBeginFrame() - failed to acquire image";
        return std::nullopt;
    }

    // Get the framebuffer for this image
    auto *framebuffer = swapchain->currentFramebuffer();
    if (!framebuffer) {
        qCWarning(KWIN_X11STANDALONE) << "VulkanLayer::doBeginFrame() - no framebuffer";
        return std::nullopt;
    }

    // Create a VulkanRenderTarget with the framebuffer
    auto vulkanRenderTarget = std::make_unique<VulkanRenderTarget>(framebuffer);

    // Set up GPU-GPU synchronization info
    // The renderer will:
    // - Wait on imageAvailableSemaphore before writing to the framebuffer
    // - Signal renderFinishedSemaphore when rendering is complete
    // - Signal inFlightFence when the command buffer is submitted
    VulkanSyncInfo syncInfo;
    syncInfo.imageAvailableSemaphore = swapchain->imageAvailableSemaphore();
    syncInfo.renderFinishedSemaphore = swapchain->renderFinishedSemaphore();
    syncInfo.inFlightFence = swapchain->inFlightFence();
    vulkanRenderTarget->setSyncInfo(syncInfo);

    // Create a RenderTarget with the VulkanRenderTarget
    RenderTarget renderTarget(vulkanRenderTarget.release());

    return OutputLayerBeginFrameInfo{
        .renderTarget = std::move(renderTarget),
        .repaint = infiniteRegion(),
    };
}

bool VulkanLayer::doEndFrame(const QRegion &renderedRegion, const QRegion &damagedRegion, OutputFrame *frame)
{
    Q_UNUSED(renderedRegion);
    Q_UNUSED(damagedRegion);
    Q_UNUSED(frame);

    // Rendering has been submitted by the ItemRendererVulkan
    // The actual presentation happens in X11StandaloneVulkanBackend::present()
    return true;
}

DrmDevice *VulkanLayer::scanoutDevice() const
{
    return nullptr;
}

QHash<uint32_t, QList<uint64_t>> VulkanLayer::supportedDrmFormats() const
{
    return {};
}

X11StandaloneVulkanBackend::X11StandaloneVulkanBackend(X11StandaloneBackend *backend)
    : m_backend(backend)
{
    m_overlayWindow = std::make_unique<OverlayWindowX11>(backend);
    m_layer = std::make_unique<VulkanLayer>(this);

    // Note: We can't connect to workspace() here as it might not be available yet
    // This connection should be made after workspace is initialized
}

X11StandaloneVulkanBackend::~X11StandaloneVulkanBackend()
{
    if (isFailed()) {
        m_overlayWindow->destroy();
    }

    // Destruction order is important:
    // 1. Swapchain (holds framebuffers, image views, sync objects)
    // 2. Context (holds VMA allocator, command/descriptor pools)
    // 3. Surface (needs instance)
    // 4. X11 resources
    // 5. Base class handles device and instance destruction
    m_swapchain.reset();
    m_context.reset();

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_colormap != XCB_COLORMAP_NONE) {
        xcb_free_colormap(connection(), m_colormap);
        m_colormap = XCB_COLORMAP_NONE;
    }

    if (m_window) {
        xcb_destroy_window(connection(), m_window);
    }

    m_overlayWindow->destroy();
}

void X11StandaloneVulkanBackend::init()
{
    if (!initInstance()) {
        setFailed(QStringLiteral("Failed to initialize Vulkan instance"));
        return;
    }

    if (!initPhysicalDevice()) {
        setFailed(QStringLiteral("Failed to initialize Vulkan physical device"));
        return;
    }

    if (!initDevice()) {
        setFailed(QStringLiteral("Failed to initialize Vulkan device"));
        return;
    }

    m_context = std::make_unique<KWin::VulkanContext>(this);
    if (!m_context->isValid()) {
        setFailed(QStringLiteral("Failed to initialize Vulkan context"));
        return;
    }

    qCDebug(KWIN_X11STANDALONE) << "Successfully created Vulkan context";

    // Create overlay window first - we need the X11 window for the Vulkan surface
    if (!initOverlayWindow()) {
        setFailed(QStringLiteral("Failed to initialize overlay window"));
        return;
    }

    // Now create the Vulkan surface using the overlay window
    if (!initSurface()) {
        setFailed(QStringLiteral("Failed to initialize Vulkan surface"));
        return;
    }

    // Finally create the swapchain
    if (!initSwapchain()) {
        setFailed(QStringLiteral("Failed to initialize Vulkan swapchain"));
        return;
    }

    qCDebug(KWIN_X11STANDALONE) << "Successfully initialized Vulkan backend";
}

bool X11StandaloneVulkanBackend::initInstance()
{
    // Required extensions for X11 surface
    QList<const char *> requiredExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XCB_SURFACE_EXTENSION_NAME};

    return createInstance(requiredExtensions);
}

bool X11StandaloneVulkanBackend::initPhysicalDevice()
{
    return selectPhysicalDevice();
}

bool X11StandaloneVulkanBackend::initDevice()
{
    // Required device extensions
    QList<const char *> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    return createDevice(requiredDeviceExtensions);
}

bool X11StandaloneVulkanBackend::initSurface()
{
    VkXcbSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.connection = connection();
    createInfo.window = m_window;

    VkResult result = vkCreateXcbSurfaceKHR(instance(), &createInfo, nullptr, &m_surface);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_X11STANDALONE) << "Failed to create Vulkan surface:" << result;
        return false;
    }

    qCDebug(KWIN_X11STANDALONE) << "Successfully created Vulkan surface";

    // Check if the surface is supported by the selected physical device
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice(), graphicsQueueFamily(), m_surface, &presentSupport);

    if (!presentSupport) {
        qCWarning(KWIN_X11STANDALONE) << "Vulkan surface is not supported by the physical device";
        return false;
    }

    return true;
}

bool X11StandaloneVulkanBackend::initSwapchain()
{
    if (!m_context) {
        return false;
    }

    const QSize size = workspace()->geometry().size();
    m_swapchain = VulkanSwapchain::create(m_context.get(), m_surface, size);
    if (!m_swapchain || !m_swapchain->isValid()) {
        qCWarning(KWIN_X11STANDALONE) << "Failed to create Vulkan swapchain";
        return false;
    }

    qCDebug(KWIN_X11STANDALONE) << "Successfully created Vulkan swapchain";

    // Connect the swapchain's render pass to the pipeline manager
    m_context->pipelineManager()->setRenderPass(m_swapchain->renderPass()->renderPass());

    return true;
}

bool X11StandaloneVulkanBackend::initOverlayWindow()
{
    if (overlayWindow()->create()) {
        xcb_connection_t *const c = connection();

        // Create colormap
        xcb_visualid_t visual = Xcb::defaultScreen()->root_visual;
        m_colormap = xcb_generate_id(c);
        xcb_create_colormap(c, XCB_COLORMAP_ALLOC_NONE, m_colormap, rootWindow(), visual);

        // Create window
        const QSize size = workspace()->geometry().size();
        m_window = xcb_generate_id(c);
        xcb_create_window(c, Xcb::defaultDepth(), m_window, overlayWindow()->window(),
                          0, 0, size.width(), size.height(), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          visual, XCB_CW_COLORMAP, &m_colormap);

        overlayWindow()->setup(m_window);
        qCDebug(KWIN_X11STANDALONE) << "Successfully created and setup overlay window";
        return true;
    } else {
        qCCritical(KWIN_X11STANDALONE) << "Failed to create overlay window";
        return false;
    }
}

std::unique_ptr<SurfaceTexture> X11StandaloneVulkanBackend::createSurfaceTextureX11(SurfacePixmapX11 *pixmap)
{
    return std::make_unique<X11StandaloneVulkanSurfaceTexture>(this, pixmap);
}

bool X11StandaloneVulkanBackend::present(Output *output, const std::shared_ptr<OutputFrame> &frame)
{
    Q_UNUSED(output);
    m_frame = frame;

    if (!m_swapchain || !m_swapchain->isValid()) {
        qCWarning(KWIN_X11STANDALONE) << "present() - no valid swapchain";
        return false;
    }

    // Present the rendered frame to the swapchain
    bool presentSuccess = m_swapchain->present();

    // Get the presentation timestamp
    auto presentTime = std::chrono::steady_clock::now();
    auto presentNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(presentTime.time_since_epoch());

    if (m_frame) {
        if (presentSuccess) {
            qCDebug(KWIN_X11STANDALONE) << "Present successful, marking frame as presented";
            m_frame->presented(presentNanos, PresentationMode::VSync);
        } else {
            // Present failed, likely swapchain needs recreation
            qCWarning(KWIN_X11STANDALONE) << "Present failed, checking if swapchain needs recreation";
            if (m_swapchain->needsRecreation()) {
                qCDebug(KWIN_X11STANDALONE) << "Swapchain needs recreation after present failure";
                // Try to recreate
                const QSize size = workspace()->geometry().size();
                if (!m_swapchain->recreate(size)) {
                    qCWarning(KWIN_X11STANDALONE) << "Failed to recreate swapchain";
                }
            }
            // Even if present failed, we still mark the frame as presented to avoid blocking
            m_frame->presented(presentNanos, PresentationMode::VSync);
        }
        m_frame.reset();
    }

    // Advance to the next frame in the swapchain
    m_swapchain->advanceFrame();

    // Show the overlay window
    if (overlayWindow()->window()) {
        qCDebug(KWIN_X11STANDALONE) << "Showing overlay window";
        overlayWindow()->show();
    } else {
        qCWarning(KWIN_X11STANDALONE) << "Overlay window is not valid, cannot show";
    }

    return presentSuccess;
}

bool X11StandaloneVulkanBackend::makeCurrent()
{
    // Vulkan doesn't have the concept of "making current" like OpenGL
    // but we can ensure the context is ready for use
    return m_context && m_context->isValid();
}

void X11StandaloneVulkanBackend::doneCurrent()
{
    // Vulkan doesn't have the concept of "making current" like OpenGL
    // This is a no-op
}

KWin::VulkanContext *X11StandaloneVulkanBackend::vulkanContext() const
{
    return m_context.get();
}

OverlayWindow *X11StandaloneVulkanBackend::overlayWindow() const
{
    return m_overlayWindow.get();
}

OutputLayer *X11StandaloneVulkanBackend::primaryLayer(Output *output)
{
    return m_layer.get();
}

void X11StandaloneVulkanBackend::screenGeometryChanged()
{
    const QSize size = workspace()->geometry().size();
    doneCurrent();

    if (m_window) {
        const uint16_t mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
        const uint32_t values[] = {uint32_t(size.width()), uint32_t(size.height())};
        xcb_configure_window(connection(), m_window, mask, values);
    }

    overlayWindow()->resize(size);
    Xcb::sync();
}

xcb_connection_t *X11StandaloneVulkanBackend::connection() const
{
    return m_backend->connection();
}

X11StandaloneVulkanSurfaceTexture::X11StandaloneVulkanSurfaceTexture(X11StandaloneVulkanBackend *backend, SurfacePixmapX11 *pixmap)
    : KWin::VulkanSurfaceTextureX11(backend, pixmap)
    , m_backend(backend)
{
}

bool X11StandaloneVulkanSurfaceTexture::create()
{
    // Delegate to the base implementation
    return KWin::VulkanSurfaceTextureX11::create();
}

void X11StandaloneVulkanSurfaceTexture::update(const QRegion &region)
{
    // Delegate to the base implementation
    KWin::VulkanSurfaceTextureX11::update(region);
}

} // namespace KWin

#include "moc_x11_standalone_vulkan_backend.cpp"
