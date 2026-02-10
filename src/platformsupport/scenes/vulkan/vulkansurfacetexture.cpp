/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkansurfacetexture.h"
#include "vulkanbackend.h"
#include "vulkancontext.h"

namespace KWin
{

VulkanSurfaceTexture::VulkanSurfaceTexture(VulkanBackend *backend)
    : m_backend(backend)
{
}

VulkanSurfaceTexture::~VulkanSurfaceTexture()
{
    if (m_backend && m_backend->device()) {
        // Use deferred destruction to avoid GPU resource in-use errors
        auto context = m_backend->vulkanContext();
        if (context) {
            context->queueImageAndViewForDestruction(m_imageView, m_image);
        } else {
            // Fallback to immediate destruction if context is not available
            if (m_imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_backend->device(), m_imageView, nullptr);
            }
            if (m_image != VK_NULL_HANDLE) {
                vkDestroyImage(m_backend->device(), m_image, nullptr);
            }
        }
        if (m_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_backend->device(), m_memory, nullptr);
        }
        m_imageView = VK_NULL_HANDLE;
        m_image = VK_NULL_HANDLE;
        m_memory = VK_NULL_HANDLE;
    }
}

bool VulkanSurfaceTexture::isValid() const
{
    return m_image != VK_NULL_HANDLE && m_imageView != VK_NULL_HANDLE;
}

VulkanBackend *VulkanSurfaceTexture::backend() const
{
    return m_backend;
}

VkImage VulkanSurfaceTexture::image() const
{
    return m_image;
}

VkImageView VulkanSurfaceTexture::imageView() const
{
    return m_imageView;
}

} // namespace KWin
