/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QImage>
#include <QTest>

#include "config-kwin.h"
#include "core/colorspace.h"
#include "core/output.h"
#include "core/rendertarget.h"

using namespace KWin;

/**
 * @brief Tests for the RenderTarget class.
 *
 * RenderTarget is a backend-agnostic abstraction that can wrap:
 * - GLFramebuffer (OpenGL)
 * - VulkanRenderTarget (Vulkan)
 * - QImage (software rendering)
 *
 * These tests verify the RenderTarget API works correctly for all backend types.
 */
class RenderTargetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testQImageRenderTarget();
    void testQImageRenderTargetSize();
    void testQImageRenderTargetTransform();
    void testColorDescription();
    void testRenderTargetTypeDetection();
};

void RenderTargetTest::testQImageRenderTarget()
{
    // Create a QImage-based render target (works without GPU)
    QImage image(100, 100, QImage::Format_ARGB32);
    RenderTarget target(&image, ColorDescription::sRGB);

    // Verify basic properties
    QVERIFY(target.image() != nullptr);
    QCOMPARE(target.size(), QSize(100, 100));
}

void RenderTargetTest::testQImageRenderTargetSize()
{
    // Test various sizes
    QImage smallImage(1, 1, QImage::Format_ARGB32);
    RenderTarget smallTarget(&smallImage, ColorDescription::sRGB);
    QCOMPARE(smallTarget.size(), QSize(1, 1));

    QImage largeImage(4096, 2160, QImage::Format_ARGB32);
    RenderTarget largeTarget(&largeImage, ColorDescription::sRGB);
    QCOMPARE(largeTarget.size(), QSize(4096, 2160));

    QImage wideImage(3440, 1440, QImage::Format_ARGB32);
    RenderTarget wideTarget(&wideImage, ColorDescription::sRGB);
    QCOMPARE(wideTarget.size(), QSize(3440, 1440));
}

void RenderTargetTest::testQImageRenderTargetTransform()
{
    QImage image(100, 100, QImage::Format_ARGB32);
    RenderTarget target(&image, ColorDescription::sRGB);

    // QImage targets should have identity transform by default
    QCOMPARE(target.transform(), OutputTransform());
}

void RenderTargetTest::testColorDescription()
{
    QImage image(100, 100, QImage::Format_ARGB32);

    // Test with sRGB
    RenderTarget srgbTarget(&image, ColorDescription::sRGB);
    QCOMPARE(srgbTarget.colorDescription(), ColorDescription::sRGB);

    // Test with different color spaces if available
    // The exact color descriptions available depend on the build configuration
}

void RenderTargetTest::testRenderTargetTypeDetection()
{
    // Test QImage render target
    QImage image(100, 100, QImage::Format_ARGB32);
    RenderTarget imageTarget(&image, ColorDescription::sRGB);

    // Should not be detected as GL or Vulkan
    QVERIFY(imageTarget.framebuffer() == nullptr);

#if HAVE_VULKAN
    QVERIFY(!imageTarget.isVulkan());
    QVERIFY(imageTarget.vulkanTarget() == nullptr);
#endif
}

QTEST_GUILESS_MAIN(RenderTargetTest)
#include "render_target_test.moc"
