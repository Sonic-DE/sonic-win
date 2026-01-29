/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "itemrenderer_vulkan.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
#include "effect/globals.h"
#include "platformsupport/scenes/vulkan/vulkanbackend.h"
#include "platformsupport/scenes/vulkan/vulkanbuffer.h"
#include "platformsupport/scenes/vulkan/vulkancontext.h"
#include "platformsupport/scenes/vulkan/vulkanframebuffer.h"
#include "platformsupport/scenes/vulkan/vulkanpipeline.h"
#include "platformsupport/scenes/vulkan/vulkanpipelinemanager.h"
#include "platformsupport/scenes/vulkan/vulkanrenderpass.h"
#include "platformsupport/scenes/vulkan/vulkanrendertarget.h"
#include "platformsupport/scenes/vulkan/vulkansurfacetexture.h"
#include "platformsupport/scenes/vulkan/vulkansurfacetexture_x11.h"
#include "platformsupport/scenes/vulkan/vulkantexture.h"
#include "scene/decorationitem.h"
#include "scene/imageitem.h"
#include "scene/item.h"
#include "scene/itemgeometry.h"
#include "scene/outlinedborderitem.h"
#include "scene/shadowitem.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "scene/workspacescene_vulkan.h"
#include "utils/common.h"

#include <cstring>
#include <typeinfo>

namespace KWin
{

ItemRendererVulkan::ItemRendererVulkan(VulkanBackend *backend)
    : m_backend(backend)
    , m_context(backend->vulkanContext())
{
    // Create uniform buffer for shader parameters
    m_uniformBuffer = VulkanBuffer::createUniformBuffer(
        m_context,
        sizeof(VulkanUniforms) * 1024); // Support up to 1024 draws per frame
}

ItemRendererVulkan::~ItemRendererVulkan()
{
    if (m_context) {
        m_context->makeCurrent();
    }
    m_uniformBuffer.reset();
}

std::unique_ptr<ImageItem> ItemRendererVulkan::createImageItem(Item *parent)
{
    return std::make_unique<ImageItemVulkan>(parent);
}

void ItemRendererVulkan::beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport)
{
    if (!m_context->makeCurrent()) {
        qCWarning(KWIN_CORE) << "Failed to make Vulkan context current";
        return;
    }

    // Don't reset descriptor pool here - it will be reset on-demand when exhausted
    // This avoids invalidating descriptors that are still in use by in-flight command buffers
    m_outputsInFlight++;
    m_frameNumber++;

    // Allocate command buffer for this frame
    m_currentCommandBuffer = m_context->allocateCommandBuffer();
    if (m_currentCommandBuffer == VK_NULL_HANDLE) {
        qCWarning(KWIN_CORE) << "Failed to allocate command buffer";
        return;
    }

    // Begin command buffer recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(m_currentCommandBuffer, &beginInfo) != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to begin command buffer recording";
        return;
    }
    qCDebug(KWIN_CORE) << "Successfully began command buffer recording";

    // Get the framebuffer from render target
    // Extract the VulkanRenderTarget from the RenderTarget
    const auto vulkanRenderTarget = renderTarget.vulkanTarget();
    if (vulkanRenderTarget) {
        m_currentFramebuffer = vulkanRenderTarget->framebuffer();
        // Extract sync info for GPU-GPU synchronization
        if (vulkanRenderTarget->hasSyncInfo()) {
            m_currentSyncInfo = vulkanRenderTarget->syncInfo();
        } else {
            m_currentSyncInfo = VulkanSyncInfo{}; // Reset to defaults
        }
    }

    // Use the viewport's projection matrix which includes render target transform
    // This is important for multi-monitor setups with different orientations
    m_currentProjection = viewport.projectionMatrix();
    const QSize size = viewport.renderRect().size().toSize();

    static int projLogCount = 0;
    if (projLogCount < 3) {
        qWarning() << "VULKAN: Projection matrix:" << m_currentProjection;
        qWarning() << "VULKAN: Viewport renderRect=" << viewport.renderRect() << "scale=" << viewport.scale();
        qWarning() << "VULKAN: Framebuffer=" << m_currentFramebuffer
                   << "size=" << (m_currentFramebuffer ? m_currentFramebuffer->size() : QSize())
                   << "syncInfo.imageAvailable=" << m_currentSyncInfo.imageAvailableSemaphore
                   << "syncInfo.renderFinished=" << m_currentSyncInfo.renderFinishedSemaphore;
        projLogCount++;
    }

    // Begin render pass if we have a framebuffer
    if (m_currentFramebuffer) {
        // Set up clear values
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Clear to transparent black
        if (m_currentFramebuffer->renderPass()->config().hasDepth) {
            clearValues[1].depthStencil = {1.0f, 0};
        }

        qCDebug(KWIN_CORE) << "Beginning render pass with" << (m_currentFramebuffer->renderPass()->config().hasDepth ? 2 : 1) << "clear values";
        m_currentFramebuffer->beginRenderPass(m_currentCommandBuffer, clearValues.data(),
                                              m_currentFramebuffer->renderPass()->config().hasDepth ? 2 : 1);
        qCDebug(KWIN_CORE) << "Render pass begun successfully";
    }

    // Set viewport with Y-flip to match OpenGL coordinate conventions
    // Vulkan has Y pointing down, but our ortho projection expects Y=0 at top
    // Using negative height (VK_KHR_maintenance1) flips the Y axis
    VkViewport vkViewport{};
    vkViewport.x = 0.0f;
    vkViewport.y = static_cast<float>(size.height()); // Start at bottom
    vkViewport.width = static_cast<float>(size.width());
    vkViewport.height = -static_cast<float>(size.height()); // Negative flips Y
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_currentCommandBuffer, 0, 1, &vkViewport);

    qCDebug(KWIN_CORE) << "beginFrame: Set viewport to" << size.width() << "x" << size.height();

    // Set scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height())};
    vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &scissor);

    qCDebug(KWIN_CORE) << "beginFrame: Set scissor to" << size.width() << "x" << size.height();

    qCDebug(KWIN_CORE) << "ItemRendererVulkan::beginFrame() - viewport:" << size;
}

void ItemRendererVulkan::endFrame()
{
    if (m_currentCommandBuffer == VK_NULL_HANDLE) {
        qCWarning(KWIN_CORE) << "endFrame called with null command buffer";
        return;
    }

    // End render pass if we were rendering to a framebuffer
    if (m_currentFramebuffer) {
        qCDebug(KWIN_CORE) << "Ending render pass";
        m_currentFramebuffer->endRenderPass(m_currentCommandBuffer);
        qCDebug(KWIN_CORE) << "Render pass ended successfully";
    } else {
        qCDebug(KWIN_CORE) << "endFrame: No framebuffer to end render pass";
    }

    // End command buffer recording
    VkResult result = vkEndCommandBuffer(m_currentCommandBuffer);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to end command buffer recording:" << result;
        return;
    }
    qCDebug(KWIN_CORE) << "Successfully ended command buffer recording";

    // Submit command buffer to graphics queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_currentCommandBuffer;

    // Check if we have GPU-GPU synchronization info (swapchain rendering)
    const bool hasGpuSync = m_currentSyncInfo.imageAvailableSemaphore != VK_NULL_HANDLE && m_currentSyncInfo.renderFinishedSemaphore != VK_NULL_HANDLE;

    qCDebug(KWIN_CORE) << "GPU-GPU sync check:" << (hasGpuSync ? "enabled" : "disabled")
                       << "imageAvailableSemaphore:" << m_currentSyncInfo.imageAvailableSemaphore
                       << "renderFinishedSemaphore:" << m_currentSyncInfo.renderFinishedSemaphore;

    if (hasGpuSync) {
        // GPU-GPU semaphore synchronization (no CPU blocking needed for render-present sync)
        // Wait on imageAvailableSemaphore before writing to the swapchain image
        VkSemaphore waitSemaphores[] = {m_currentSyncInfo.imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        // Signal renderFinishedSemaphore when rendering is done (for present to wait on)
        VkSemaphore signalSemaphores[] = {m_currentSyncInfo.renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // Use the in-flight fence for CPU-GPU sync (command buffer reuse protection)
        VkFence fence = m_currentSyncInfo.inFlightFence;
        if (fence == VK_NULL_HANDLE) {
            fence = m_context->getOrCreateFence();
            vkResetFences(m_backend->device(), 1, &fence);
        }

        VkResult result = vkQueueSubmit(m_backend->graphicsQueue(), 1, &submitInfo, fence);
        if (result != VK_SUCCESS) {
            qCWarning(KWIN_CORE) << "Failed to submit command buffer with GPU-GPU sync:" << result;
            m_currentCommandBuffer = VK_NULL_HANDLE;
            m_currentFramebuffer = nullptr;
            m_currentSyncInfo = VulkanSyncInfo{};
            return;
        }
        qCDebug(KWIN_CORE) << "Successfully submitted command buffer with GPU-GPU sync, waiting for fence" << fence;

        // No blocking wait here! The GPU-GPU synchronization handles the timing:
        // - Render waits on imageAvailableSemaphore (signaled by acquireNextImage)
        // - Present waits on renderFinishedSemaphore (signaled by this submit)
        // - The inFlightFence will be waited on at the START of the NEXT frame

        // Handle release points for external sync (e.g., DMA-BUF clients)
        if (!m_releasePoints.empty() && m_context->supportsExternalFenceFd()) {
            // Export fence to sync fd for release points
            VkFence exportableFence = m_context->createExportableFence();
            if (exportableFence != VK_NULL_HANDLE) {
                // Submit a no-op command to signal the exportable fence after render completes
                VkSubmitInfo syncSubmit{};
                syncSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                syncSubmit.waitSemaphoreCount = 1;
                syncSubmit.pWaitSemaphores = signalSemaphores; // Wait on renderFinished
                VkPipelineStageFlags syncStages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
                syncSubmit.pWaitDstStageMask = syncStages;

                if (vkQueueSubmit(m_backend->graphicsQueue(), 1, &syncSubmit, exportableFence) == VK_SUCCESS) {
                    FileDescriptor syncFd = m_context->exportFenceToSyncFd(exportableFence);
                    if (syncFd.isValid()) {
                        for (const auto &releasePoint : m_releasePoints) {
                            if (releasePoint) {
                                releasePoint->addReleaseFence(syncFd);
                            }
                        }
                    }
                }
                vkDestroyFence(m_backend->device(), exportableFence, nullptr);
            }
        }
        m_releasePoints.clear();

        qCDebug(KWIN_CORE) << "ItemRendererVulkan::endFrame() - GPU-GPU semaphore sync";
    } else {
        qCDebug(KWIN_CORE) << "Using fallback path: no swapchain semaphores, use blocking synchronization";
        // Fallback: no swapchain semaphores, use blocking synchronization
        // This path is used for:
        // - Rendering to offscreen textures
        // - Backends without proper semaphore support

        // Try external fence fd for non-blocking client sync if supported
        if (m_context->supportsExternalFenceFd() && !m_releasePoints.empty()) {
            VkFence exportableFence = m_context->createExportableFence();
            if (exportableFence != VK_NULL_HANDLE) {
                if (vkQueueSubmit(m_backend->graphicsQueue(), 1, &submitInfo, exportableFence) == VK_SUCCESS) {
                    FileDescriptor syncFd = m_context->exportFenceToSyncFd(exportableFence);
                    if (syncFd.isValid()) {
                        for (const auto &releasePoint : m_releasePoints) {
                            if (releasePoint) {
                                releasePoint->addReleaseFence(syncFd);
                            }
                        }
                        qCDebug(KWIN_CORE) << "ItemRendererVulkan::endFrame() - non-blocking sync with export fence";
                    } else {
                        vkWaitForFences(m_backend->device(), 1, &exportableFence, VK_TRUE, UINT64_MAX);
                    }
                    m_releasePoints.clear();
                    vkDestroyFence(m_backend->device(), exportableFence, nullptr);
                    m_currentCommandBuffer = VK_NULL_HANDLE;
                    m_currentFramebuffer = nullptr;
                    m_currentSyncInfo = VulkanSyncInfo{};
                    return;
                }
                vkDestroyFence(m_backend->device(), exportableFence, nullptr);
            }
        }

        // Final fallback: blocking synchronization
        VkFence fence = m_context->getOrCreateFence();
        vkResetFences(m_backend->device(), 1, &fence);

        VkResult result = vkQueueSubmit(m_backend->graphicsQueue(), 1, &submitInfo, fence);
        if (result != VK_SUCCESS) {
            qCWarning(KWIN_CORE) << "Failed to submit command buffer:" << result;
            m_currentCommandBuffer = VK_NULL_HANDLE;
            m_currentFramebuffer = nullptr;
            m_currentSyncInfo = VulkanSyncInfo{};
            return;
        }
        qCDebug(KWIN_CORE) << "Successfully submitted command buffer with export fence";

        // Wait for the frame to complete
        vkWaitForFences(m_backend->device(), 1, &fence, VK_TRUE, UINT64_MAX);
        m_releasePoints.clear();

        qCDebug(KWIN_CORE) << "ItemRendererVulkan::endFrame() - blocking sync (fallback)";
    }

    m_currentCommandBuffer = VK_NULL_HANDLE;
    m_currentFramebuffer = nullptr;
    m_currentSyncInfo = VulkanSyncInfo{};

    // Decrement output counter - when it reaches 0, next beginFrame can reset pool
    if (m_outputsInFlight > 0) {
        m_outputsInFlight--;
    }
}

void ItemRendererVulkan::renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region)
{
    if (m_currentCommandBuffer == VK_NULL_HANDLE) {
        return;
    }

    // The render pass already clears the framebuffer with the clear color
    // If we need to render a specific background color or pattern, we'd do it here
    // For now, the clear in beginFrame handles this

    qCDebug(KWIN_CORE) << "ItemRendererVulkan::renderBackground() - region:" << region.boundingRect();
}

QVector4D ItemRendererVulkan::modulate(float opacity, float brightness) const
{
    const float a = opacity;
    const float rgb = opacity * brightness;
    return QVector4D(rgb, rgb, rgb, a);
}

// Helper to build geometry from quads using RenderGeometry (matching OpenGL's clipQuads approach)
static RenderGeometry buildGeometryFromQuads(const WindowQuadList &quads, qreal deviceScale)
{
    RenderGeometry geometry;
    geometry.reserve(quads.count() * 6);

    for (const WindowQuad &quad : quads) {
        geometry.appendWindowQuad(quad, deviceScale);
    }

    return geometry;
}

void ItemRendererVulkan::createRenderNode(Item *item, RenderContext *context)
{
    const QList<Item *> sortedChildItems = item->sortedChildItems();
    const qreal scale = context->renderTargetScale;

    // Build transform matrix for this item (matching OpenGL approach)
    const auto logicalPosition = QVector2D(item->position().x(), item->position().y());

    QMatrix4x4 matrix;
    matrix.translate(roundVector(logicalPosition * scale).toVector3D());
    if (context->transformStack.size() == 1) {
        matrix *= context->rootTransform;
    }
    if (!item->transform().isIdentity()) {
        matrix.scale(scale, scale);
        matrix *= item->transform();
        matrix.scale(1 / scale, 1 / scale);
    }
    context->transformStack.push(context->transformStack.top() * matrix);
    context->opacityStack.push(context->opacityStack.top() * item->opacity());

    // Process child items with z < 0 first (behind this item)
    for (Item *childItem : sortedChildItems) {
        if (childItem->z() >= 0) {
            break;
        }
        if (childItem->explicitVisible()) {
            createRenderNode(childItem, context);
        }
    }

    // Handle border radius (matching OpenGL approach)
    if (const BorderRadius radius = item->borderRadius(); !radius.isNull()) {
        const QRectF nativeRect = snapToPixelGridF(scaledRect(item->rect(), scale));
        const BorderRadius nativeRadius = radius.scaled(scale).rounded();
        context->cornerStack.push({
            .box = nativeRect,
            .radius = nativeRadius,
        });
    } else if (!context->cornerStack.isEmpty()) {
        const auto &top = std::as_const(context->cornerStack).top();
        context->cornerStack.push({
            .box = matrix.inverted().mapRect(top.box),
            .radius = top.radius,
        });
    }

    // Preprocess the item to ensure pixmap/texture is created
    item->preprocess();

    // Build geometry from quads (using RenderGeometry like OpenGL)
    RenderGeometry geometry = buildGeometryFromQuads(item->quads(), scale);

    // Handle different item types (matching OpenGL's approach)
    if (auto shadowItem = qobject_cast<ShadowItem *>(item)) {
        // ShadowItem - use VulkanShadowTextureProvider
        auto *textureProvider = static_cast<VulkanShadowTextureProvider *>(shadowItem->textureProvider());
        qWarning() << "VULKAN: ShadowItem geometry=" << geometry.count() << "vertices"
                   << "textureProvider=" << textureProvider
                   << "texture=" << (textureProvider ? textureProvider->texture() : nullptr);
        if (!geometry.isEmpty()) {
            if (textureProvider && textureProvider->texture()) {
                VulkanTexture *texture = textureProvider->texture();
                qWarning() << "VULKAN: ShadowItem texture size=" << texture->size()
                           << "valid=" << texture->isValid();

                // Post-process texture coordinates (convert pixel coords to normalized)
                geometry.postProcessTextureCoordinates(texture->matrix(VulkanCoordinateType::Unnormalized));

                RenderNode node;
                node.traits = ShaderTrait::MapTexture;
                node.textures.append(texture);
                node.opacity = context->opacityStack.top();
                node.hasAlpha = true;
                node.colorDescription = item->colorDescription();
                node.renderingIntent = item->renderingIntent();
                node.bufferReleasePoint = nullptr;
                node.transformMatrix = context->transformStack.top();

                // Copy geometry to node
                for (const auto &vertex : geometry) {
                    node.geometry.append({
                        .position = vertex.position,
                        .texcoord = vertex.texcoord,
                    });
                }
                node.vertexCount = node.geometry.count();

                if (node.opacity < 1.0) {
                    node.traits |= ShaderTrait::Modulate;
                }

                context->renderNodes.append(node);
                // Log shadow transform for debugging
                float tx = node.transformMatrix.column(3).x();
                float ty = node.transformMatrix.column(3).y();
                qWarning() << "VULKAN: ShadowItem ADDED render node with" << node.vertexCount << "vertices"
                           << "transform=(" << tx << "," << ty << ")"
                           << "texSize=" << texture->size();
            }
        }
    } else if (auto decorationItem = qobject_cast<DecorationItem *>(item)) {
        // DecorationItem - use SceneVulkanDecorationRenderer
        auto *renderer = static_cast<const SceneVulkanDecorationRenderer *>(decorationItem->renderer());
        qWarning() << "VULKAN: DecorationItem geometry=" << geometry.count() << "vertices"
                   << "renderer=" << renderer
                   << "texture=" << (renderer ? renderer->texture() : nullptr);
        if (!geometry.isEmpty()) {
            if (renderer && renderer->texture()) {
                VulkanTexture *texture = renderer->texture();
                qWarning() << "VULKAN: DecorationItem texture size=" << texture->size()
                           << "valid=" << texture->isValid();

                // Post-process texture coordinates (convert pixel coords to normalized)
                geometry.postProcessTextureCoordinates(texture->matrix(VulkanCoordinateType::Unnormalized));

                RenderNode node;
                node.traits = ShaderTrait::MapTexture;
                node.textures.append(texture);
                node.opacity = context->opacityStack.top();
                node.hasAlpha = true;
                node.colorDescription = item->colorDescription();
                node.renderingIntent = item->renderingIntent();
                node.bufferReleasePoint = nullptr;
                node.transformMatrix = context->transformStack.top();

                // Copy geometry to node
                for (const auto &vertex : geometry) {
                    node.geometry.append({
                        .position = vertex.position,
                        .texcoord = vertex.texcoord,
                    });
                }
                node.vertexCount = node.geometry.count();

                if (node.opacity < 1.0) {
                    node.traits |= ShaderTrait::Modulate;
                }

                context->renderNodes.append(node);
                qWarning() << "VULKAN: DecorationItem ADDED render node with" << node.vertexCount << "vertices";
            }
        }
    } else if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        // SurfaceItem - main window content
        SurfacePixmap *pixmap = surfaceItem->pixmap();
        auto vulkanSurfaceTexture = pixmap ? dynamic_cast<VulkanSurfaceTextureX11 *>(pixmap->texture()) : nullptr;
        if (pixmap && !geometry.isEmpty()) {
            if (vulkanSurfaceTexture && vulkanSurfaceTexture->texture() && vulkanSurfaceTexture->texture()->isValid()) {
                VulkanTexture *texture = vulkanSurfaceTexture->texture();

                // Log geometry details
                qWarning() << "VULKAN: SurfaceItem texture=" << texture->size()
                           << "itemPos=" << item->position() << "itemRect=" << item->rect()
                           << "quads=" << item->quads().count() << "geomVerts=" << geometry.count();

                // Log vertices for LARGE items (backgrounds) to debug rendering
                static int largeSurfaceLogCount = 0;
                if (texture->size().height() >= 1000 && largeSurfaceLogCount < 10) {
                    qWarning() << "VULKAN: LARGE SurfaceItem (background?) VERTS:";
                    for (int i = 0; i < qMin(geometry.count(), 6); i++) {
                        qWarning() << "  v" << i << ": pos=" << geometry[i].position << "tex=" << geometry[i].texcoord;
                    }
                    qWarning() << "VULKAN: LARGE SurfaceItem transform:" << context->transformStack.top();
                    largeSurfaceLogCount++;
                }

                // Log vertices for small items (panels) to debug sizing issue
                static int smallSurfaceLogCount = 0;
                if (texture->size().height() < 100 && smallSurfaceLogCount < 5) {
                    qWarning() << "VULKAN: SMALL SurfaceItem VERTS (panel?):";
                    for (int i = 0; i < qMin(geometry.count(), 6); i++) {
                        qWarning() << "  v" << i << ": pos=" << geometry[i].position << "tex=" << geometry[i].texcoord;
                    }
                    smallSurfaceLogCount++;
                }

                // Post-process texture coordinates (convert pixel coords to normalized)
                QMatrix4x4 texMatrix = texture->matrix(VulkanCoordinateType::Unnormalized);
                geometry.postProcessTextureCoordinates(texMatrix);

                // Log transform matrix and NORMALIZED tex coords for small items (panels)
                static int smallTransformLogCount = 0;
                if (texture->size().height() < 100 && smallTransformLogCount < 5) {
                    qWarning() << "VULKAN: SMALL SurfaceItem texMatrix(0,0)=" << texMatrix(0, 0)
                               << "texMatrix(1,1)=" << texMatrix(1, 1)
                               << "texMatrix(0,3)=" << texMatrix(0, 3)
                               << "texMatrix(1,3)=" << texMatrix(1, 3);
                    qWarning() << "VULKAN: SMALL SurfaceItem NORMALIZED tex coords:";
                    for (int i = 0; i < qMin(geometry.count(), 6); i++) {
                        qWarning() << "  v" << i << ": tex=" << geometry[i].texcoord;
                    }
                    qWarning() << "VULKAN: SMALL SurfaceItem transform:" << context->transformStack.top();
                    smallTransformLogCount++;
                }

                RenderNode node;
                node.traits = ShaderTrait::MapTexture;
                node.textures.append(texture);
                node.opacity = context->opacityStack.top();
                node.hasAlpha = pixmap->hasAlphaChannel();
                node.colorDescription = surfaceItem->colorDescription();
                node.renderingIntent = surfaceItem->renderingIntent();
                node.bufferReleasePoint = surfaceItem->bufferReleasePoint();
                node.transformMatrix = context->transformStack.top();

                if (node.bufferReleasePoint) {
                    m_releasePoints.insert(node.bufferReleasePoint);
                }

                // Copy geometry to node
                for (const auto &vertex : geometry) {
                    node.geometry.append({
                        .position = vertex.position,
                        .texcoord = vertex.texcoord,
                    });
                }
                node.vertexCount = node.geometry.count();

                // Handle rounded corners
                if (!context->cornerStack.isEmpty()) {
                    const auto &top = context->cornerStack.top();
                    if (!top.radius.isNull()) {
                        node.traits |= ShaderTrait::RoundedCorners;
                        node.hasAlpha = true;
                        node.box = QVector4D(top.box.x() + top.box.width() * 0.5,
                                             top.box.y() + top.box.height() * 0.5,
                                             top.box.width() * 0.5,
                                             top.box.height() * 0.5);
                        node.borderRadius = top.radius.toVector();
                    }
                }

                // Handle opacity modulation
                if (node.opacity < 1.0) {
                    node.traits |= ShaderTrait::Modulate;
                }

                context->renderNodes.append(node);
                qWarning() << "VULKAN: SurfaceItem ADDED render node with" << node.vertexCount << "vertices, opacity=" << node.opacity;
            } else {
                qWarning() << "VULKAN: SurfaceItem SKIPPED - invalid texture";
            }
        }
    } else if (auto imageItem = qobject_cast<ImageItemVulkan *>(item)) {
        // ImageItemVulkan - use the texture from preprocess()
        qWarning() << "VULKAN: ImageItemVulkan geometry=" << geometry.count() << "vertices"
                   << "texture=" << imageItem->texture();
        if (!geometry.isEmpty() && imageItem->texture()) {
            VulkanTexture *texture = imageItem->texture();
            qWarning() << "VULKAN: ImageItemVulkan texture size=" << texture->size()
                       << "valid=" << texture->isValid();

            // Post-process texture coordinates (convert pixel coords to normalized)
            geometry.postProcessTextureCoordinates(texture->matrix(VulkanCoordinateType::Unnormalized));

            RenderNode node;
            node.traits = ShaderTrait::MapTexture;
            node.textures.append(texture);
            node.opacity = context->opacityStack.top();
            node.hasAlpha = true;
            node.colorDescription = item->colorDescription();
            node.renderingIntent = item->renderingIntent();
            node.bufferReleasePoint = nullptr;
            node.transformMatrix = context->transformStack.top();

            // Copy geometry to node
            for (const auto &vertex : geometry) {
                node.geometry.append({
                    .position = vertex.position,
                    .texcoord = vertex.texcoord,
                });
            }
            node.vertexCount = node.geometry.count();

            if (node.opacity < 1.0) {
                node.traits |= ShaderTrait::Modulate;
            }

            context->renderNodes.append(node);
            qWarning() << "VULKAN: ImageItemVulkan ADDED render node with" << node.vertexCount << "vertices";
        }
    } else if (auto borderItem = qobject_cast<OutlinedBorderItem *>(item)) {
        // OutlinedBorderItem - uses Border trait, no texture needed
        qWarning() << "VULKAN: OutlinedBorderItem geometry=" << geometry.count() << "vertices";
        if (!geometry.isEmpty()) {
            const BorderOutline outline = borderItem->outline();
            const int thickness = std::round(outline.thickness() * scale);
            const QRectF outerRect = snapToPixelGridF(scaledRect(borderItem->rect(), scale));
            const QRectF innerRect = outerRect.adjusted(thickness, thickness, -thickness, -thickness);
            qWarning() << "VULKAN: OutlinedBorderItem outerRect=" << outerRect << "innerRect=" << innerRect
                       << "color=" << outline.color();

            RenderNode node;
            node.traits = ShaderTrait::Border;
            node.opacity = context->opacityStack.top();
            node.hasAlpha = true;
            node.transformMatrix = context->transformStack.top();
            node.box = QVector4D(outerRect.x(), outerRect.y(), outerRect.width(), outerRect.height());
            // Inner box stored in borderRadius for the Border shader
            node.borderRadius = QVector4D(innerRect.x(), innerRect.y(), innerRect.width(), innerRect.height());
            node.borderColor = outline.color();

            // Copy geometry to node
            for (const auto &vertex : geometry) {
                node.geometry.append({
                    .position = vertex.position,
                    .texcoord = vertex.texcoord,
                });
            }
            node.vertexCount = node.geometry.count();

            if (node.opacity < 1.0) {
                node.traits |= ShaderTrait::Modulate;
            }

            context->renderNodes.append(node);
            qWarning() << "VULKAN: OutlinedBorderItem ADDED render node with" << node.vertexCount << "vertices";
        }
    } else {
        // Unhandled item type
        qWarning() << "VULKAN: UNHANDLED item type:" << item->metaObject()->className()
                   << "geometry=" << geometry.count() << "vertices";
    }

    // Process children with z >= 0 (in front of this item)
    for (Item *childItem : sortedChildItems) {
        if (childItem->z() < 0) {
            continue;
        }
        if (childItem->explicitVisible()) {
            createRenderNode(childItem, context);
        }
    }

    // Pop stacks at the end (matching the push at the start)
    context->transformStack.pop();
    context->opacityStack.pop();
    if (!context->cornerStack.isEmpty()) {
        context->cornerStack.pop();
    }
}

void ItemRendererVulkan::renderNodes(const RenderContext &context, VkCommandBuffer cmd)
{
    if (context.renderNodes.isEmpty()) {
        return;
    }

    auto *pipelineManager = m_context->pipelineManager();
    VulkanBuffer *streamingBuffer = m_context->streamingBuffer();

    // Calculate total vertex data size
    size_t totalVertexSize = 0;
    for (const RenderNode &node : context.renderNodes) {
        totalVertexSize += node.geometry.count() * sizeof(GLVertex2D);
    }

    // Upload all vertex data to streaming buffer
    std::vector<GLVertex2D> allVertices;
    allVertices.reserve(totalVertexSize / sizeof(GLVertex2D));

    int currentVertexOffset = 0;
    for (RenderNode &node : const_cast<QList<RenderNode> &>(context.renderNodes)) {
        node.firstVertex = currentVertexOffset;
        for (int i = 0; i < node.geometry.count(); ++i) {
            allVertices.push_back(node.geometry.at(i));
        }
        currentVertexOffset += node.vertexCount;
    }

    if (!allVertices.empty()) {
        void *mappedData = streamingBuffer->map();
        if (mappedData) {
            memcpy(mappedData, allVertices.data(), allVertices.size() * sizeof(GLVertex2D));
            streamingBuffer->unmap();
            // Flush to ensure vertex data is visible to GPU (needed for non-HOST_COHERENT memory)
            streamingBuffer->flush(0, allVertices.size() * sizeof(GLVertex2D));
        }

        // Bind vertex buffer
        VkBuffer vertexBuffers[] = {streamingBuffer->buffer()};
        VkDeviceSize offsets[] = {0};

        // Validate vertex buffer before binding
        if (vertexBuffers[0] == VK_NULL_HANDLE) {
            qCWarning(KWIN_CORE) << "Vertex buffer is null, skipping vertex buffer binding";
        } else {
            vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
            qCDebug(KWIN_CORE) << "Bound vertex buffer successfully";
        }
    }

    // Render each node
    VulkanPipeline *currentPipeline = nullptr;

    for (const RenderNode &node : context.renderNodes) {
        if (node.vertexCount == 0) {
            continue;
        }

        // Get or create pipeline for this node's traits
        VulkanPipeline *pipeline = pipelineManager->pipeline(node.traits);
        if (!pipeline) {
            qCWarning(KWIN_CORE) << "Failed to get pipeline for traits:" << static_cast<int>(node.traits);
            continue;
        }

        // Validate pipeline before using it
        if (!pipeline->isValid()) {
            qCWarning(KWIN_CORE) << "Pipeline is invalid for traits:" << static_cast<int>(node.traits);
            continue;
        }

        // Bind pipeline if changed
        if (pipeline != currentPipeline) {
            if (pipeline->pipeline() != VK_NULL_HANDLE) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline());
                currentPipeline = pipeline;
                qCDebug(KWIN_CORE) << "Bound pipeline with traits:" << static_cast<int>(node.traits);
            } else {
                qCWarning(KWIN_CORE) << "Skipping null pipeline binding for traits:" << static_cast<int>(node.traits);
                continue;
            }
        }

        // Set push constants (MVP matrix and texture matrix)
        VulkanPushConstants pc{};
        const QMatrix4x4 mvp = context.projectionMatrix * node.transformMatrix;

        // Debug: log what we're drawing and where
        static int drawDebugCount = 0;
        if (drawDebugCount < 10 && !node.textures.isEmpty()) {
            QSize texSize = node.textures[0]->size();
            // Extract translation from transform matrix (column 3, rows 0-1)
            float tx = node.transformMatrix.column(3).x();
            float ty = node.transformMatrix.column(3).y();
            qWarning() << "VULKAN: RENDER NODE - texSize=" << texSize
                       << "firstVert=" << node.firstVertex << "vertCount=" << node.vertexCount
                       << "translate=(" << tx << "," << ty << ")";
            // Log first vertex position
            if (!node.geometry.isEmpty()) {
                qWarning() << "  First vertex pos=(" << node.geometry[0].position.x()
                           << "," << node.geometry[0].position.y() << ")";
            }
            drawDebugCount++;
        }

        memcpy(pc.mvp, mvp.data(), sizeof(pc.mvp));
        // Texture matrix is identity - we already applied it via postProcessTextureCoordinates()
        // on the CPU side before uploading the geometry, so the shader doesn't need to transform again
        memset(pc.textureMatrix, 0, sizeof(pc.textureMatrix));
        pc.textureMatrix[0] = 1.0f;
        pc.textureMatrix[5] = 1.0f;
        pc.textureMatrix[10] = 1.0f;
        pc.textureMatrix[15] = 1.0f;

        vkCmdPushConstants(cmd, pipeline->layout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(VulkanPushConstants), &pc);

        // Update and bind descriptor sets for textures and UBO
        // Pipeline requires descriptor set 0, so we must bind one before drawing
        if (!node.textures.isEmpty() && node.textures[0]) {
            VkDescriptorSet descriptorSet = m_context->allocateDescriptorSet(pipeline->descriptorSetLayout());
            if (descriptorSet == VK_NULL_HANDLE) {
                qCWarning(KWIN_CORE) << "Failed to allocate descriptor set, skipping node";
                continue;
            }

            // Update descriptor set with texture (binding 0)
            VkDescriptorImageInfo imageInfo{};
            imageInfo.sampler = node.textures[0]->sampler();
            imageInfo.imageView = node.textures[0]->imageView();
            if (imageInfo.imageView == VK_NULL_HANDLE) {
                qCWarning(KWIN_CORE) << "Texture has null image view, skipping node";
                continue;
            }
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Prepare uniform buffer data
            VulkanUniforms uniforms{};
            const QVector4D modulation = modulate(node.opacity, 1.0f);
            memcpy(uniforms.uniformColor, &modulation, sizeof(uniforms.uniformColor));
            uniforms.opacity = node.opacity;
            uniforms.brightness = 1.0f;
            uniforms.saturation = 1.0f;
            memcpy(uniforms.geometryBox, &node.box, sizeof(uniforms.geometryBox));
            memcpy(uniforms.borderRadius, &node.borderRadius, sizeof(uniforms.borderRadius));
            uniforms.borderThickness = node.borderThickness;
            const QVector4D borderColor(node.borderColor.redF(), node.borderColor.greenF(),
                                        node.borderColor.blueF(), node.borderColor.alphaF());
            memcpy(uniforms.borderColor, &borderColor, sizeof(uniforms.borderColor));

            // Upload uniforms to buffer
            static int uniformIndex = 0;
            VkDeviceSize uniformOffset = (uniformIndex % 1024) * sizeof(VulkanUniforms);
            m_uniformBuffer->upload(&uniforms, sizeof(VulkanUniforms), uniformOffset);
            uniformIndex++;

            // UBO descriptor (binding 1)
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffer->buffer();
            bufferInfo.offset = uniformOffset;
            bufferInfo.range = sizeof(VulkanUniforms);

            // Update both bindings
            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            // Binding 0: Texture
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSet;
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfo;

            // Binding 1: UBO
            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSet;
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_backend->device(), static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline->layout(), 0, 1, &descriptorSet, 0, nullptr);
            qCDebug(KWIN_CORE) << "Bound descriptor set with texture and UBO";
        } else {
            // No texture available but pipeline expects one - skip this node
            // TODO: Add default 1x1 white texture for non-textured draws
            qCDebug(KWIN_CORE) << "No texture for MapTexture pipeline, skipping node";
            continue;
        }

        // Draw (descriptor set is guaranteed bound since we continue above if not)
        if (node.vertexCount > 0) {
            vkCmdDraw(cmd, node.vertexCount, 1, node.firstVertex, 0);

            // Log ALL large texture draws (backgrounds)
            if (!node.textures.isEmpty()) {
                QSize texSize = node.textures[0]->size();
                static int largeDrawCount = 0;
                static int smallDrawCount = 0;

                if (texSize.height() >= 1000) {
                    // This is a background - log every time to track
                    if (largeDrawCount < 30) {
                        qWarning() << "VULKAN: DRAW LARGE (background?)" << node.vertexCount << "verts"
                                   << "texSize=" << texSize
                                   << "opacity=" << node.opacity
                                   << "firstVert=" << node.firstVertex;
                        // Log first few vertex positions
                        if (!node.geometry.isEmpty()) {
                            qWarning() << "  v0 pos=" << node.geometry[0].position << "tex=" << node.geometry[0].texcoord;
                            if (node.geometry.size() > 1)
                                qWarning() << "  v1 pos=" << node.geometry[1].position << "tex=" << node.geometry[1].texcoord;
                        }
                        largeDrawCount++;
                    }
                } else if (smallDrawCount < 10) {
                    qWarning() << "VULKAN: DRAW SMALL" << node.vertexCount << "verts at offset" << node.firstVertex
                               << "texSize=" << texSize << "opacity=" << node.opacity;
                    smallDrawCount++;
                }
            }
        }
    }
}

void ItemRendererVulkan::renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data)
{
    if (m_currentCommandBuffer == VK_NULL_HANDLE) {
        qCWarning(KWIN_CORE) << "No active command buffer for rendering";
        return;
    }

    // Build render context
    RenderContext context{
        .renderNodes = {},
        .transformStack = {},
        .opacityStack = {},
        .cornerStack = {},
        .projectionMatrix = m_currentProjection,
        .rootTransform = data.toMatrix(viewport.scale()),
        .clip = region,
        .hardwareClipping = static_cast<bool>(mask & Effect::PAINT_WINDOW_TRANSFORMED),
        .renderTargetScale = viewport.scale(),
    };

    // Initialize stacks (matching OpenGL approach)
    // Push identity - rootTransform is applied in createRenderNode when stack size is 1
    context.transformStack.push(QMatrix4x4());
    context.opacityStack.push(data.opacity());

    // Build render nodes from item tree
    createRenderNode(item, &context);

    // Set scissor for clipping if needed
    static int scissorDebugCount = 0;
    if (scissorDebugCount < 20) {
        qWarning() << "VULKAN: renderItem - hardwareClipping=" << context.hardwareClipping
                   << "regionEmpty=" << region.isEmpty()
                   << "regionBounds=" << region.boundingRect()
                   << "mask=" << Qt::hex << mask
                   << "renderNodesCount=" << context.renderNodes.size();
        scissorDebugCount++;
    }

    if (context.hardwareClipping && !region.isEmpty()) {
        const QRect clipRect = region.boundingRect();
        VkRect2D scissor{};
        scissor.offset = {clipRect.x(), clipRect.y()};
        scissor.extent = {static_cast<uint32_t>(clipRect.width()), static_cast<uint32_t>(clipRect.height())};
        vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &scissor);
        qWarning() << "VULKAN: SET SCISSOR to" << clipRect;
    }

    // Render all nodes
    renderNodes(context, m_currentCommandBuffer);

    qCDebug(KWIN_CORE) << "ItemRendererVulkan::renderItem() - rendered" << context.renderNodes.size() << "nodes for item:" << item;
}

} // namespace KWin
