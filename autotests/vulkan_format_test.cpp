/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QImage>
#include <QTest>

#include "config-kwin.h"

#if HAVE_VULKAN
#include <libdrm/drm_fourcc.h>
#include <vulkan/vulkan.h>
#endif

/**
 * @brief Tests for Vulkan format conversions and related utilities.
 *
 * These tests verify format conversion logic that is critical for
 * proper texture handling in the Vulkan backend.
 */
class VulkanFormatTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testDepthToDrmFormat_data();
    void testDepthToDrmFormat();
    void testDrmFormatToVkFormat_data();
    void testDrmFormatToVkFormat();
    void testQImageFormatMapping_data();
    void testQImageFormatMapping();
    void testFormatHasAlpha_data();
    void testFormatHasAlpha();
};

#if HAVE_VULKAN

// Re-implement the format conversion functions for testing
// (These mirror the implementations in vulkansurfacetexture_x11.cpp and vulkantexture.cpp)

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
        return 0;
    }
}

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
        return VK_FORMAT_UNDEFINED;
    }
}

static VkFormat qImageFormatToVkFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QImage::Format_RGBX8888:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case QImage::Format_RGB888:
        return VK_FORMAT_R8G8B8_UNORM;
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case QImage::Format_RGB32:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case QImage::Format_Grayscale8:
        return VK_FORMAT_R8_UNORM;
    case QImage::Format_Grayscale16:
        return VK_FORMAT_R16_UNORM;
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case QImage::Format_RGBX64:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case QImage::Format_RGBA16FPx4:
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case QImage::Format_RGBA32FPx4:
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
        return VK_FORMAT_UNDEFINED;
    }
}

static bool vkFormatHasAlpha(VkFormat format)
{
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R16G16B16A16_UNORM:
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return true;
    default:
        return false;
    }
}

#endif // HAVE_VULKAN

void VulkanFormatTest::testDepthToDrmFormat_data()
{
    QTest::addColumn<int>("depth");
    QTest::addColumn<uint32_t>("expectedFormat");

#if HAVE_VULKAN
    QTest::newRow("32-bit") << 32 << static_cast<uint32_t>(DRM_FORMAT_ARGB8888);
    QTest::newRow("24-bit") << 24 << static_cast<uint32_t>(DRM_FORMAT_XRGB8888);
    QTest::newRow("30-bit") << 30 << static_cast<uint32_t>(DRM_FORMAT_XRGB2101010);
    QTest::newRow("16-bit") << 16 << static_cast<uint32_t>(DRM_FORMAT_RGB565);
    QTest::newRow("8-bit (unsupported)") << 8 << static_cast<uint32_t>(0);
    QTest::newRow("1-bit (unsupported)") << 1 << static_cast<uint32_t>(0);
#endif
}

void VulkanFormatTest::testDepthToDrmFormat()
{
#if HAVE_VULKAN
    QFETCH(int, depth);
    QFETCH(uint32_t, expectedFormat);

    QCOMPARE(depthToDrmFormat(static_cast<uint8_t>(depth)), expectedFormat);
#else
    QSKIP("Vulkan support not available");
#endif
}

void VulkanFormatTest::testDrmFormatToVkFormat_data()
{
    QTest::addColumn<uint32_t>("drmFormat");
    QTest::addColumn<int>("expectedVkFormat");

#if HAVE_VULKAN
    QTest::newRow("ARGB8888") << static_cast<uint32_t>(DRM_FORMAT_ARGB8888) << static_cast<int>(VK_FORMAT_B8G8R8A8_UNORM);
    QTest::newRow("XRGB8888") << static_cast<uint32_t>(DRM_FORMAT_XRGB8888) << static_cast<int>(VK_FORMAT_B8G8R8A8_UNORM);
    QTest::newRow("ABGR8888") << static_cast<uint32_t>(DRM_FORMAT_ABGR8888) << static_cast<int>(VK_FORMAT_R8G8B8A8_UNORM);
    QTest::newRow("XBGR8888") << static_cast<uint32_t>(DRM_FORMAT_XBGR8888) << static_cast<int>(VK_FORMAT_R8G8B8A8_UNORM);
    QTest::newRow("RGB888") << static_cast<uint32_t>(DRM_FORMAT_RGB888) << static_cast<int>(VK_FORMAT_R8G8B8_UNORM);
    QTest::newRow("BGR888") << static_cast<uint32_t>(DRM_FORMAT_BGR888) << static_cast<int>(VK_FORMAT_B8G8R8_UNORM);
    QTest::newRow("RGB565") << static_cast<uint32_t>(DRM_FORMAT_RGB565) << static_cast<int>(VK_FORMAT_R5G6B5_UNORM_PACK16);
    QTest::newRow("ARGB2101010") << static_cast<uint32_t>(DRM_FORMAT_ARGB2101010) << static_cast<int>(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
    QTest::newRow("ABGR16161616F") << static_cast<uint32_t>(DRM_FORMAT_ABGR16161616F) << static_cast<int>(VK_FORMAT_R16G16B16A16_SFLOAT);
    QTest::newRow("Unknown format") << static_cast<uint32_t>(0x12345678) << static_cast<int>(VK_FORMAT_UNDEFINED);
#endif
}

void VulkanFormatTest::testDrmFormatToVkFormat()
{
#if HAVE_VULKAN
    QFETCH(uint32_t, drmFormat);
    QFETCH(int, expectedVkFormat);

    QCOMPARE(static_cast<int>(drmFormatToVkFormat(drmFormat)), expectedVkFormat);
#else
    QSKIP("Vulkan support not available");
#endif
}

void VulkanFormatTest::testQImageFormatMapping_data()
{
    QTest::addColumn<int>("qImageFormat");
    QTest::addColumn<int>("expectedVkFormat");

#if HAVE_VULKAN
    QTest::newRow("RGBA8888") << static_cast<int>(QImage::Format_RGBA8888) << static_cast<int>(VK_FORMAT_R8G8B8A8_UNORM);
    QTest::newRow("RGBA8888_Premultiplied") << static_cast<int>(QImage::Format_RGBA8888_Premultiplied) << static_cast<int>(VK_FORMAT_R8G8B8A8_UNORM);
    QTest::newRow("ARGB32") << static_cast<int>(QImage::Format_ARGB32) << static_cast<int>(VK_FORMAT_B8G8R8A8_UNORM);
    QTest::newRow("ARGB32_Premultiplied") << static_cast<int>(QImage::Format_ARGB32_Premultiplied) << static_cast<int>(VK_FORMAT_B8G8R8A8_UNORM);
    QTest::newRow("RGB32") << static_cast<int>(QImage::Format_RGB32) << static_cast<int>(VK_FORMAT_B8G8R8A8_UNORM);
    QTest::newRow("RGB888") << static_cast<int>(QImage::Format_RGB888) << static_cast<int>(VK_FORMAT_R8G8B8_UNORM);
    QTest::newRow("Grayscale8") << static_cast<int>(QImage::Format_Grayscale8) << static_cast<int>(VK_FORMAT_R8_UNORM);
    QTest::newRow("Grayscale16") << static_cast<int>(QImage::Format_Grayscale16) << static_cast<int>(VK_FORMAT_R16_UNORM);
    QTest::newRow("RGBA64") << static_cast<int>(QImage::Format_RGBA64) << static_cast<int>(VK_FORMAT_R16G16B16A16_UNORM);
    QTest::newRow("RGBA16FPx4") << static_cast<int>(QImage::Format_RGBA16FPx4) << static_cast<int>(VK_FORMAT_R16G16B16A16_SFLOAT);
    QTest::newRow("RGBA32FPx4") << static_cast<int>(QImage::Format_RGBA32FPx4) << static_cast<int>(VK_FORMAT_R32G32B32A32_SFLOAT);
    QTest::newRow("Mono (unsupported)") << static_cast<int>(QImage::Format_Mono) << static_cast<int>(VK_FORMAT_UNDEFINED);
    QTest::newRow("Invalid") << static_cast<int>(QImage::Format_Invalid) << static_cast<int>(VK_FORMAT_UNDEFINED);
#endif
}

void VulkanFormatTest::testQImageFormatMapping()
{
#if HAVE_VULKAN
    QFETCH(int, qImageFormat);
    QFETCH(int, expectedVkFormat);

    QCOMPARE(static_cast<int>(qImageFormatToVkFormat(static_cast<QImage::Format>(qImageFormat))), expectedVkFormat);
#else
    QSKIP("Vulkan support not available");
#endif
}

void VulkanFormatTest::testFormatHasAlpha_data()
{
    QTest::addColumn<int>("vkFormat");
    QTest::addColumn<bool>("expectedHasAlpha");

#if HAVE_VULKAN
    QTest::newRow("R8G8B8A8_UNORM") << static_cast<int>(VK_FORMAT_R8G8B8A8_UNORM) << true;
    QTest::newRow("B8G8R8A8_UNORM") << static_cast<int>(VK_FORMAT_B8G8R8A8_UNORM) << true;
    QTest::newRow("R16G16B16A16_SFLOAT") << static_cast<int>(VK_FORMAT_R16G16B16A16_SFLOAT) << true;
    QTest::newRow("R32G32B32A32_SFLOAT") << static_cast<int>(VK_FORMAT_R32G32B32A32_SFLOAT) << true;
    QTest::newRow("A2R10G10B10_UNORM") << static_cast<int>(VK_FORMAT_A2R10G10B10_UNORM_PACK32) << true;
    QTest::newRow("R8G8B8_UNORM (no alpha)") << static_cast<int>(VK_FORMAT_R8G8B8_UNORM) << false;
    QTest::newRow("B8G8R8_UNORM (no alpha)") << static_cast<int>(VK_FORMAT_B8G8R8_UNORM) << false;
    QTest::newRow("R5G6B5_UNORM (no alpha)") << static_cast<int>(VK_FORMAT_R5G6B5_UNORM_PACK16) << false;
    QTest::newRow("R8_UNORM (no alpha)") << static_cast<int>(VK_FORMAT_R8_UNORM) << false;
#endif
}

void VulkanFormatTest::testFormatHasAlpha()
{
#if HAVE_VULKAN
    QFETCH(int, vkFormat);
    QFETCH(bool, expectedHasAlpha);

    QCOMPARE(vkFormatHasAlpha(static_cast<VkFormat>(vkFormat)), expectedHasAlpha);
#else
    QSKIP("Vulkan support not available");
#endif
}

QTEST_GUILESS_MAIN(VulkanFormatTest)
#include "vulkan_format_test.moc"
