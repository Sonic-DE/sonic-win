/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkansurfacetexture_x11.h"
#include "core/graphicsbuffer.h"
#include "effect/xcb.h"
#include "scene/surfaceitem_x11.h"
#include "utils/common.h"
#include "utils/drm_format_helper.h"
#include "utils/filedescriptor.h"
#include "vulkanbackend.h"
#include "vulkanbuffer.h"
#include "vulkancontext.h"
#include "vulkantexture.h"

#include <QImage>
#include <cstring>
#include <fcntl.h>
#include <libdrm/drm_fourcc.h>
#include <unistd.h>
#include <xcb/dri3.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "scene/surfaceitem_x11.h"
#include "x11window.h"

namespace KWin
{

// DRM format names for R8 and GR88 (not defined in older drm_fourcc.h)
#ifndef DRM_FORMAT_R8
#define DRM_FORMAT_R8 fourcc_code('R', '8', ' ', ' ')
#endif
#ifndef DRM_FORMAT_GR88
#define DRM_FORMAT_GR88 fourcc_code('G', 'R', '8', '8')
#endif

// DRI3 capability flags
struct Dri3Capabilities
{
    bool dri3Extension = false; // DRI3 X11 extension present
    bool gbmModifiers = false; // GBM supports modifiers (DRI3 1.2+)
    bool syncobjs = false; // DRM syncobjs supported (DRI3 1.4)
    bool externalMemory = false; // Vulkan external memory support (NVIDIA)

    Dri3Capabilities() = default;
    ~Dri3Capabilities() = default;

    Dri3Capabilities(const Dri3Capabilities &) = delete;
    Dri3Capabilities &operator=(const Dri3Capabilities &) = delete;
};

// Static cache for DRI3 capabilities - probed once per compositor session
static std::unique_ptr<Dri3Capabilities> g_dri3Capabilities;

// Forward declaration
static std::unique_ptr<Dri3Capabilities> probeDri3Capabilities(xcb_connection_t *c, xcb_pixmap_t pixmap);

// Helper to get DRI3 capabilities (probes once if not already done)
static const Dri3Capabilities &getDri3Capabilities(xcb_connection_t *c, xcb_pixmap_t pixmap)
{
    if (!g_dri3Capabilities) {
        g_dri3Capabilities = probeDri3Capabilities(c, pixmap);
    }
    return *g_dri3Capabilities;
}

// Helper to check if DRI3 extension is available with full capability probing
static std::unique_ptr<Dri3Capabilities> probeDri3Capabilities(xcb_connection_t *c, xcb_pixmap_t pixmap)
{
    auto caps = std::make_unique<Dri3Capabilities>();

    // Check if DRI3 extension is present
    const xcb_query_extension_reply_t *extReply = xcb_get_extension_data(c, &xcb_dri3_id);
    caps->dri3Extension = extReply && extReply->present;

    if (!caps->dri3Extension) {
        qCDebug(KWIN_VULKAN) << "[DRI3] DRI3 extension not available";
        return caps;
    }

    qCDebug(KWIN_VULKAN) << "[DRI3] DRI3 extension available, probing capabilities...";

    // Probe GBM modifiers support (DRI3 1.2+)
#ifdef GBM_BO_IMPORT_FD_MODIFIER
    // GBM_BO_IMPORT_FD_MODIFIER is available at compile time
    caps->gbmModifiers = true;
    qCDebug(KWIN_VULKAN) << "[DRI3] GBM_BO_IMPORT_FD_MODIFIER available at compile time (DRI3 1.2+)";
#else
    qCDebug(KWIN_VULKAN) << "[DRI3] GBM_BO_IMPORT_FD_MODIFIER not defined at compile time";
#endif

    // Probe DRM syncobj support (DRI3 1.4) - try to open DRM device directly
    // Try common DRM device paths
    const char *drmDevicePaths[] = {
        "/dev/dri/card0",
        "/dev/dri/renderD128",
        "/dev/dri/card1",
        "/dev/dri/renderD129",
        nullptr};

    for (int i = 0; drmDevicePaths[i] != nullptr; ++i) {
        int fd = open(drmDevicePaths[i], O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }

        // Test syncobj support
        struct drm_syncobj_create syncobjTest = {};
        if (drmIoctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &syncobjTest) == 0) {
            caps->syncobjs = true;
            qCDebug(KWIN_VULKAN) << "[DRI3] DRM syncobjs supported (DRI3 1.4) via" << drmDevicePaths[i];

            // Clean up the test syncobj
            struct drm_syncobj_destroy destroyTest = {};
            destroyTest.handle = syncobjTest.handle;
            drmIoctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &destroyTest);
        } else {
            qCDebug(KWIN_VULKAN) << "[DRI3] DRM syncobjs not supported on" << drmDevicePaths[i] << "(DRI3 1.4)";
        }

        close(fd);
        break; // Found a usable DRM device
    }

    return caps;
}

VulkanSurfaceTextureX11::VulkanSurfaceTextureX11(VulkanBackend *backend, SurfacePixmapX11 *pixmap)
    : VulkanSurfaceTexture(backend)
    , m_pixmap(pixmap)
    , m_context(backend->vulkanContext())
{
}

VulkanSurfaceTextureX11::~VulkanSurfaceTextureX11()
{
    // Clear the texture planes and staging buffer
    m_texture.reset();
    m_stagingBuffer.reset();
}

bool VulkanSurfaceTextureX11::isValid() const
{
    return m_texture.isValid();
}

bool VulkanSurfaceTextureX11::create()
{
    if (!m_pixmap || !m_pixmap->isValid()) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - invalid pixmap";
        return false;
    }

    if (!m_context) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - no Vulkan context";
        return false;
    }

    const xcb_pixmap_t currentPixmapId = m_pixmap->pixmap();
    const QSize currentSize = m_pixmap->size();

    // Check if we can reuse the existing texture
    // Texture can be reused if:
    // 1. We have a valid texture
    // 2. The size hasn't changed
    if (m_texture.isValid() && m_size == currentSize) {
        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - reusing cached texture for pixmap" << currentPixmapId;
        return true;
    }

    // Texture needs to be recreated - clear the old one
    if (m_texture.isValid()) {
        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - invalidating cached texture (size changed:" << (m_size != currentSize) << ")";
        m_texture.reset();
        m_stagingBuffer.reset();
    }

    m_size = currentSize;
    qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - m_size from m_pixmap->size():" << m_size;
    if (m_size.isEmpty()) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - empty size";
        return false;
    }

    // Log detailed information about the pixmap and its size
    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - pixmap details:";
    qCDebug(KWIN_VULKAN) << "  - Pixmap ID:" << currentPixmapId;
    qCDebug(KWIN_VULKAN) << "  - Actual size:" << m_size;
    qCDebug(KWIN_VULKAN) << "  - Expected size (standard):" << QSize(1920, 1080);
    qCDebug(KWIN_VULKAN) << "  - Size ratio (width):" << (m_size.width() > 0 ? 1920.0 / m_size.width() : 0);
    qCDebug(KWIN_VULKAN) << "  - Size ratio (height):" << (m_size.height() > 0 ? 1080.0 / m_size.height() : 0);

    qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - attempting to create texture for pixmap of size" << m_size;

    // Check DRI3 capabilities before attempting DMA-BUF import (probed once and cached)
    const auto &caps = getDri3Capabilities(connection(), currentPixmapId);

    // Try DMA-BUF import first (if available)
    static bool forceCpuUpload = qgetenv("KWIN_VULKAN_FORCE_CPU") == "1";
    if (m_context->supportsDmaBufImport() && caps.dri3Extension && !forceCpuUpload) {
        qCInfo(KWIN_VULKAN) << "[DMA-BUF] Capabilities: DRI3=" << caps.dri3Extension
                            << ", GBM modifiers=" << caps.gbmModifiers
                            << ", syncobjs=" << caps.syncobjs;
        qCInfo(KWIN_VULKAN) << "[DMA-BUF] Import supported, attempting for pixmap size" << m_size;
        if (createWithDmaBuf()) {
            m_useDmaBuf = true;
            qCInfo(KWIN_VULKAN) << "[DMA-BUF] SUCCESS: Using zero-copy DMA-BUF import for pixmap" << currentPixmapId;
            return true;
        } else {
            qCWarning(KWIN_VULKAN) << "[DMA-BUF] Import failed, falling back to CPU upload path";
        }
    } else {
        qCInfo(KWIN_VULKAN) << "[DMA-BUF] Import not supported by Vulkan implementation or DRI3, using CPU upload";
    }

    // Fall back to CPU upload
    if (createWithCpuUpload()) {
        m_useDmaBuf = false;
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - SUCCESS: using CPU upload for pixmap" << currentPixmapId;
        return true;
    }

    qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - failed to create texture";
    return false;
}

bool VulkanSurfaceTextureX11::createWithDmaBuf()
{
    xcb_connection_t *c = connection();
    if (!c) {
        qCWarning(KWIN_VULKAN) << "[DMA-BUF] No X11 connection";
        return false;
    }

    xcb_pixmap_t pixmap = m_pixmap->pixmap();
    if (pixmap == XCB_PIXMAP_NONE) {
        qCWarning(KWIN_VULKAN) << "[DMA-BUF] Invalid pixmap";
        return false;
    }

    // Use xcb_dri3_buffer_from_pixmap to get DMA-BUF fd
    xcb_dri3_buffer_from_pixmap_cookie_t cookie = xcb_dri3_buffer_from_pixmap(c, pixmap);
    xcb_dri3_buffer_from_pixmap_reply_t *reply = xcb_dri3_buffer_from_pixmap_reply(c, cookie, nullptr);
    if (!reply) {
        qCWarning(KWIN_VULKAN) << "[DMA-BUF] xcb_dri3_buffer_from_pixmap failed";
        return false;
    }

    // Get the file descriptor (there are reply->nfd fds)
    int *fds = xcb_dri3_buffer_from_pixmap_reply_fds(c, reply);
    if (!fds || reply->nfd < 1) {
        qCWarning(KWIN_VULKAN) << "[DMA-BUF] No file descriptors in reply";
        free(reply);
        return false;
    }

    qCDebug(KWIN_VULKAN) << "[DMA-BUF] Buffer from pixmap: nfd=" << reply->nfd
                         << "width=" << reply->width << "height=" << reply->height
                         << "stride=" << reply->stride << "depth=" << reply->depth
                         << "bpp=" << reply->bpp;

    // Build DmaBufAttributes from the reply
    DmaBufAttributes attributes;
    attributes.width = reply->width;
    attributes.height = reply->height;
    attributes.planeCount = reply->nfd;

    // Determine DRM format from depth and bpp
    // X11 pixmaps are typically XRGB8888 or ARGB8888
    if (reply->depth == 24 && reply->bpp == 32) {
        attributes.format = DRM_FORMAT_XRGB8888;
    } else if (reply->depth == 32 && reply->bpp == 32) {
        attributes.format = DRM_FORMAT_ARGB8888;
    } else {
        qCWarning(KWIN_VULKAN) << "[DMA-BUF] Unsupported depth/bpp:" << reply->depth << "/" << reply->bpp;
        free(reply);
        return false;
    }

    // Set up plane attributes
    for (int i = 0; i < reply->nfd && i < 4; i++) {
        // Duplicate the fd since we don't own it
        int fd = dup(fds[i]);
        if (fd < 0) {
            qCWarning(KWIN_VULKAN) << "[DMA-BUF] Failed to duplicate fd for plane" << i;
            // Clean up already duplicated fds
            for (int j = 0; j < i; j++) {
                close(attributes.fd[j].get());
            }
            free(reply);
            return false;
        }
        attributes.fd[i] = FileDescriptor(fd);
        attributes.offset[i] = 0; // X11 DRI3 doesn't provide offsets
        attributes.pitch[i] = reply->stride;
    }

    // Modifier is typically DRM_FORMAT_MOD_INVALID for X11 pixmaps
    attributes.modifier = DRM_FORMAT_MOD_INVALID;

    free(reply);

    // Check if this is a multi-plane format (YUV)
    const auto formatInfo = FormatInfo::get(attributes.format);
    if (formatInfo && formatInfo->yuvConversion()) {
        qCDebug(KWIN_VULKAN) << "[DMA-BUF] Multi-plane YUV format detected:" << attributes.format;
        return createMultiPlaneWithDmaBuf(attributes);
    }

    // Single-plane import
    auto texture = m_context->importDmaBufAsTexture(attributes);
    if (!texture || !texture->isValid()) {
        qCWarning(KWIN_VULKAN) << "[DMA-BUF] Failed to import single-plane DMA-BUF";
        return false;
    }

    m_texture.planes.append(std::shared_ptr<VulkanTexture>(texture.release()));
    qCDebug(KWIN_VULKAN) << "[DMA-BUF] Successfully imported single-plane DMA-BUF";
    return true;
}

bool VulkanSurfaceTextureX11::createMultiPlaneWithDmaBuf(const DmaBufAttributes &attributes)
{
    const auto formatInfo = FormatInfo::get(attributes.format);
    if (!formatInfo || !formatInfo->yuvConversion()) {
        qCWarning(KWIN_VULKAN) << "[Multi-Plane] Not a YUV format:" << attributes.format;
        return false;
    }

    const auto yuvConversion = formatInfo->yuvConversion();
    const auto &planes = yuvConversion->plane;

    if (attributes.planeCount != planes.size()) {
        qCWarning(KWIN_VULKAN) << "[Multi-Plane] Plane count mismatch: expected" << planes.size()
                               << "got" << attributes.planeCount;
        return false;
    }

    qCDebug(KWIN_VULKAN) << "[Multi-Plane] Importing" << planes.size() << "planes for format" << attributes.format;

    // Import each plane separately
    for (int i = 0; i < planes.size(); i++) {
        const auto &planeInfo = planes[i];

        // Calculate plane size (may be smaller than overall size due to subsampling)
        QSize planeSize(
            attributes.width / planeInfo.widthDivisor,
            attributes.height / planeInfo.heightDivisor);

        // Convert plane DRM format to Vulkan format
        VkFormat vkFormat;
        switch (planeInfo.format) {
        case DRM_FORMAT_R8:
            vkFormat = VK_FORMAT_R8_UNORM;
            break;
        case DRM_FORMAT_GR88:
            vkFormat = VK_FORMAT_R8G8_UNORM;
            break;
        default:
            qCWarning(KWIN_VULKAN) << "[Multi-Plane] Unsupported plane format:" << planeInfo.format;
            m_texture.reset();
            return false;
        }

        qCDebug(KWIN_VULKAN) << "[Multi-Plane] Importing plane" << i
                             << "format" << (planeInfo.format == DRM_FORMAT_R8 ? "R8" : "GR88")
                             << "size" << planeSize;

        auto texture = m_context->importDmaBufPlaneAsTexture(attributes, i, vkFormat, planeSize);
        if (!texture || !texture->isValid()) {
            qCWarning(KWIN_VULKAN) << "[Multi-Plane] Failed to import plane" << i;
            m_texture.reset();
            return false;
        }

        m_texture.planes.append(std::shared_ptr<VulkanTexture>(texture.release()));
    }

    qCDebug(KWIN_VULKAN) << "[Multi-Plane] Successfully imported" << m_texture.planes.size() << "planes";
    return true;
}

bool VulkanSurfaceTextureX11::createWithCpuUpload()
{
    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - creating texture with size" << m_size;

    // Log detailed information about the CPU upload path
    qCDebug(KWIN_VULKAN) << "CPU upload texture creation details:";
    qCDebug(KWIN_VULKAN) << "  - Pixmap size:" << m_size;
    qCDebug(KWIN_VULKAN) << "  - Pixmap ID:" << m_pixmap->pixmap();
    qCDebug(KWIN_VULKAN) << "  - Memory allocation: Host-visible staging buffer + device-local texture";

    // Create a Vulkan texture with the appropriate format
    // X11 data is sRGB encoded, so we use SRGB format for correct sampling
    // This makes the hardware do sRGB-to-linear conversion on texture fetch
    VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
    qCDebug(KWIN_VULKAN) << "  - Texture format:" << format << "(VK_FORMAT_B8G8R8A8_SRGB)";

    auto texture = VulkanTexture::allocate(m_context, m_size, format);
    if (!texture || !texture->isValid()) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to allocate texture";
        return false;
    }
    m_texture.planes.append(std::shared_ptr<VulkanTexture>(texture.release()));

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - texture allocated successfully";

    // Create staging buffer for CPU → GPU transfers
    const VkDeviceSize bufferSize = m_size.width() * m_size.height() * 4; // BGRA = 4 bytes per pixel
    m_stagingBuffer = VulkanBuffer::createStagingBuffer(m_context, bufferSize);

    if (!m_stagingBuffer || !m_stagingBuffer->isValid()) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to create staging buffer";
        m_texture.reset();
        return false;
    }

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - staging buffer created successfully";

    // Do initial upload
    updateWithCpuUpload(QRegion(QRect(QPoint(0, 0), m_size)));

    return true;
}

void VulkanSurfaceTextureX11::update(const QRegion &region)
{
    if (!m_pixmap || !m_pixmap->isValid()) {
        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::update - invalid pixmap";
        return;
    }

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::update - region:" << region.boundingRect()
                         << "using DMA-BUF:" << m_useDmaBuf
                         << "texture size:" << m_size
                         << "pixmap size:" << m_pixmap->size();

    if (m_useDmaBuf) {
        // For DMA-BUF, we need to issue an external memory acquire barrier
        // to ensure the GPU sees the updated content from X server

        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::update - DMA-BUF path - zero-copy update";
        qCDebug(KWIN_VULKAN) << "  - Update region:" << region.boundingRect();
        qCDebug(KWIN_VULKAN) << "  - Region count:" << region.rectCount();
        qCDebug(KWIN_VULKAN) << "  - Plane count:" << m_texture.planes.size();

        // Log detailed information about the DMA-BUF texture
        if (m_texture.isValid() && !m_texture.planes.isEmpty()) {
            // Issue external memory acquire barriers for all planes
            // This ensures the GPU sees the updated content from the X server
            VkCommandBuffer cmd = m_context->beginSingleTimeCommands();

            for (int planeIdx = 0; planeIdx < m_texture.planes.size(); planeIdx++) {
                VulkanTexture *plane = m_texture.planes[planeIdx].get();
                qCDebug(KWIN_VULKAN) << "  - Plane" << planeIdx << ":";
                qCDebug(KWIN_VULKAN) << "    * Texture valid:" << plane->isValid();
                qCDebug(KWIN_VULKAN) << "    * Current layout:" << plane->currentLayout();
                qCDebug(KWIN_VULKAN) << "    * Image:" << plane->image();
                qCDebug(KWIN_VULKAN) << "    * Image view:" << plane->imageView();
                qCDebug(KWIN_VULKAN) << "    * Texture size:" << plane->size();

                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = plane->currentLayout();
                barrier.newLayout = plane->currentLayout(); // Keep the same layout
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = plane->image();
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0;
                barrier.subresourceRange.layerCount = 1;
                // Use memory read bit for external memory synchronization
                // This tells the driver that new data may be available from external source
                barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

                vkCmdPipelineBarrier(cmd,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     0,
                                     0, nullptr,
                                     0, nullptr,
                                     1, &barrier);

                qCDebug(KWIN_VULKAN) << "    * Issued external memory acquire barrier";
            }

            m_context->endSingleTimeCommands(cmd);
            qCDebug(KWIN_VULKAN) << "  - Memory synchronization complete for" << m_texture.planes.size() << "planes";
        }

        return;
    }

    // CPU upload path
    updateWithCpuUpload(region);
}

void VulkanSurfaceTextureX11::updateWithCpuUpload(const QRegion &region)
{
    if (!m_texture.isValid() || m_texture.planes.isEmpty() || !m_stagingBuffer) {
        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - texture or staging buffer not available";
        return;
    }

    VulkanTexture *firstPlane = m_texture.planes.first().get();

    const xcb_pixmap_t nativePixmap = m_pixmap->pixmap();
    if (nativePixmap == XCB_PIXMAP_NONE) {
        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - invalid pixmap";
        return;
    }

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - updating region" << region.boundingRect();

    // Use the global X11 connection
    xcb_connection_t *c = connection();
    if (!c) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - no X11 connection";
        return;
    }

    // For simplicity, we update the entire texture when any region changes
    // A more optimized implementation would only update the damaged regions
    const QRect bounds = region.boundingRect();
    const int x = bounds.x();
    const int y = bounds.y();
    const int width = bounds.width();
    const int height = bounds.height();

    // Get the image data from X11
    xcb_get_image_cookie_t cookie = xcb_get_image(
        c,
        XCB_IMAGE_FORMAT_Z_PIXMAP,
        nativePixmap,
        x, y,
        width, height,
        ~0);

    xcb_get_image_reply_t *reply = xcb_get_image_reply(c, cookie, nullptr);
    if (!reply) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - xcb_get_image failed";
        return;
    }

    const uint8_t *data = xcb_get_image_data(reply);
    const int dataLength = xcb_get_image_data_length(reply);
    const uint8_t depth = reply->depth;
    const int bytesPerPixel = (depth == 24) ? 4 : 4; // X11 pads 24-bit to 32-bit

    // Log detailed information about the image data received from X11
    qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - xcb_get_image details:";
    qCWarning(KWIN_VULKAN) << "  - Depth:" << depth;
    qCWarning(KWIN_VULKAN) << "  - Data length:" << dataLength;
    qCWarning(KWIN_VULKAN) << "  - Expected data length:" << (width * height * bytesPerPixel);
    qCWarning(KWIN_VULKAN) << "  - Region size:" << width << "x" << height;
    qCWarning(KWIN_VULKAN) << "  - Texture size:" << m_size.width() << "x" << m_size.height();
    qCWarning(KWIN_VULKAN) << "  - Bytes per pixel:" << bytesPerPixel;

    // Check for potential sizing issues
    if (dataLength != width * height * bytesPerPixel) {
        qCWarning(KWIN_VULKAN) << "  - POTENTIAL ISSUE: Data length mismatch";
    }

    // Copy data to staging buffer
    void *mappedData = m_stagingBuffer->map();
    if (!mappedData) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - failed to map staging buffer";
        free(reply);
        return;
    }

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - staging buffer mapped successfully";

    if (mappedData) {
        // For depth 24 (XRGB), the X byte (padding/alpha) may be 0 or garbage.
        // We need to set it to 0xFF for proper alpha blending.
        const bool needsAlphaFix = (depth == 24);

        if (needsAlphaFix) {
            // Copy pixel-by-pixel, fixing alpha channel
            // X11 XRGB is stored as BGRX in memory (little-endian), byte 3 is the X/alpha
            uint8_t *dst = static_cast<uint8_t *>(mappedData);
            const int dstStride = m_size.width() * 4;

            if (x == 0 && y == 0 && width == m_size.width() && height == m_size.height()) {
                // Full update
                const int pixelCount = width * height;
                for (int i = 0; i < pixelCount; ++i) {
                    dst[i * 4 + 0] = data[i * 4 + 0]; // B
                    dst[i * 4 + 1] = data[i * 4 + 1]; // G
                    dst[i * 4 + 2] = data[i * 4 + 2]; // R
                    dst[i * 4 + 3] = 0xFF; // A = opaque
                }
            } else {
                // Partial update
                const int srcStride = width * 4;
                dst += (y * dstStride) + (x * 4);

                for (int row = 0; row < height; ++row) {
                    for (int col = 0; col < width; ++col) {
                        const int srcIdx = row * srcStride + col * 4;
                        const int dstIdx = row * dstStride + col * 4;
                        dst[dstIdx + 0] = data[srcIdx + 0]; // B
                        dst[dstIdx + 1] = data[srcIdx + 1]; // G
                        dst[dstIdx + 2] = data[srcIdx + 2]; // R
                        dst[dstIdx + 3] = 0xFF; // A = opaque
                    }
                }
            }
        } else {
            // Depth 32 (ARGB) - copy with forced alpha=0xFF
            // Many X11 apps don't properly set alpha even on 32-bit visuals
            uint8_t *dst = static_cast<uint8_t *>(mappedData);
            const int dstStride = m_size.width() * 4;

            if (x == 0 && y == 0 && width == m_size.width() && height == m_size.height()) {
                // Full update
                const int pixelCount = width * height;
                for (int i = 0; i < pixelCount; ++i) {
                    dst[i * 4 + 0] = data[i * 4 + 0]; // B
                    dst[i * 4 + 1] = data[i * 4 + 1]; // G
                    dst[i * 4 + 2] = data[i * 4 + 2]; // R
                    dst[i * 4 + 3] = 0xFF; // A = opaque
                }
            } else {
                // Partial update
                const int srcStride = width * 4;
                dst += (y * dstStride) + (x * 4);

                for (int row = 0; row < height; ++row) {
                    for (int col = 0; col < width; ++col) {
                        const int srcIdx = row * srcStride + col * 4;
                        const int dstIdx = row * dstStride + col * 4;
                        dst[dstIdx + 0] = data[srcIdx + 0]; // B
                        dst[dstIdx + 1] = data[srcIdx + 1]; // G
                        dst[dstIdx + 2] = data[srcIdx + 2]; // R
                        dst[dstIdx + 3] = 0xFF; // A = opaque
                    }
                }
            }
        }
        m_stagingBuffer->unmap();
        // Flush the staging buffer to ensure data is visible to GPU
        // VMA may allocate non-coherent memory, so explicit flush is required
        m_stagingBuffer->flush(0, m_size.width() * m_size.height() * 4);
    }

    free(reply);

    // Copy from staging buffer to texture using a command buffer
    VkCommandBuffer cmd = m_context->beginSingleTimeCommands();
    if (cmd == VK_NULL_HANDLE) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - failed to begin command buffer";
        return;
    }

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - command buffer allocated successfully";

    // Transition image to transfer destination layout
    firstPlane->transitionLayout(cmd,
                                 firstPlane->currentLayout(),
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Copy buffer to image
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    // bufferRowLength specifies the row stride in PIXELS (not bytes)
    // We always write with full texture stride, so set this to full width
    copyRegion.bufferRowLength = static_cast<uint32_t>(m_size.width());
    copyRegion.bufferImageHeight = static_cast<uint32_t>(m_size.height());
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {static_cast<uint32_t>(m_size.width()), static_cast<uint32_t>(m_size.height()), 1};

    vkCmdCopyBufferToImage(cmd,
                           m_stagingBuffer->buffer(),
                           firstPlane->image(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copyRegion);

    // Transition back to shader read optimal layout
    firstPlane->transitionLayout(cmd,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    m_context->endSingleTimeCommands(cmd);

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - texture updated successfully";
}

} // namespace KWin
