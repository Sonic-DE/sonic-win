/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkansurfacetexture.h"
#include "vulkanbackend.h"

namespace KWin
{

VulkanSurfaceTexture::VulkanSurfaceTexture(VulkanBackend *backend)
    : m_backend(backend)
{
}

VulkanSurfaceTexture::~VulkanSurfaceTexture()
{
    if (m_backend && m_backend->device()) {
        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_backend->device(), m_imageView, nullptr);
        }
        if (m_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_backend->device(), m_image, nullptr);
        }
        if (m_memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_backend->device(), m_memory, nullptr);
        }
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
