/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "itemrenderer_vulkan.h"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effect.h"
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
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#include "utils/common.h"

#include <cstring>

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

    // Calculate projection matrix
    const QSize size = viewport.renderRect().size().toSize();
    m_currentProjection.setToIdentity();
    m_currentProjection.ortho(0, size.width(), size.height(), 0, -1, 1);

    // Begin render pass if we have a framebuffer
    if (m_currentFramebuffer) {
        // Set up clear values
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 1.0f, 1.0f}}; // Clear to blue for better visibility
        if (m_currentFramebuffer->renderPass()->config().hasDepth) {
            clearValues[1].depthStencil = {1.0f, 0};
        }

        qCDebug(KWIN_CORE) << "Beginning render pass with" << (m_currentFramebuffer->renderPass()->config().hasDepth ? 2 : 1) << "clear values";
        m_currentFramebuffer->beginRenderPass(m_currentCommandBuffer, clearValues.data(),
                                              m_currentFramebuffer->renderPass()->config().hasDepth ? 2 : 1);
        qCDebug(KWIN_CORE) << "Render pass begun successfully";
    }

    // Set viewport
    VkViewport vkViewport{};
    vkViewport.x = 0.0f;
    vkViewport.y = 0.0f;
    vkViewport.width = static_cast<float>(size.width());
    vkViewport.height = static_cast<float>(size.height());
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

void ItemRendererVulkan::createRenderNode(Item *item, RenderContext *context)
{
    const QList<Item *> children = item->childItems();
    const QMatrix4x4 matrix = context->transformStack.top();
    const qreal opacity = context->opacityStack.top();

    // Get the item's quads and textures
    WindowQuadList quads = item->quads();
    if (quads.isEmpty()) {
        // Process children even if this item has no quads
        for (Item *child : children) {
            context->transformStack.push(matrix * child->transform());
            context->opacityStack.push(opacity * child->opacity());
            createRenderNode(child, context);
            context->transformStack.pop();
            context->opacityStack.pop();
        }
        return;
    }

    // Create render node for this item
    RenderNode node;
    node.opacity = opacity;
    node.hasAlpha = true; // Conservative default
    node.transformMatrix = matrix;
    node.traits = ShaderTrait::MapTexture;

    // Get color description for color management
    if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        node.colorDescription = surfaceItem->colorDescription();
        node.renderingIntent = surfaceItem->renderingIntent();
        node.bufferReleasePoint = surfaceItem->bufferReleasePoint();
        if (node.bufferReleasePoint) {
            m_releasePoints.insert(node.bufferReleasePoint);
        }
    }

    // Handle rounded corners
    if (!context->cornerStack.isEmpty()) {
        const RenderCorner &corner = context->cornerStack.top();
        if (!corner.radius.isNull()) {
            node.traits |= ShaderTrait::RoundedCorners;
            node.box = QVector4D(corner.box.x(), corner.box.y(),
                                 corner.box.width(), corner.box.height());
            node.borderRadius = QVector4D(
                corner.radius.topLeft(),
                corner.radius.topRight(),
                corner.radius.bottomRight(),
                corner.radius.bottomLeft());
        }
    }

    // Handle opacity modulation
    if (opacity < 1.0) {
        node.traits |= ShaderTrait::Modulate;
    }

    // Get textures from the item
    if (auto surfaceItem = qobject_cast<SurfaceItem *>(item)) {
        if (auto *surfacePixmap = surfaceItem->pixmap()) {
            // Try to get the Vulkan texture from the surface pixmap
            // This requires the surface pixmap to have a VulkanSurfaceTexture
            if (auto vulkanSurfaceTexture = dynamic_cast<VulkanSurfaceTextureX11 *>(surfacePixmap->texture())) {
                if (auto texture = vulkanSurfaceTexture->texture()) {
                    // Validate that the texture is valid before using it
                    if (texture->isValid()) {
                        node.textures.append(texture);
                        qCDebug(KWIN_CORE) << "ItemRendererVulkan::createRenderNode() - added valid texture to node";
                    } else {
                        qCWarning(KWIN_CORE) << "ItemRendererVulkan::createRenderNode() - texture is invalid, skipping";
                    }
                } else {
                    qCDebug(KWIN_CORE) << "ItemRendererVulkan::createRenderNode() - no Vulkan texture available";
                }
            } else {
                qCDebug(KWIN_CORE) << "ItemRendererVulkan::createRenderNode() - surface pixmap texture is not VulkanSurfaceTextureX11";
            }
        }
    }

    // Build geometry from quads
    for (const WindowQuad &quad : quads) {
        const QPointF p0 = matrix.map(QPointF(quad[0].x(), quad[0].y()));
        const QPointF p1 = matrix.map(QPointF(quad[1].x(), quad[1].y()));
        const QPointF p2 = matrix.map(QPointF(quad[2].x(), quad[2].y()));
        const QPointF p3 = matrix.map(QPointF(quad[3].x(), quad[3].y()));

        // Triangle 1: p0, p1, p2
        node.geometry.append({
            .position = QVector2D(p0.x(), p0.y()),
            .texcoord = QVector2D(quad[0].u(), quad[0].v()),
        });
        node.geometry.append({
            .position = QVector2D(p1.x(), p1.y()),
            .texcoord = QVector2D(quad[1].u(), quad[1].v()),
        });
        node.geometry.append({
            .position = QVector2D(p2.x(), p2.y()),
            .texcoord = QVector2D(quad[2].u(), quad[2].v()),
        });

        // Triangle 2: p0, p2, p3
        node.geometry.append({
            .position = QVector2D(p0.x(), p0.y()),
            .texcoord = QVector2D(quad[0].u(), quad[0].v()),
        });
        node.geometry.append({
            .position = QVector2D(p2.x(), p2.y()),
            .texcoord = QVector2D(quad[2].u(), quad[2].v()),
        });
        node.geometry.append({
            .position = QVector2D(p3.x(), p3.y()),
            .texcoord = QVector2D(quad[3].u(), quad[3].v()),
        });
    }

    node.vertexCount = node.geometry.count();
    context->renderNodes.append(node);

    // Process children
    for (Item *child : children) {
        context->transformStack.push(matrix * child->transform());
        context->opacityStack.push(opacity * child->opacity());

        // Handle window item corners
        if (qobject_cast<WindowItem *>(child) && !child->borderRadius().isNull()) {
            RenderCorner corner;
            corner.box = child->rect();
            corner.radius = child->borderRadius();
            context->cornerStack.push(corner);
            createRenderNode(child, context);
            context->cornerStack.pop();
        } else {
            createRenderNode(child, context);
        }

        context->transformStack.pop();
        context->opacityStack.pop();
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
        memcpy(pc.mvp, mvp.data(), sizeof(pc.mvp));
        memset(pc.textureMatrix, 0, sizeof(pc.textureMatrix));
        pc.textureMatrix[0] = 1.0f;
        pc.textureMatrix[5] = 1.0f;
        pc.textureMatrix[10] = 1.0f;
        pc.textureMatrix[15] = 1.0f;

        if (!node.textures.isEmpty() && node.textures[0]) {
            const QMatrix4x4 &textureMatrix = node.textures[0]->matrix();
            memcpy(pc.textureMatrix, textureMatrix.data(), sizeof(pc.textureMatrix));
        }

        vkCmdPushConstants(cmd, pipeline->layout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(VulkanPushConstants), &pc);

        // Update and bind descriptor sets for textures
        if (!node.textures.isEmpty() && node.textures[0]) {
            VkDescriptorSet descriptorSet = m_context->allocateDescriptorSet(pipeline->descriptorSetLayout());
            if (descriptorSet == VK_NULL_HANDLE) {
                qCWarning(KWIN_CORE) << "Failed to allocate descriptor set, skipping texture binding";
                continue;
            }

            // Validate descriptor set before using it
            if (descriptorSet != VK_NULL_HANDLE) {
                // Update descriptor set with texture
                VkDescriptorImageInfo imageInfo{};
                imageInfo.sampler = node.textures[0]->sampler();
                imageInfo.imageView = node.textures[0]->imageView();
                if (imageInfo.imageView == VK_NULL_HANDLE) {
                    qCWarning(KWIN_CORE) << "Texture has null image view, skipping descriptor update";
                    continue;
                }

                // Validate image view before using it
                if (imageInfo.imageView == VK_NULL_HANDLE) {
                    qCWarning(KWIN_CORE) << "Descriptor set image view is null, skipping update";
                    continue;
                }
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                VkWriteDescriptorSet descriptorWrite{};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = descriptorSet;
                descriptorWrite.dstBinding = 0;
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pImageInfo = &imageInfo;

                vkUpdateDescriptorSets(m_backend->device(), 1, &descriptorWrite, 0, nullptr);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        pipeline->layout(), 0, 1, &descriptorSet, 0, nullptr);
                qCDebug(KWIN_CORE) << "Bound descriptor set with texture";
            } else {
                qCWarning(KWIN_CORE) << "Failed to allocate descriptor set, skipping texture binding";
            }
        }

        // Update uniform buffer for fragment shader parameters
        VulkanUniforms uniforms{};
        const QVector4D modulation = modulate(node.opacity, 1.0f);
        memcpy(uniforms.uniformColor, &modulation, sizeof(uniforms.uniformColor));
        uniforms.opacity = node.opacity;
        uniforms.saturation = 1.0f;
        memcpy(uniforms.geometryBox, &node.box, sizeof(uniforms.geometryBox));
        memcpy(uniforms.borderRadius, &node.borderRadius, sizeof(uniforms.borderRadius));
        uniforms.borderThickness = node.borderThickness;
        const QVector4D borderColor(node.borderColor.redF(), node.borderColor.greenF(),
                                    node.borderColor.blueF(), node.borderColor.alphaF());
        memcpy(uniforms.borderColor, &borderColor, sizeof(uniforms.borderColor));

        // For now, use push constants for basic rendering
        // A complete implementation would use uniform buffers

        // Draw
        if (node.vertexCount > 0) {
            vkCmdDraw(cmd, node.vertexCount, 1, node.firstVertex, 0);
            qCDebug(KWIN_CORE) << "Drew" << node.vertexCount << "vertices";
        } else {
            qCDebug(KWIN_CORE) << "Skipped draw call with 0 vertices";
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

    // Initialize stacks
    context.transformStack.push(context.rootTransform);
    context.opacityStack.push(data.opacity());

    // Build render nodes from item tree
    createRenderNode(item, &context);

    // Set scissor for clipping if needed
    if (context.hardwareClipping && !region.isEmpty()) {
        const QRect clipRect = region.boundingRect();
        VkRect2D scissor{};
        scissor.offset = {clipRect.x(), clipRect.y()};
        scissor.extent = {static_cast<uint32_t>(clipRect.width()), static_cast<uint32_t>(clipRect.height())};
        vkCmdSetScissor(m_currentCommandBuffer, 0, 1, &scissor);
    }

    // Render all nodes
    renderNodes(context, m_currentCommandBuffer);

    qCDebug(KWIN_CORE) << "ItemRendererVulkan::renderItem() - rendered" << context.renderNodes.size() << "nodes for item:" << item;
}

} // namespace KWin
