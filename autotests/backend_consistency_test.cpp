/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QTest>
#include <memory>
#include <type_traits>

#include "config-kwin.h"

// OpenGL headers
#include "opengl/glframebuffer.h"
#include "opengl/gltexture.h"
#include "opengl/openglcontext.h"

#if HAVE_VULKAN
// Vulkan headers
#include "platformsupport/scenes/vulkan/vulkancontext.h"
#include "platformsupport/scenes/vulkan/vulkanframebuffer.h"
#include "platformsupport/scenes/vulkan/vulkantexture.h"
#endif

using namespace KWin;

/**
 * @brief Tests to verify OpenGL and Vulkan backends have consistent API patterns.
 *
 * These tests verify:
 * 1. Factory methods return the correct smart pointer types
 * 2. Accessor methods return consistent pointer types
 * 3. Both backends follow the same ownership patterns
 */
class BackendConsistencyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testTextureFactoryReturnTypes();
    void testFramebufferAccessorTypes();
    void testContextStaticStorage();
    void testContextFramebufferMethodSignatures();
};

void BackendConsistencyTest::testTextureFactoryReturnTypes()
{
    // Verify GLTexture factory methods return unique_ptr
    {
        using UploadReturnType = decltype(GLTexture::upload(std::declval<const QImage &>()));
        static_assert(std::is_same_v<UploadReturnType, std::unique_ptr<GLTexture>>,
                      "GLTexture::upload should return std::unique_ptr<GLTexture>");

        using AllocateReturnType = decltype(GLTexture::allocate(std::declval<GLenum>(), std::declval<const QSize &>(), 1));
        static_assert(std::is_same_v<AllocateReturnType, std::unique_ptr<GLTexture>>,
                      "GLTexture::allocate should return std::unique_ptr<GLTexture>");

        using WrapperReturnType = decltype(GLTexture::createNonOwningWrapper(0, 0, QSize()));
        static_assert(std::is_same_v<WrapperReturnType, std::unique_ptr<GLTexture>>,
                      "GLTexture::createNonOwningWrapper should return std::unique_ptr<GLTexture>");

        QVERIFY(true); // Static asserts passed
    }

#if HAVE_VULKAN
    // Verify VulkanTexture factory methods return unique_ptr (matching OpenGL pattern)
    {
        using UploadReturnType = decltype(VulkanTexture::upload(nullptr, std::declval<const QImage &>()));
        static_assert(std::is_same_v<UploadReturnType, std::unique_ptr<VulkanTexture>>,
                      "VulkanTexture::upload should return std::unique_ptr<VulkanTexture>");

        using AllocateReturnType = decltype(VulkanTexture::allocate(nullptr, std::declval<const QSize &>(), VK_FORMAT_UNDEFINED));
        static_assert(std::is_same_v<AllocateReturnType, std::unique_ptr<VulkanTexture>>,
                      "VulkanTexture::allocate should return std::unique_ptr<VulkanTexture>");

        using RenderTargetReturnType = decltype(VulkanTexture::createRenderTarget(nullptr, std::declval<const QSize &>(), VK_FORMAT_UNDEFINED));
        static_assert(std::is_same_v<RenderTargetReturnType, std::unique_ptr<VulkanTexture>>,
                      "VulkanTexture::createRenderTarget should return std::unique_ptr<VulkanTexture>");

        using DepthStencilReturnType = decltype(VulkanTexture::createDepthStencil(nullptr, std::declval<const QSize &>()));
        static_assert(std::is_same_v<DepthStencilReturnType, std::unique_ptr<VulkanTexture>>,
                      "VulkanTexture::createDepthStencil should return std::unique_ptr<VulkanTexture>");

        using WrapperReturnType = decltype(VulkanTexture::createNonOwningWrapper(nullptr, VK_NULL_HANDLE, VK_FORMAT_UNDEFINED, QSize()));
        static_assert(std::is_same_v<WrapperReturnType, std::unique_ptr<VulkanTexture>>,
                      "VulkanTexture::createNonOwningWrapper should return std::unique_ptr<VulkanTexture>");

        QVERIFY(true); // Static asserts passed
    }
#endif
}

void BackendConsistencyTest::testFramebufferAccessorTypes()
{
#if HAVE_VULKAN
    // Verify VulkanFramebuffer texture accessors return raw pointers (like OpenGL's colorAttachment())
    {
        using ColorTextureReturnType = decltype(std::declval<const VulkanFramebuffer>().colorTexture());
        static_assert(std::is_same_v<ColorTextureReturnType, VulkanTexture *>,
                      "VulkanFramebuffer::colorTexture should return VulkanTexture*");

        using DepthTextureReturnType = decltype(std::declval<const VulkanFramebuffer>().depthTexture());
        static_assert(std::is_same_v<DepthTextureReturnType, VulkanTexture *>,
                      "VulkanFramebuffer::depthTexture should return VulkanTexture*");

        QVERIFY(true); // Static asserts passed
    }
#endif
}

void BackendConsistencyTest::testContextStaticStorage()
{
    // Verify both contexts use non-thread-local static storage for currentContext
    // This is verified by the fact that OpenGlContext::s_currentContext is not thread_local
    // and VulkanContext::s_currentContext should match

    // We can't directly test static member storage class, but we can verify
    // the currentContext() static method exists and returns the correct type
    {
        using GLContextReturnType = decltype(OpenGlContext::currentContext());
        static_assert(std::is_same_v<GLContextReturnType, OpenGlContext *>,
                      "OpenGlContext::currentContext should return OpenGlContext*");
    }

#if HAVE_VULKAN
    {
        using VkContextReturnType = decltype(VulkanContext::currentContext());
        static_assert(std::is_same_v<VkContextReturnType, VulkanContext *>,
                      "VulkanContext::currentContext should return VulkanContext*");
    }
#endif

    QVERIFY(true); // Static asserts passed
}

void BackendConsistencyTest::testContextFramebufferMethodSignatures()
{
    // Verify both contexts have consistent framebuffer stack methods
    // OpenGL: GLFramebuffer *currentFramebuffer() - non-const
    // Vulkan should match: VulkanFramebuffer *currentFramebuffer() - non-const

    // Check OpenGL signature
    {
        // OpenGlContext::currentFramebuffer is non-const
        using GLCurrentFbReturnType = decltype(std::declval<OpenGlContext &>().currentFramebuffer());
        static_assert(std::is_same_v<GLCurrentFbReturnType, GLFramebuffer *>,
                      "OpenGlContext::currentFramebuffer should return GLFramebuffer*");
    }

#if HAVE_VULKAN
    // Check Vulkan signature matches OpenGL (non-const method)
    {
        using VkCurrentFbReturnType = decltype(std::declval<VulkanContext &>().currentFramebuffer());
        static_assert(std::is_same_v<VkCurrentFbReturnType, VulkanFramebuffer *>,
                      "VulkanContext::currentFramebuffer should return VulkanFramebuffer*");
    }
#endif

    QVERIFY(true); // Static asserts passed
}

QTEST_GUILESS_MAIN(BackendConsistencyTest)
#include "backend_consistency_test.moc"
