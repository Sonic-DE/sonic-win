/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include "core/colorspace.h"
#include "core/output.h"

#include <QImage>
#include <memory>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanFramebuffer;
class VulkanTexture;

/**
 * @brief Synchronization info for GPU-GPU semaphore-based synchronization.
 *
 * This struct carries the semaphores and fence needed for proper Vulkan
 * synchronization without CPU blocking.
 */
struct VulkanSyncInfo
{
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE; ///< Wait before rendering
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE; ///< Signal after rendering
    VkFence inFlightFence = VK_NULL_HANDLE; ///< Signal when command buffer completes
};

class KWIN_EXPORT VulkanRenderTarget
{
public:
    explicit VulkanRenderTarget(VulkanFramebuffer *framebuffer, const ColorDescription &colorDescription = ColorDescription::sRGB);
    explicit VulkanRenderTarget(VulkanTexture *texture, const ColorDescription &colorDescription = ColorDescription::sRGB);
    explicit VulkanRenderTarget(QImage *image, const ColorDescription &colorDescription = ColorDescription::sRGB);

    QSize size() const;
    OutputTransform transform() const;
    const ColorDescription &colorDescription() const;

    QImage *image() const;
    VulkanFramebuffer *framebuffer() const;
    VulkanTexture *texture() const;

    /**
     * @brief Set synchronization info for GPU-GPU sync.
     *
     * When set, the renderer should:
     * - Wait on imageAvailableSemaphore before writing to the framebuffer
     * - Signal renderFinishedSemaphore when rendering is complete
     * - Signal inFlightFence when the command buffer has been submitted
     */
    void setSyncInfo(const VulkanSyncInfo &syncInfo);

    /**
     * @brief Get the synchronization info.
     */
    const VulkanSyncInfo &syncInfo() const;

    /**
     * @brief Check if GPU-GPU synchronization is available.
     */
    bool hasSyncInfo() const;

private:
    QImage *m_image = nullptr;
    VulkanFramebuffer *m_framebuffer = nullptr;
    VulkanTexture *m_texture = nullptr;
    const OutputTransform m_transform;
    const ColorDescription m_colorDescription;
    VulkanSyncInfo m_syncInfo;
};

} // namespace KWin
