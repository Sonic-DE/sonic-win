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
}

bool VulkanSurfaceTexture::isValid() const
{
    return m_texture.isValid();
}

VulkanBackend *VulkanSurfaceTexture::backend() const
{
    return m_backend;
}

} // namespace KWin
