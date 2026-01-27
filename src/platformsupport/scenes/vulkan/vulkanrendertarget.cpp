/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkanrendertarget.h"
#include "platformsupport/scenes/vulkan/vulkanframebuffer.h"
#include "platformsupport/scenes/vulkan/vulkantexture.h"
#include "utils/common.h"

namespace KWin
{

VulkanRenderTarget::VulkanRenderTarget(VulkanFramebuffer *framebuffer, const ColorDescription &colorDescription)
    : m_framebuffer(framebuffer)
    , m_transform(OutputTransform::Normal)
    , m_colorDescription(colorDescription)
{
}

VulkanRenderTarget::VulkanRenderTarget(VulkanTexture *texture, const ColorDescription &colorDescription)
    : m_texture(texture)
    , m_transform(OutputTransform::Normal)
    , m_colorDescription(colorDescription)
{
}

VulkanRenderTarget::VulkanRenderTarget(QImage *image, const ColorDescription &colorDescription)
    : m_image(image)
    , m_transform(OutputTransform::Normal)
    , m_colorDescription(colorDescription)
{
}

QSize VulkanRenderTarget::size() const
{
    if (m_framebuffer) {
        return m_framebuffer->size();
    }
    if (m_texture) {
        return m_texture->size();
    }
    if (m_image) {
        return m_image->size();
    }
    return QSize();
}

OutputTransform VulkanRenderTarget::transform() const
{
    return m_transform;
}

const ColorDescription &VulkanRenderTarget::colorDescription() const
{
    return m_colorDescription;
}

QImage *VulkanRenderTarget::image() const
{
    return m_image;
}

VulkanFramebuffer *VulkanRenderTarget::framebuffer() const
{
    return m_framebuffer;
}

VulkanTexture *VulkanRenderTarget::texture() const
{
    return m_texture;
}

void VulkanRenderTarget::setSyncInfo(const VulkanSyncInfo &syncInfo)
{
    m_syncInfo = syncInfo;
    qCDebug(KWIN_CORE) << "Set sync info - imageAvailableSemaphore:" << syncInfo.imageAvailableSemaphore
                       << "renderFinishedSemaphore:" << syncInfo.renderFinishedSemaphore
                       << "inFlightFence:" << syncInfo.inFlightFence;
}

const VulkanSyncInfo &VulkanRenderTarget::syncInfo() const
{
    qCDebug(KWIN_CORE) << "Getting sync info - imageAvailableSemaphore:" << m_syncInfo.imageAvailableSemaphore
                       << "renderFinishedSemaphore:" << m_syncInfo.renderFinishedSemaphore
                       << "inFlightFence:" << m_syncInfo.inFlightFence;
    return m_syncInfo;
}

bool VulkanRenderTarget::hasSyncInfo() const
{
    return m_syncInfo.imageAvailableSemaphore != VK_NULL_HANDLE || m_syncInfo.renderFinishedSemaphore != VK_NULL_HANDLE;
}

} // namespace KWin
