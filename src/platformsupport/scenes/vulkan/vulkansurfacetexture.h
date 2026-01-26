/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "scene/surfaceitem.h"
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanBackend;

/**
 * @brief Texture wrapper for Vulkan surface textures
 */
class KWIN_EXPORT VulkanSurfaceTexture : public SurfaceTexture
{
public:
    explicit VulkanSurfaceTexture(VulkanBackend *backend);
    ~VulkanSurfaceTexture() override;

    bool isValid() const override;

    VulkanBackend *backend() const;
    VkImage image() const;
    VkImageView imageView() const;

protected:
    VulkanBackend *m_backend;
    VkImage m_image = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
};

} // namespace KWin
