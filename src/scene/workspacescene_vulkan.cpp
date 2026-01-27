/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "config-kwin.h"

#if HAVE_VULKAN

#include "compositor.h"
#include "core/rendertarget.h"
#include "decorations/decoratedwindow.h"
#include "platformsupport/scenes/vulkan/vulkancontext.h"
#include "scene/itemrenderer_vulkan.h"
#include "shadow.h"
#include "utils/common.h"
#include "window.h"
#include "workspacescene_vulkan.h"

#include <QPainter>
#include <cmath>

namespace KWin
{

WorkspaceSceneVulkan::WorkspaceSceneVulkan(VulkanBackend *backend)
    : WorkspaceScene(std::make_unique<ItemRendererVulkan>(backend))
    , m_backend(backend)
{
}

WorkspaceSceneVulkan::~WorkspaceSceneVulkan()
{
}

std::unique_ptr<DecorationRenderer> WorkspaceSceneVulkan::createDecorationRenderer(Decoration::DecoratedWindowImpl *impl)
{
    return std::make_unique<SceneVulkanDecorationRenderer>(impl);
}

std::unique_ptr<ShadowTextureProvider> WorkspaceSceneVulkan::createShadowTextureProvider(Shadow *shadow)
{
    return std::make_unique<VulkanShadowTextureProvider>(shadow);
}

//-----------------------------------------------------------------------------
// VulkanShadowTextureProvider
//-----------------------------------------------------------------------------

VulkanShadowTextureProvider::VulkanShadowTextureProvider(Shadow *shadow)
    : ShadowTextureProvider(shadow)
{
}

VulkanShadowTextureProvider::~VulkanShadowTextureProvider()
{
    m_texture.reset();
}

void VulkanShadowTextureProvider::update()
{
    if (m_shadow->hasDecorationShadow()) {
        // Use the decoration shadow image directly
        QImage shadowImage = m_shadow->decorationShadowImage();
        if (shadowImage.isNull()) {
            return;
        }

        auto *backend = static_cast<WorkspaceSceneVulkan *>(Compositor::self()->scene())->backend();
        auto *context = backend->vulkanContext();
        if (!context) {
            return;
        }

        m_texture = VulkanTexture::upload(context, shadowImage);
        if (m_texture) {
            m_texture->setFilter(VK_FILTER_LINEAR);
            m_texture->setWrapMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        }
        return;
    }

    // Build the shadow from individual elements
    const QSize top(m_shadow->shadowElement(Shadow::ShadowElementTop).size());
    const QSize topRight(m_shadow->shadowElement(Shadow::ShadowElementTopRight).size());
    const QSize right(m_shadow->shadowElement(Shadow::ShadowElementRight).size());
    const QSize bottom(m_shadow->shadowElement(Shadow::ShadowElementBottom).size());
    const QSize bottomLeft(m_shadow->shadowElement(Shadow::ShadowElementBottomLeft).size());
    const QSize left(m_shadow->shadowElement(Shadow::ShadowElementLeft).size());
    const QSize topLeft(m_shadow->shadowElement(Shadow::ShadowElementTopLeft).size());
    const QSize bottomRight(m_shadow->shadowElement(Shadow::ShadowElementBottomRight).size());

    const int width = std::max({topLeft.width(), left.width(), bottomLeft.width()}) + std::max(top.width(), bottom.width()) + std::max({topRight.width(), right.width(), bottomRight.width()});
    const int height = std::max({topLeft.height(), top.height(), topRight.height()}) + std::max(left.height(), right.height()) + std::max({bottomLeft.height(), bottom.height(), bottomRight.height()});

    if (width == 0 || height == 0) {
        return;
    }

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const int innerRectTop = std::max({topLeft.height(), top.height(), topRight.height()});
    const int innerRectLeft = std::max({topLeft.width(), left.width(), bottomLeft.width()});

    QPainter p;
    p.begin(&image);

    p.drawImage(QRectF(0, 0, topLeft.width(), topLeft.height()),
                m_shadow->shadowElement(Shadow::ShadowElementTopLeft));
    p.drawImage(QRectF(innerRectLeft, 0, top.width(), top.height()),
                m_shadow->shadowElement(Shadow::ShadowElementTop));
    p.drawImage(QRectF(width - topRight.width(), 0, topRight.width(), topRight.height()),
                m_shadow->shadowElement(Shadow::ShadowElementTopRight));

    p.drawImage(QRectF(0, innerRectTop, left.width(), left.height()),
                m_shadow->shadowElement(Shadow::ShadowElementLeft));
    p.drawImage(QRectF(width - right.width(), innerRectTop, right.width(), right.height()),
                m_shadow->shadowElement(Shadow::ShadowElementRight));

    p.drawImage(QRectF(0, height - bottomLeft.height(), bottomLeft.width(), bottomLeft.height()),
                m_shadow->shadowElement(Shadow::ShadowElementBottomLeft));
    p.drawImage(QRectF(innerRectLeft, height - bottom.height(), bottom.width(), bottom.height()),
                m_shadow->shadowElement(Shadow::ShadowElementBottom));
    p.drawImage(QRectF(width - bottomRight.width(), height - bottomRight.height(),
                       bottomRight.width(), bottomRight.height()),
                m_shadow->shadowElement(Shadow::ShadowElementBottomRight));

    p.end();

    auto *backend = static_cast<WorkspaceSceneVulkan *>(Compositor::self()->scene())->backend();
    auto *context = backend->vulkanContext();
    if (!context) {
        return;
    }

    m_texture = VulkanTexture::upload(context, image);
    if (m_texture) {
        m_texture->setFilter(VK_FILTER_LINEAR);
        m_texture->setWrapMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    }
}

//-----------------------------------------------------------------------------
// SceneVulkanDecorationRenderer
//-----------------------------------------------------------------------------

static void clamp_row(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::copy(src, src + width, dest + left);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp_sides(int left, int width, int right, const uint32_t *src, uint32_t *dest)
{
    std::fill_n(dest, left, *src);
    std::fill_n(dest + left + width, right, *(src + width - 1));
}

static void clamp(QImage &image, const QRect &viewport)
{
    Q_ASSERT(image.depth() == 32);
    if (viewport.isEmpty()) {
        image = {};
        return;
    }

    const QRect rect = image.rect();

    const int left = viewport.left() - rect.left();
    const int top = viewport.top() - rect.top();
    const int right = rect.right() - viewport.right();
    const int bottom = rect.bottom() - viewport.bottom();

    const int width = rect.width() - left - right;
    const int height = rect.height() - top - bottom;

    const uint32_t *firstRow = reinterpret_cast<uint32_t *>(image.scanLine(top));
    const uint32_t *lastRow = reinterpret_cast<uint32_t *>(image.scanLine(top + height - 1));

    for (int i = 0; i < top; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(i));
        clamp_row(left, width, right, firstRow + left, dest);
    }

    for (int i = 0; i < height; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + i));
        clamp_sides(left, width, right, dest + left, dest);
    }

    for (int i = 0; i < bottom; ++i) {
        uint32_t *dest = reinterpret_cast<uint32_t *>(image.scanLine(top + height + i));
        clamp_row(left, width, right, lastRow + left, dest);
    }
}

static int align(int value, int alignTo)
{
    return (value + alignTo - 1) & ~(alignTo - 1);
}

SceneVulkanDecorationRenderer::SceneVulkanDecorationRenderer(Decoration::DecoratedWindowImpl *client)
    : DecorationRenderer(client)
{
}

SceneVulkanDecorationRenderer::~SceneVulkanDecorationRenderer()
{
    m_texture.reset();
}

void SceneVulkanDecorationRenderer::render(const QRegion &region)
{
    if (areImageSizesDirty()) {
        resizeTexture();
        resetImageSizesDirty();
    }

    if (!m_texture) {
        return;
    }

    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);

    const qreal devicePixelRatio = effectiveDevicePixelRatio();
    const int topHeight = std::round(top.height() * devicePixelRatio);
    const int bottomHeight = std::round(bottom.height() * devicePixelRatio);
    const int leftWidth = std::round(left.width() * devicePixelRatio);

    const QPoint topPosition(0, 0);
    const QPoint bottomPosition(0, topPosition.y() + topHeight + (2 * TexturePad));
    const QPoint leftPosition(0, bottomPosition.y() + bottomHeight + (2 * TexturePad));
    const QPoint rightPosition(0, leftPosition.y() + leftWidth + (2 * TexturePad));

    const QRect dirtyRect = region.boundingRect();

    renderPart(top.intersected(dirtyRect), top, topPosition, devicePixelRatio);
    renderPart(bottom.intersected(dirtyRect), bottom, bottomPosition, devicePixelRatio);
    renderPart(left.intersected(dirtyRect), left, leftPosition, devicePixelRatio, true);
    renderPart(right.intersected(dirtyRect), right, rightPosition, devicePixelRatio, true);
}

void SceneVulkanDecorationRenderer::renderPart(const QRectF &rect, const QRectF &partRect,
                                               const QPoint &textureOffset,
                                               qreal devicePixelRatio, bool rotated)
{
    if (!rect.isValid() || !m_texture) {
        return;
    }

    const QMargins padding = texturePadForPart(rect, partRect);
    int verticalPadding = padding.top() + padding.bottom();
    int horizontalPadding = padding.left() + padding.right();

    QSize imageSize(toNativeSize(rect.width()), toNativeSize(rect.height()));
    if (rotated) {
        imageSize = QSize(imageSize.height(), imageSize.width());
    }
    QSize paddedImageSize = imageSize;
    paddedImageSize.rheight() += verticalPadding;
    paddedImageSize.rwidth() += horizontalPadding;

    QImage image(paddedImageSize, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::transparent);

    QRect padClip = QRect(padding.left(), padding.top(), imageSize.width(), imageSize.height());
    QPainter painter(&image);
    const qreal inverseScale = 1.0 / devicePixelRatio;
    painter.scale(inverseScale, inverseScale);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(padClip);
    painter.translate(padding.left(), padding.top());
    if (rotated) {
        painter.translate(0, imageSize.height());
        painter.rotate(-90);
    }
    painter.scale(devicePixelRatio, devicePixelRatio);
    painter.translate(-rect.topLeft());
    renderToPainter(&painter, rect);
    painter.end();

    // Fill padding pixels by copying from the neighbour row
    clamp(image, padClip);

    QPoint dirtyOffset = ((rect.topLeft() - partRect.topLeft()) * devicePixelRatio).toPoint();
    if (padding.top() == 0) {
        dirtyOffset.ry() += TexturePad;
    }
    if (padding.left() == 0) {
        dirtyOffset.rx() += TexturePad;
    }

    // Update the texture with the rendered part
    m_texture->update(image, textureOffset + dirtyOffset);
}

const QMargins SceneVulkanDecorationRenderer::texturePadForPart(const QRectF &rect, const QRectF &partRect)
{
    QMargins result = QMargins(0, 0, 0, 0);
    if (rect.top() == partRect.top()) {
        result.setTop(TexturePad);
    }
    if (rect.bottom() == partRect.bottom()) {
        result.setBottom(TexturePad);
    }
    if (rect.left() == partRect.left()) {
        result.setLeft(TexturePad);
    }
    if (rect.right() == partRect.right()) {
        result.setRight(TexturePad);
    }
    return result;
}

void SceneVulkanDecorationRenderer::resizeTexture()
{
    QRectF left, top, right, bottom;
    client()->window()->layoutDecorationRects(left, top, right, bottom);
    QSize size;

    size.rwidth() = toNativeSize(std::max({top.width(), bottom.width(), left.height(), right.height()}));
    size.rheight() = toNativeSize(top.height()) + toNativeSize(bottom.height()) + toNativeSize(left.width()) + toNativeSize(right.width());

    size.rheight() += 4 * (2 * TexturePad);
    size.rwidth() += 2 * TexturePad;
    size.rwidth() = align(size.width(), 128);

    if (m_texture && m_texture->size() == size) {
        return;
    }

    if (!size.isEmpty()) {
        auto *backend = static_cast<WorkspaceSceneVulkan *>(Compositor::self()->scene())->backend();
        auto *context = backend->vulkanContext();
        if (!context) {
            return;
        }

        m_texture = VulkanTexture::allocate(context, size, VK_FORMAT_R8G8B8A8_UNORM);
        if (m_texture) {
            m_texture->setContentTransform(OutputTransform::FlipY);
            m_texture->setFilter(VK_FILTER_LINEAR);
            m_texture->setWrapMode(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        }
    } else {
        m_texture.reset();
    }
}

int SceneVulkanDecorationRenderer::toNativeSize(double size) const
{
    return std::round(size * effectiveDevicePixelRatio());
}

} // namespace KWin

#include "moc_workspacescene_vulkan.cpp"

#endif // HAVE_VULKAN
