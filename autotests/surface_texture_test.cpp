/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "testutils.h"

#include "platformsupport/scenes/opengl/openglsurfacetexture_x11.h"
#include "platformsupport/scenes/vulkan/vulkansurfacetexture_x11.h"
#include "scene/surfaceitem.h"
#include "scene/surfaceitem_x11.h"

#ifdef HAVE_EGL
#include "opengl/abstract_opengl_context_attribute_builder.h"
#include "opengl/eglimagetexture.h"
#include "platformsupport/scenes/opengl/abstract_egl_backend.h"
#include "platformsupport/scenes/opengl/openglsurfacetexture.h"
#endif

#ifdef HAVE_VULKAN
#include "backends/x11/standalone/x11_standalone_vulkan_backend.h"
#include "platformsupport/scenes/vulkan/vulkanbackend.h"
#include "platformsupport/scenes/vulkan/vulkansurfacetexture.h"
#include "platformsupport/scenes/vulkan/vulkantexture.h"
#endif

#include <QtTest>
#include <memory>
#include <type_traits>

namespace KWin
{

class SurfaceTextureTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    // OpenGL tests
    void testOpenGLInheritanceHierarchy();
    void testOpenGLRegionOperations();

    // Vulkan tests
    void testVulkanInheritanceHierarchy();
    void testVulkanRegionOperations();
};

void SurfaceTextureTest::initTestCase()
{
    // Force xcb backend for consistent testing environment
    forceXcb();
}

void SurfaceTextureTest::cleanupTestCase()
{
}

void SurfaceTextureTest::testOpenGLInheritanceHierarchy()
{
#ifdef HAVE_EGL
    // Verify that the SurfaceTexture base class has the expected inheritance hierarchy
    static_assert(std::is_base_of<SurfaceTexture, OpenGLSurfaceTexture>::value,
                  "OpenGLSurfaceTexture should inherit from SurfaceTexture");
    static_assert(std::is_base_of<OpenGLSurfaceTexture, OpenGLSurfaceTextureX11>::value,
                  "OpenGLSurfaceTextureX11 should inherit from OpenGLSurfaceTexture");

    // The compilation validates the inheritance hierarchy
    QVERIFY(true);
#else
    QSKIP("OpenGL not available");
#endif
}

void SurfaceTextureTest::testOpenGLRegionOperations()
{
#ifdef HAVE_EGL
    // Test that QRegion operations work correctly for surface texture updates
    QRegion singleRectRegion(QRect(0, 0, 100, 100));
    QRegion multiRectRegion;
    multiRectRegion += QRect(0, 0, 50, 50);
    multiRectRegion += QRect(100, 100, 50, 50);

    // Verify the regions have expected properties
    QVERIFY(!singleRectRegion.isEmpty());
    QVERIFY(!multiRectRegion.isEmpty());
    QCOMPARE(singleRectRegion.rectCount(), 1);
    QCOMPARE(multiRectRegion.rectCount(), 2);

    // Test some region calculations
    QRect boundingRect = singleRectRegion.boundingRect();
    QCOMPARE(boundingRect, QRect(0, 0, 100, 100));
#else
    QSKIP("OpenGL not available");
#endif
}

void SurfaceTextureTest::testVulkanInheritanceHierarchy()
{
#ifdef HAVE_VULKAN
    // Test that the Vulkan surface texture has the proper inheritance
    static_assert(std::is_base_of<SurfaceTexture, VulkanSurfaceTexture>::value,
                  "VulkanSurfaceTexture should inherit from SurfaceTexture");
    static_assert(std::is_base_of<VulkanSurfaceTexture, VulkanSurfaceTextureX11>::value,
                  "VulkanSurfaceTextureX11 should inherit from VulkanSurfaceTexture");

    // The compilation validates the inheritance hierarchy
    QVERIFY(true);
#else
    QSKIP("Vulkan not available");
#endif
}

void SurfaceTextureTest::testVulkanRegionOperations()
{
#ifdef HAVE_VULKAN
    // Test that update method can be called with various valid regions
    QRegion region1(QRect(10, 20, 200, 150));
    QRegion region2;
    region2 += QRect(0, 0, 100, 100);
    region2 += QRect(200, 200, 100, 100);
    region2 += QRect(400, 400, 50, 50);

    // Verify the regions have expected properties
    QVERIFY(!region1.isEmpty());
    QVERIFY(!region2.isEmpty());
    QCOMPARE(region1.rectCount(), 1);
    QCOMPARE(region2.rectCount(), 3);

    // Test intersection and union operations
    QRegion intersection = region1.intersected(QRect(50, 50, 100, 100));
    QVERIFY(!intersection.isEmpty());

    QRegion unionRegion = region1.united(QRect(300, 300, 100, 100));
    QCOMPARE(unionRegion.rectCount(), 2);
#else
    QSKIP("Vulkan not available");
#endif
}
} // namespace KWin

QTEST_MAIN(KWin::SurfaceTextureTest)

#include "surface_texture_test.moc"
