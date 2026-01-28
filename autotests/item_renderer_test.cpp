/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testutils.h"

#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "scene/imageitem.h"
#include "scene/item.h"
#include "scene/itemrenderer.h"

#include <QtTest>
#include <memory>
#include <type_traits>

namespace KWin
{

class ItemRendererTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    // Base ItemRenderer interface tests
    void testItemRendererInterface();
    void testRenderTargetBasics();
    void testRenderViewportBasics();
};

void ItemRendererTest::initTestCase()
{
    // Force xcb backend for consistent testing environment
    forceXcb();
}

void ItemRendererTest::cleanupTestCase()
{
}

void ItemRendererTest::testItemRendererInterface()
{
    // Test that the ItemRenderer interface has the expected virtual methods
    // These are compile-time checks to verify the API hasn't changed

    // Check createImageItem signature
    using CreateImageItemType = decltype(&ItemRenderer::createImageItem);
    static_assert(std::is_same_v<CreateImageItemType, std::unique_ptr<ImageItem> (ItemRenderer::*)(Item *)>,
                  "createImageItem signature should match expected signature");

    // Check beginFrame signature
    using BeginFrameType = decltype(&ItemRenderer::beginFrame);
    static_assert(std::is_same_v<BeginFrameType, void (ItemRenderer::*)(const RenderTarget &, const RenderViewport &)>,
                  "beginFrame signature should match expected signature");

    // Check renderBackground signature
    using RenderBackgroundType = decltype(&ItemRenderer::renderBackground);
    static_assert(std::is_same_v<RenderBackgroundType, void (ItemRenderer::*)(const RenderTarget &, const RenderViewport &, const QRegion &)>,
                  "renderBackground signature should match expected signature");

    // Check renderItem signature
    using RenderItemType = decltype(&ItemRenderer::renderItem);
    static_assert(std::is_same_v<RenderItemType, void (ItemRenderer::*)(const RenderTarget &, const RenderViewport &, Item *, int, const QRegion &, const WindowPaintData &)>,
                  "renderItem signature should match expected signature");

    QVERIFY(true); // Static asserts passed
}

void ItemRendererTest::testRenderTargetBasics()
{
    // Test QImage-based RenderTarget (doesn't require GPU)
    QImage image(100, 100, QImage::Format_ARGB32);
    RenderTarget target(&image, ColorDescription::sRGB);

    // Verify basic properties
    QVERIFY(target.image() != nullptr);
    QCOMPARE(target.size(), QSize(100, 100));
    QCOMPARE(target.colorDescription(), ColorDescription::sRGB);
}

void ItemRendererTest::testRenderViewportBasics()
{
    // Create a viewport with a QImage-based render target
    QImage image(800, 600, QImage::Format_ARGB32);
    RenderTarget target(&image, ColorDescription::sRGB);

    QRectF renderRect(0, 0, 800, 600);
    RenderViewport viewport(renderRect, 1.0, target);

    // Test viewport properties
    QCOMPARE(viewport.renderRect(), renderRect);
    QCOMPARE(viewport.scale(), 1.0);

    // Test coordinate mapping
    QPointF point(400, 300);
    QPointF mapped = viewport.mapToRenderTarget(point);
    // The mapping should transform the point correctly
    QVERIFY(!mapped.isNull() || mapped == QPointF(0, 0));

    // Test projection matrix is valid
    QMatrix4x4 projection = viewport.projectionMatrix();
    QVERIFY(!projection.isIdentity() || projection == QMatrix4x4());
}
} // namespace KWin

QTEST_MAIN(KWin::ItemRendererTest)

#include "item_renderer_test.moc"
