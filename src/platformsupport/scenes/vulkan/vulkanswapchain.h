/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QSize>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanContext;
class VulkanBackend;
class VulkanRenderPass;
class VulkanFramebuffer;
class VulkanTexture;

/**
 * @brief Vulkan swapchain wrapper for presentation.
 *
 * Manages the swapchain, its images, framebuffers, and synchronization primitives.
 */
class KWIN_EXPORT VulkanSwapchain
{
public:
    /**
     * @brief Maximum number of frames in flight.
     */
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    ~VulkanSwapchain();

    // Non-copyable
    VulkanSwapchain(const VulkanSwapchain &) = delete;
    VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;

    /**
     * @brief Create a swapchain for the given surface.
     */
    static std::unique_ptr<VulkanSwapchain> create(VulkanContext *context, VkSurfaceKHR surface, const QSize &size);

    /**
     * @brief Check if the swapchain is valid.
     */
    bool isValid() const
    {
        return m_swapchain != VK_NULL_HANDLE;
    }

    /**
     * @brief Get the Vulkan swapchain handle.
     */
    VkSwapchainKHR swapchain() const
    {
        return m_swapchain;
    }

    /**
     * @brief Get the surface handle.
     */
    VkSurfaceKHR surface() const
    {
        return m_surface;
    }

    /**
     * @brief Get the swapchain image format.
     */
    VkFormat format() const
    {
        return m_format;
    }

    /**
     * @brief Get the swapchain extent.
     */
    VkExtent2D extent() const
    {
        return m_extent;
    }

    /**
     * @brief Get the swapchain size as QSize.
     */
    QSize size() const
    {
        return QSize(m_extent.width, m_extent.height);
    }

    /**
     * @brief Get the number of swapchain images.
     */
    uint32_t imageCount() const
    {
        return static_cast<uint32_t>(m_images.size());
    }

    /**
     * @brief Get the render pass for this swapchain.
     */
    VulkanRenderPass *renderPass() const
    {
        return m_renderPass.get();
    }

    /**
     * @brief Get a framebuffer for the given image index.
     */
    VulkanFramebuffer *framebuffer(uint32_t index) const;

    /**
     * @brief Acquire the next swapchain image.
     *
     * @param timeout Timeout in nanoseconds (UINT64_MAX for infinite).
     * @return The image index, or UINT32_MAX on failure.
     */
    uint32_t acquireNextImage(uint64_t timeout = UINT64_MAX);

    /**
     * @brief Get the current image index (after acquireNextImage).
     */
    uint32_t currentImageIndex() const
    {
        return m_currentImageIndex;
    }

    /**
     * @brief Get the current framebuffer.
     */
    VulkanFramebuffer *currentFramebuffer() const;

    /**
     * @brief Present the current image.
     *
     * @return True if presentation succeeded.
     */
    bool present();

    /**
     * @brief Recreate the swapchain (e.g., after resize).
     */
    bool recreate(const QSize &newSize);

    /**
     * @brief Check if the swapchain needs to be recreated.
     */
    bool needsRecreation() const
    {
        return m_needsRecreation;
    }

    /**
     * @brief Get the semaphore signaled when an image is available.
     */
    VkSemaphore imageAvailableSemaphore() const;

    /**
     * @brief Get the semaphore to signal when rendering is finished.
     */
    VkSemaphore renderFinishedSemaphore() const;

    /**
     * @brief Get the fence for the current frame.
     */
    VkFence inFlightFence() const;

    /**
     * @brief Wait for the in-flight fence.
     */
    void waitForFence();

    /**
     * @brief Reset the in-flight fence.
     */
    void resetFence();

    /**
     * @brief Get the current frame index (0 to MAX_FRAMES_IN_FLIGHT-1).
     */
    uint32_t currentFrame() const
    {
        return m_currentFrame;
    }

    /**
     * @brief Advance to the next frame.
     */
    void advanceFrame();

private:
    VulkanSwapchain(VulkanContext *context, VkSurfaceKHR surface);

    bool create(const QSize &size);
    bool createSwapchain(const QSize &size);
    bool createImageViews();
    bool createRenderPass();
    bool createFramebuffers();
    bool createSyncObjects();
    void cleanup();
    void cleanupSwapchain();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, const QSize &requestedSize);

    VulkanContext *m_context;
    VkSurfaceKHR m_surface;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent = {0, 0};

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    std::unique_ptr<VulkanRenderPass> m_renderPass;
    std::vector<std::unique_ptr<VulkanFramebuffer>> m_framebuffers;

    // Synchronization
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_renderFinishedSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};

    uint32_t m_currentImageIndex = 0;
    uint32_t m_currentFrame = 0;
    bool m_needsRecreation = false;
};

} // namespace KWin
