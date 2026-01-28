/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/colorspace.h"
#include "core/syncobjtimeline.h"
#include "platformsupport/scenes/vulkan/vulkanpipeline.h"
#include "platformsupport/scenes/vulkan/vulkanrendertarget.h"
#include "scene/borderradius.h"
#include "scene/itemgeometry.h"
#include "scene/itemrenderer.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"

#include <QMatrix4x4>
#include <QStack>
#include <unordered_set>
#include <vulkan/vulkan.h>

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::ShaderTraits)

namespace KWin
{

class VulkanBackend;
class VulkanContext;
class VulkanBuffer;
class VulkanTexture;
class VulkanFramebuffer;
class VulkanSwapchain;
class VulkanRenderTarget;

class KWIN_EXPORT ItemRendererVulkan : public ItemRenderer
{
public:
    struct RenderNode
    {
        ShaderTraits traits;
        QVarLengthArray<VulkanTexture *, 4> textures;
        RenderGeometry geometry;
        QMatrix4x4 transformMatrix;
        int firstVertex = 0;
        int vertexCount = 0;
        qreal opacity = 1;
        bool hasAlpha = false;
        ColorDescription colorDescription = ColorDescription::sRGB;
        RenderingIntent renderingIntent = RenderingIntent::Perceptual;
        std::shared_ptr<SyncReleasePoint> bufferReleasePoint;
        QVector4D box;
        QVector4D borderRadius;
        int borderThickness = 0;
        QColor borderColor;
    };

    struct RenderCorner
    {
        QRectF box;
        BorderRadius radius;
    };

    struct RenderContext
    {
        QList<RenderNode> renderNodes;
        QStack<QMatrix4x4> transformStack;
        QStack<qreal> opacityStack;
        QStack<RenderCorner> cornerStack;
        const QMatrix4x4 projectionMatrix;
        const QMatrix4x4 rootTransform;
        const QRegion clip;
        const bool hardwareClipping;
        const qreal renderTargetScale;
    };

    explicit ItemRendererVulkan(VulkanBackend *backend);
    ~ItemRendererVulkan();

    std::unique_ptr<ImageItem> createImageItem(Item *parent = nullptr) override;

    void beginFrame(const RenderTarget &renderTarget, const RenderViewport &viewport) override;
    void endFrame() override;

    void renderBackground(const RenderTarget &renderTarget, const RenderViewport &viewport, const QRegion &region) override;
    void renderItem(const RenderTarget &renderTarget, const RenderViewport &viewport, Item *item, int mask, const QRegion &region, const WindowPaintData &data) override;

    VulkanBackend *backend() const
    {
        return m_backend;
    }
    VulkanContext *context() const
    {
        return m_context;
    }

private:
    QVector4D modulate(float opacity, float brightness) const;
    void createRenderNode(Item *item, RenderContext *context);
    void renderNodes(const RenderContext &context, VkCommandBuffer cmd);

    VulkanBackend *m_backend;
    VulkanContext *m_context;

    VkCommandBuffer m_currentCommandBuffer = VK_NULL_HANDLE;
    VulkanFramebuffer *m_currentFramebuffer = nullptr;
    QMatrix4x4 m_currentProjection;

    std::unique_ptr<VulkanBuffer> m_uniformBuffer;
    VkDescriptorSet m_currentDescriptorSet = VK_NULL_HANDLE;

    std::unordered_set<std::shared_ptr<SyncReleasePoint>> m_releasePoints;

    // GPU-GPU synchronization info for the current frame
    VulkanSyncInfo m_currentSyncInfo;
};

} // namespace KWin
