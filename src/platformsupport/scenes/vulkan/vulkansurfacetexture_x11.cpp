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
#include <libdrm/drm_fourcc.h>
#include <unistd.h>
#include <xcb/dri3.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include "scene/surfaceitem_x11.h"
#include "x11window.h"

namespace KWin
{

// Helper to check if DRI3 extension is available
static bool isDri3Available(xcb_connection_t *c)
{
    static bool dri3Available = false;

    // Query DRI3 extension
    xcb_dri3_query_version_cookie_t cookie = xcb_dri3_query_version(c, 1, 2);
    xcb_dri3_query_version_reply_t *reply = xcb_dri3_query_version_reply(c, cookie, nullptr);

    if (reply) {
        qCDebug(KWIN_VULKAN) << "DRI3 extension available, version:" << reply->major_version << "." << reply->minor_version;
        // We need at least DRI3 1.2
        dri3Available = (reply->major_version >= 1 && reply->minor_version >= 2) ? true : false;
        free(reply);
    } else {
        qCDebug(KWIN_VULKAN) << "DRI3 extension not available";
        dri3Available = false;
    }

    return dri3Available == 1;
}

VulkanSurfaceTextureX11::VulkanSurfaceTextureX11(VulkanBackend *backend, SurfacePixmapX11 *pixmap)
    : VulkanSurfaceTexture(backend)
    , m_pixmap(pixmap)
    , m_context(backend->vulkanContext())
{
}

VulkanSurfaceTextureX11::~VulkanSurfaceTextureX11()
{
    // Clear the texture and staging buffer
    m_texture.reset();
    m_stagingBuffer.reset();
}

bool VulkanSurfaceTextureX11::isValid() const
{
    return m_texture && m_texture->isValid();
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
    if (m_texture && m_texture->isValid() && m_size == currentSize) {
        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::create() - reusing cached texture for pixmap" << currentPixmapId;
        return true;
    }

    // Texture needs to be recreated - clear the old one
    if (m_texture) {
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

    // Try DMA-BUF import first (if available)
    static bool forceCpuUpload = qgetenv("KWIN_VULKAN_FORCE_CPU") == "1";
    if (m_context->supportsDmaBufImport() && isDri3Available(connection()) && !forceCpuUpload) {
        qCInfo(KWIN_VULKAN) << "[DMA-BUF] Import supported, attempting for pixmap size" << m_size;
        if (createWithDmaBuf()) {
            m_useDmaBuf = true;
            qCInfo(KWIN_VULKAN) << "[DMA-BUF] SUCCESS: Using zero-copy DMA-BUF import for pixmap" << currentPixmapId;
            return true;
        } else {
            qCWarning(KWIN_VULKAN) << "[DMA-BUF] Import failed, falling back to CPU upload path";
        }
    } else {
        qCInfo(KWIN_VULKAN) << "[DMA-BUF] Import not supported by Vulkan implementation, using CPU upload";
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
    // DMA-BUF import not yet implemented
    return false;
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

    m_texture = VulkanTexture::allocate(m_context, m_size, format);
    if (!m_texture) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to allocate texture (null texture)";
        return false;
    }
    if (!m_texture->isValid()) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to allocate texture (invalid texture)";
        return false;
    }

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - texture allocated successfully";

    // Create staging buffer for CPU → GPU transfers
    const VkDeviceSize bufferSize = m_size.width() * m_size.height() * 4; // BGRA = 4 bytes per pixel
    m_stagingBuffer = VulkanBuffer::createStagingBuffer(m_context, bufferSize);

    if (!m_stagingBuffer) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to create staging buffer (null buffer)";
        m_texture.reset();
        return false;
    }
    if (!m_stagingBuffer->isValid()) {
        qCWarning(KWIN_VULKAN) << "VulkanSurfaceTextureX11::createWithCpuUpload() - failed to create staging buffer (invalid buffer)";
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

        // Log detailed information about the DMA-BUF texture
        if (m_texture) {
            qCDebug(KWIN_VULKAN) << "  - Texture valid:" << m_texture->isValid();
            qCDebug(KWIN_VULKAN) << "  - Current layout:" << m_texture->currentLayout();
            qCDebug(KWIN_VULKAN) << "  - Image:" << m_texture->image();
            qCDebug(KWIN_VULKAN) << "  - Image view:" << m_texture->imageView();
            qCDebug(KWIN_VULKAN) << "  - Texture size:" << m_texture->size();
            qCDebug(KWIN_VULKAN) << "  - Pixmap size:" << m_size;

            // Check for size mismatches
            if (m_texture->size() != m_size) {
                qCWarning(KWIN_VULKAN) << "  - POTENTIAL ISSUE: Texture size doesn't match pixmap size";
                qCWarning(KWIN_VULKAN) << "    * Expected size:" << m_size;
                qCWarning(KWIN_VULKAN) << "    * Actual size:" << m_texture->size();
                qCWarning(KWIN_VULKAN) << "    * Size ratio (width):" << (m_texture->size().width() > 0 ? static_cast<float>(m_size.width()) / m_texture->size().width() : 0);
                qCWarning(KWIN_VULKAN) << "    * Size ratio (height):" << (m_texture->size().height() > 0 ? static_cast<float>(m_size.height()) / m_texture->size().height() : 0);
            }

            // Log layout transition information
            qCDebug(KWIN_VULKAN) << "  - Layout transition info:";

            // Issue external memory acquire barrier for DMA-BUF synchronization
            // This ensures the GPU sees the updated content from the X server
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = m_texture->currentLayout();
            barrier.newLayout = m_texture->currentLayout(); // Keep the same layout
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = m_texture->image();
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            // Use memory read bit for external memory synchronization
            // This tells the driver that new data may be available from external source
            barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

            VkCommandBuffer cmd = m_context->beginSingleTimeCommands();
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0,
                                 0, nullptr,
                                 0, nullptr,
                                 1, &barrier);
            m_context->endSingleTimeCommands(cmd);

            qCDebug(KWIN_VULKAN) << "  - Memory synchronization:";
            qCDebug(KWIN_VULKAN) << "    * Issued external memory acquire barrier";
            qCDebug(KWIN_VULKAN) << "    * External memory type:" << (m_texture->ownsImage() ? "Owned by texture" : "External memory");
        }

        return;
    }

    // CPU upload path
    updateWithCpuUpload(region);
}

void VulkanSurfaceTextureX11::updateWithCpuUpload(const QRegion &region)
{
    if (!m_texture || !m_stagingBuffer) {
        qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - texture or staging buffer not available";
        return;
    }

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

    // Log detailed information about the buffer-to-image copy operation
    qCDebug(KWIN_VULKAN) << "Buffer-to-image copy details:";
    qCDebug(KWIN_VULKAN) << "  - Buffer row length (pixels):" << copyRegion.bufferRowLength;
    qCDebug(KWIN_VULKAN) << "  - Buffer image height:" << copyRegion.bufferImageHeight;
    qCDebug(KWIN_VULKAN) << "  - Update region:" << QRect(x, y, width, height);
    qCDebug(KWIN_VULKAN) << "  - Texture size:" << m_size;

    if (x == 0 && y == 0 && width == m_size.width() && height == m_size.height()) {
        // Full copy
        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {static_cast<uint32_t>(m_size.width()),
                                  static_cast<uint32_t>(m_size.height()), 1};
        qCDebug(KWIN_VULKAN) << "  - Copy type: Full texture update";
    } else {
        // Partial copy - buffer data is at the same offset we wrote to in staging buffer
        // The staging buffer has full texture stride, which we specified in bufferRowLength
        copyRegion.bufferOffset = (y * m_size.width() + x) * 4;
        copyRegion.imageOffset = {x, y, 0};
        copyRegion.imageExtent = {static_cast<uint32_t>(width),
                                  static_cast<uint32_t>(height), 1};
        qCDebug(KWIN_VULKAN) << "  - Copy type: Partial texture update";
        qCDebug(KWIN_VULKAN) << "  - Buffer offset:" << copyRegion.bufferOffset;
        qCDebug(KWIN_VULKAN) << "  - Image offset:" << copyRegion.imageOffset.x << "," << copyRegion.imageOffset.y;
        qCDebug(KWIN_VULKAN) << "  - Image extent:" << copyRegion.imageExtent.width << "x" << copyRegion.imageExtent.height;
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

    qCDebug(KWIN_VULKAN) << "VulkanSurfaceTextureX11::updateWithCpuUpload() - image layout transition recorded";

    m_context->endSingleTimeCommands(cmd);
}

} // namespace KWin
