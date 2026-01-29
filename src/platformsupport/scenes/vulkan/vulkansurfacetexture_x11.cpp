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
#include "utils/filedescriptor.h"
#include "vulkanbackend.h"
#include "vulkanbuffer.h"
#include "vulkancontext.h"
#include "vulkantexture.h"

#include <QImage>
#include <cstring>
#include <libdrm/drm_fourcc.h>
#include <unistd.h>
#include <xcb/dri3.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

namespace KWin
{

// Helper to check if DRI3 extension is available
static bool isDri3Available(xcb_connection_t *c)
{
    static int dri3Available = -1;
    if (dri3Available >= 0) {
        return dri3Available == 1;
    }

    // Query DRI3 extension
    xcb_dri3_query_version_cookie_t cookie = xcb_dri3_query_version(c, 1, 2);
    xcb_dri3_query_version_reply_t *reply = xcb_dri3_query_version_reply(c, cookie, nullptr);

    if (reply) {
        qCDebug(KWIN_CORE) << "DRI3 extension available, version:" << reply->major_version << "." << reply->minor_version;
        // We need at least DRI3 1.2 for xcb_dri3_buffers_from_pixmap
        dri3Available = (reply->major_version > 1 || (reply->major_version == 1 && reply->minor_version >= 2)) ? 1 : 0;
        free(reply);
    } else {
        qCDebug(KWIN_CORE) << "DRI3 extension not available";
        dri3Available = 0;
    }

    return dri3Available == 1;
}

// Convert pixmap depth to DRM format
static uint32_t depthToDrmFormat(uint8_t depth)
{
    switch (depth) {
    case 32:
        return DRM_FORMAT_ARGB8888;
    case 24:
        return DRM_FORMAT_XRGB8888;
    case 30:
        return DRM_FORMAT_XRGB2101010;
    case 16:
        return DRM_FORMAT_RGB565;
    default:
        qCWarning(KWIN_CORE) << "Unknown pixmap depth:" << depth;
        return 0;
    }
}

// Convert DRM format to Vulkan format
static VkFormat drmFormatToVkFormat(uint32_t drmFormat)
{
    switch (drmFormat) {
    case DRM_FORMAT_ARGB8888:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case DRM_FORMAT_XRGB8888:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case DRM_FORMAT_ABGR8888:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_XBGR8888:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_RGB888:
        return VK_FORMAT_R8G8B8_UNORM;
    case DRM_FORMAT_BGR888:
        return VK_FORMAT_B8G8R8_UNORM;
    case DRM_FORMAT_RGB565:
        return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case DRM_FORMAT_BGR565:
        return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case DRM_FORMAT_ARGB2101010:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case DRM_FORMAT_XRGB2101010:
        return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case DRM_FORMAT_ABGR2101010:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case DRM_FORMAT_XBGR2101010:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case DRM_FORMAT_ABGR16161616F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    default:
        qCWarning(KWIN_CORE) << "Unknown DRM format:" << Qt::hex << drmFormat;
        return VK_FORMAT_UNDEFINED;
    }
}

VulkanSurfaceTextureX11::VulkanSurfaceTextureX11(VulkanBackend *backend, SurfacePixmapX11 *pixmap)
    : VulkanSurfaceTexture(backend)
    , m_pixmap(pixmap)
    , m_context(backend->vulkanContext())
{
}

VulkanSurfaceTextureX11::~VulkanSurfaceTextureX11()
{
    // Clear base class handles BEFORE destroying m_texture to prevent double-destruction.
    // The base class destructor would otherwise try to destroy these already-freed handles.
    m_image = VK_NULL_HANDLE;
    m_imageView = VK_NULL_HANDLE;

    m_stagingBuffer.reset();
    m_texture.reset();
}

bool VulkanSurfaceTextureX11::create()
{
    if (!m_pixmap || !m_pixmap->isValid()) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::create() - invalid pixmap";
        return false;
    }

    if (!m_context) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::create() - no Vulkan context";
        return false;
    }

    m_size = m_pixmap->size();
    if (m_size.isEmpty()) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::create() - empty size";
        return false;
    }

    qWarning() << "VulkanSurfaceTextureX11::create() - attempting to create texture for pixmap of size" << m_size;

    // Try DMA-BUF import first (if available)
    if (m_context->supportsDmaBufImport()) {
        qWarning() << "VulkanSurfaceTextureX11::create() - DMA-BUF import supported, attempting";
        if (createWithDmaBuf()) {
            m_useDmaBuf = true;
            qWarning() << "VulkanSurfaceTextureX11::create() - SUCCESS: using DMA-BUF import";
            return true;
        } else {
            qWarning() << "VulkanSurfaceTextureX11::create() - DMA-BUF import failed, falling back to CPU upload";
        }
    } else {
        qWarning() << "VulkanSurfaceTextureX11::create() - DMA-BUF import not supported, using CPU upload";
    }

    // Fall back to CPU upload
    if (createWithCpuUpload()) {
        m_useDmaBuf = false;
        qWarning() << "VulkanSurfaceTextureX11::create() - SUCCESS: using CPU upload";
        return true;
    }

    qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::create() - failed to create texture";
    return false;
}

bool VulkanSurfaceTextureX11::createWithDmaBuf()
{
    xcb_connection_t *c = connection();
    if (!c) {
        qCDebug(KWIN_CORE) << "createWithDmaBuf: no X11 connection";
        return false;
    }

    // Check if DRI3 is available (requires DRI3 1.2+)
    if (!isDri3Available(c)) {
        qCDebug(KWIN_CORE) << "createWithDmaBuf: DRI3 1.2+ not available";
        return false;
    }

    const xcb_pixmap_t nativePixmap = m_pixmap->pixmap();
    if (nativePixmap == XCB_PIXMAP_NONE) {
        qCDebug(KWIN_CORE) << "createWithDmaBuf: invalid pixmap";
        return false;
    }

    qCDebug(KWIN_CORE) << "createWithDmaBuf: attempting to import pixmap" << nativePixmap;

    // Query buffers from the pixmap using DRI3
    xcb_dri3_buffers_from_pixmap_cookie_t cookie = xcb_dri3_buffers_from_pixmap(c, nativePixmap);
    xcb_dri3_buffers_from_pixmap_reply_t *reply = xcb_dri3_buffers_from_pixmap_reply(c, cookie, nullptr);

    if (!reply) {
        qCDebug(KWIN_CORE) << "createWithDmaBuf: xcb_dri3_buffers_from_pixmap failed";
        return false;
    }

    qCDebug(KWIN_CORE) << "createWithDmaBuf: received reply with" << reply->nfd << "planes, width:" << reply->width << "height:" << reply->height << "depth:" << reply->depth;

    const int planeCount = reply->nfd;
    if (planeCount <= 0 || planeCount > 4) {
        qCWarning(KWIN_CORE) << "createWithDmaBuf: invalid plane count:" << planeCount;
        free(reply);
        return false;
    }

    // Get the file descriptors from the reply
    int *fds = xcb_dri3_buffers_from_pixmap_reply_fds(c, reply);
    uint32_t *strides = xcb_dri3_buffers_from_pixmap_strides(reply);
    uint32_t *offsets = xcb_dri3_buffers_from_pixmap_offsets(reply);

    // Get the pixmap depth to determine format
    const uint8_t depth = reply->depth;
    qCDebug(KWIN_CORE) << "createWithDmaBuf: pixmap depth is" << depth;
    const uint32_t drmFormat = depthToDrmFormat(depth);
    if (drmFormat == 0) {
        qCWarning(KWIN_CORE) << "createWithDmaBuf: unsupported pixmap depth:" << depth;
        // Close the file descriptors
        for (int i = 0; i < planeCount; i++) {
            close(fds[i]);
        }
        free(reply);
        return false;
    }

    qCDebug(KWIN_CORE) << "createWithDmaBuf: DRM format is" << Qt::hex << drmFormat;

    // Convert DRM format to Vulkan format
    VkFormat vkFormat = drmFormatToVkFormat(drmFormat);
    if (vkFormat == VK_FORMAT_UNDEFINED) {
        qCWarning(KWIN_CORE) << "createWithDmaBuf: unsupported DRM format:" << Qt::hex << drmFormat;
        // Close the file descriptors
        for (int i = 0; i < planeCount; i++) {
            close(fds[i]);
        }
        free(reply);
        return false;
    }

    qCDebug(KWIN_CORE) << "createWithDmaBuf: Vulkan format is" << vkFormat;

    // Build DmaBufAttributes - save values before freeing reply
    DmaBufAttributes dmaBufAttrs;
    dmaBufAttrs.planeCount = planeCount;
    dmaBufAttrs.width = reply->width;
    dmaBufAttrs.height = reply->height;
    dmaBufAttrs.format = drmFormat;
    dmaBufAttrs.modifier = reply->modifier;

    // Save dimensions for debug logging
    const uint16_t pixmapWidth = reply->width;
    const uint16_t pixmapHeight = reply->height;

    for (int i = 0; i < planeCount; i++) {
        // FileDescriptor takes ownership of the fd
        dmaBufAttrs.fd[i] = FileDescriptor(fds[i]);
        dmaBufAttrs.offset[i] = offsets[i];
        dmaBufAttrs.pitch[i] = strides[i];
    }

    free(reply);

    // Import the DMA-BUF as a Vulkan texture
    m_texture = m_context->importDmaBufAsTexture(dmaBufAttrs);
    if (!m_texture) {
        qCWarning(KWIN_CORE) << "createWithDmaBuf: failed to import DMA-BUF as Vulkan texture (null texture)";
        return false;
    }
    if (!m_texture->isValid()) {
        qCWarning(KWIN_CORE) << "createWithDmaBuf: failed to import DMA-BUF as Vulkan texture (invalid texture)";
        return false;
    }

    // Set the VkImage/VkImageView on the base class for compatibility
    m_image = m_texture->image();
    m_imageView = m_texture->imageView();

    // DMA-BUF imported textures typically don't need Y-flip like CPU uploads
    // The coordinate system matches between DRM and Vulkan

    qCDebug(KWIN_CORE) << "createWithDmaBuf: successfully imported" << pixmapWidth << "x" << pixmapHeight
                       << "pixmap with" << planeCount << "plane(s), modifier:" << Qt::hex << dmaBufAttrs.modifier;

    return true;
}

bool VulkanSurfaceTextureX11::createWithCpuUpload()
{
    qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::createWithCpuUpload() - creating texture with size" << m_size;

    // Create a Vulkan texture with the appropriate format
    // X11 data is sRGB encoded, so we use SRGB format for correct sampling
    // This makes the hardware do sRGB-to-linear conversion on texture fetch
    VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;

    m_texture = VulkanTexture::allocate(m_context, m_size, format);
    if (!m_texture) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to allocate texture (null texture)";
        return false;
    }
    if (!m_texture->isValid()) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to allocate texture (invalid texture)";
        return false;
    }

    qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::createWithCpuUpload() - texture allocated successfully";

    // Create staging buffer for CPU → GPU transfers
    const VkDeviceSize bufferSize = m_size.width() * m_size.height() * 4; // BGRA = 4 bytes per pixel
    m_stagingBuffer = VulkanBuffer::createStagingBuffer(m_context, bufferSize);

    if (!m_stagingBuffer) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to create staging buffer (null buffer)";
        m_texture.reset();
        return false;
    }
    if (!m_stagingBuffer->isValid()) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to create staging buffer (invalid buffer)";
        m_texture.reset();
        return false;
    }

    qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::createWithCpuUpload() - staging buffer created successfully";

    // Do initial upload
    updateWithCpuUpload(QRegion(QRect(QPoint(0, 0), m_size)));

    // Set the VkImage/VkImageView on the base class for compatibility
    m_image = m_texture->image();
    m_imageView = m_texture->imageView();

    // Note: We don't apply FlipY here because the viewport already has Y-flip via negative height.
    // The texture data from X11 has Y=0 at top, which matches Vulkan's image coordinate system.

    return true;
}

void VulkanSurfaceTextureX11::update(const QRegion &region)
{
    if (!m_pixmap || !m_pixmap->isValid()) {
        return;
    }

    if (m_useDmaBuf) {
        // For DMA-BUF, we just need to invalidate caches
        // The GPU shares memory with the X server
        // A memory barrier might be needed in the rendering code
        return;
    }

    // CPU upload path
    updateWithCpuUpload(region);
}

void VulkanSurfaceTextureX11::updateWithCpuUpload(const QRegion &region)
{
    if (!m_texture || !m_stagingBuffer) {
        qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - texture or staging buffer not available";
        return;
    }

    const xcb_pixmap_t nativePixmap = m_pixmap->pixmap();
    if (nativePixmap == XCB_PIXMAP_NONE) {
        qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - invalid pixmap";
        return;
    }

    qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - updating region" << region.boundingRect();

    // Use the global X11 connection
    xcb_connection_t *c = connection();
    if (!c) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - no X11 connection";
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
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - xcb_get_image failed";
        return;
    }

    const uint8_t *data = xcb_get_image_data(reply);
    const int dataLength = xcb_get_image_data_length(reply);
    const uint8_t depth = reply->depth;
    const int bytesPerPixel = (depth == 24) ? 4 : 4; // X11 pads 24-bit to 32-bit

    qWarning() << "VulkanSurfaceTextureX11::updateWithCpuUpload() - xcb_get_image: depth=" << depth
               << "dataLength=" << dataLength << "expected=" << (width * height * bytesPerPixel)
               << "size=" << width << "x" << height;

    // Copy data to staging buffer
    void *mappedData = m_stagingBuffer->map();
    if (!mappedData) {
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - failed to map staging buffer";
        free(reply);
        return;
    }

    qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - staging buffer mapped successfully";

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
        qCWarning(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - failed to begin command buffer";
        return;
    }

    qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - command buffer allocated successfully";

    // Transition image to transfer destination layout
    m_texture->transitionLayout(cmd,
                                m_texture->currentLayout(),
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

    if (x == 0 && y == 0 && width == m_size.width() && height == m_size.height()) {
        // Full copy
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {static_cast<uint32_t>(m_size.width()),
                                  static_cast<uint32_t>(m_size.height()), 1};
    } else {
        // Partial copy - buffer data is at the same offset we wrote to in staging buffer
        // The staging buffer has full texture stride, which we specified in bufferRowLength
        copyRegion.bufferOffset = (y * m_size.width() + x) * 4;
        copyRegion.imageOffset = {x, y, 0};
        copyRegion.imageExtent = {static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height), 1};
    }

    vkCmdCopyBufferToImage(cmd,
                           m_stagingBuffer->buffer(),
                           m_texture->image(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copyRegion);

    // Transition image to shader read layout
    m_texture->transitionLayout(cmd,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_PIPELINE_STAGE_TRANSFER_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    qCDebug(KWIN_CORE) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - image layout transition recorded";

    m_context->endSingleTimeCommands(cmd);
}

} // namespace KWin
