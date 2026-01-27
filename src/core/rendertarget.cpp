/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "core/rendertarget.h"
#include "opengl/glutils.h"
#include "platformsupport/scenes/vulkan/vulkanrendertarget.h"

namespace KWin
{

RenderTarget::RenderTarget(GLFramebuffer *fbo, const ColorDescription &colorDescription)
    : m_framebuffer(fbo)
    , m_transform(fbo->colorAttachment() ? fbo->colorAttachment()->contentTransform() : OutputTransform())
    , m_colorDescription(colorDescription)
{
}

RenderTarget::RenderTarget(QImage *image, const ColorDescription &colorDescription)
    : m_image(image)
    , m_colorDescription(colorDescription)
{
}

RenderTarget::RenderTarget(VulkanRenderTarget *vulkanTarget, const ColorDescription &colorDescription)
    : m_vulkanTarget(vulkanTarget)
    , m_colorDescription(colorDescription)
{
}

QSize RenderTarget::size() const
{
    if (m_framebuffer) {
        return m_framebuffer->size();
    } else if (m_image) {
        return m_image->size();
    } else if (m_vulkanTarget.has_value()) {
        // For Vulkan, we need to get the size from the VulkanRenderTarget
        VulkanRenderTarget *vulkanTarget = std::any_cast<VulkanRenderTarget *>(m_vulkanTarget);
        return vulkanTarget->size();
    } else {
        Q_UNREACHABLE();
    }
}

OutputTransform RenderTarget::transform() const
{
    return m_transform;
}

GLFramebuffer *RenderTarget::framebuffer() const
{
    return m_framebuffer;
}

GLTexture *RenderTarget::texture() const
{
    return m_framebuffer->colorAttachment();
}

QImage *RenderTarget::image() const
{
    return m_image;
}

const ColorDescription &RenderTarget::colorDescription() const
{
    return m_colorDescription;
}

VulkanRenderTarget *RenderTarget::vulkanTarget() const
{
    if (m_vulkanTarget.has_value()) {
        return std::any_cast<VulkanRenderTarget *>(m_vulkanTarget);
    }
    return nullptr;
}

} // namespace KWin
