/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkanswapchain.h"
#include "utils/common.h"
#include "vulkanbackend.h"
#include "vulkancontext.h"
#include "vulkanframebuffer.h"
#include "vulkanrenderpass.h"

#include <QDebug>
#include <algorithm>
#include <limits>

namespace KWin
{

static QString getVulkanResultString(VkResult result)
{
    switch (result) {
    case VK_SUCCESS:
        return QStringLiteral("VK_SUCCESS");
    case VK_NOT_READY:
        return QStringLiteral("VK_NOT_READY");
    case VK_TIMEOUT:
        return QStringLiteral("VK_TIMEOUT");
    case VK_EVENT_SET:
        return QStringLiteral("VK_EVENT_SET");
    case VK_EVENT_RESET:
        return QStringLiteral("VK_EVENT_RESET");
    case VK_INCOMPLETE:
        return QStringLiteral("VK_INCOMPLETE");
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return QStringLiteral("VK_ERROR_OUT_OF_HOST_MEMORY");
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return QStringLiteral("VK_ERROR_OUT_OF_DEVICE_MEMORY");
    case VK_ERROR_INITIALIZATION_FAILED:
        return QStringLiteral("VK_ERROR_INITIALIZATION_FAILED");
    case VK_ERROR_DEVICE_LOST:
        return QStringLiteral("VK_ERROR_DEVICE_LOST");
    case VK_ERROR_MEMORY_MAP_FAILED:
        return QStringLiteral("VK_ERROR_MEMORY_MAP_FAILED");
    case VK_ERROR_LAYER_NOT_PRESENT:
        return QStringLiteral("VK_ERROR_LAYER_NOT_PRESENT");
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return QStringLiteral("VK_ERROR_EXTENSION_NOT_PRESENT");
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return QStringLiteral("VK_ERROR_FEATURE_NOT_PRESENT");
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return QStringLiteral("VK_ERROR_INCOMPATIBLE_DRIVER");
    case VK_ERROR_TOO_MANY_OBJECTS:
        return QStringLiteral("VK_ERROR_TOO_MANY_OBJECTS");
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return QStringLiteral("VK_ERROR_FORMAT_NOT_SUPPORTED");
    case VK_ERROR_FRAGMENTED_POOL:
        return QStringLiteral("VK_ERROR_FRAGMENTED_POOL");
    case VK_ERROR_UNKNOWN:
        return QStringLiteral("VK_ERROR_UNKNOWN");
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return QStringLiteral("VK_ERROR_OUT_OF_POOL_MEMORY");
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return QStringLiteral("VK_ERROR_INVALID_EXTERNAL_HANDLE");
    case VK_ERROR_FRAGMENTATION:
        return QStringLiteral("VK_ERROR_FRAGMENTATION");
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return QStringLiteral("VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS");
    case VK_ERROR_SURFACE_LOST_KHR:
        return QStringLiteral("VK_ERROR_SURFACE_LOST_KHR");
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return QStringLiteral("VK_ERROR_NATIVE_WINDOW_IN_USE_KHR");
    case VK_SUBOPTIMAL_KHR:
        return QStringLiteral("VK_SUBOPTIMAL_KHR");
    case VK_ERROR_OUT_OF_DATE_KHR:
        return QStringLiteral("VK_ERROR_OUT_OF_DATE_KHR");
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return QStringLiteral("VK_ERROR_INCOMPATIBLE_DISPLAY_KHR");
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return QStringLiteral("VK_ERROR_VALIDATION_FAILED_EXT");
    case VK_ERROR_INVALID_SHADER_NV:
        return QStringLiteral("VK_ERROR_INVALID_SHADER_NV");
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return QStringLiteral("VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT");
    case VK_ERROR_NOT_PERMITTED_EXT:
        return QStringLiteral("VK_ERROR_NOT_PERMITTED_EXT");
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return QStringLiteral("VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT");
    case VK_THREAD_IDLE_KHR:
        return QStringLiteral("VK_THREAD_IDLE_KHR");
    case VK_THREAD_DONE_KHR:
        return QStringLiteral("VK_THREAD_DONE_KHR");
    case VK_OPERATION_DEFERRED_KHR:
        return QStringLiteral("VK_OPERATION_DEFERRED_KHR");
    case VK_OPERATION_NOT_DEFERRED_KHR:
        return QStringLiteral("VK_OPERATION_NOT_DEFERRED_KHR");
    case VK_PIPELINE_COMPILE_REQUIRED_EXT:
        return QStringLiteral("VK_PIPELINE_COMPILE_REQUIRED_EXT");
    default:
        return QString("Unknown VkResult: %1").arg(result);
    }
}

VulkanSwapchain::VulkanSwapchain(VulkanContext *context, VkSurfaceKHR surface)
    : m_context(context)
    , m_surface(surface)
{
}

VulkanSwapchain::~VulkanSwapchain()
{
    cleanup();
}

std::unique_ptr<VulkanSwapchain> VulkanSwapchain::create(VulkanContext *context, VkSurfaceKHR surface, const QSize &size)
{
    auto swapchain = std::unique_ptr<VulkanSwapchain>(new VulkanSwapchain(context, surface));

    if (!swapchain->create(size)) {
        return nullptr;
    }

    return swapchain;
}

bool VulkanSwapchain::create(const QSize &size)
{
    if (!createSwapchain(size)) {
        return false;
    }

    if (!createImageViews()) {
        cleanupSwapchain();
        return false;
    }

    if (!createRenderPass()) {
        cleanupSwapchain();
        return false;
    }

    if (!createFramebuffers()) {
        cleanupSwapchain();
        return false;
    }

    if (!createSyncObjects()) {
        cleanup(); // Full cleanup including sync objects
        return false;
    }

    return true;
}

void VulkanSwapchain::cleanup()
{
    VkDevice device = m_context->backend()->device();
    if (device == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(device);

    // Destroy sync objects
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
        }
        if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, m_imageAvailableSemaphores[i], nullptr);
        }
        if (m_inFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device, m_inFlightFences[i], nullptr);
        }
    }

    cleanupSwapchain();
}

void VulkanSwapchain::cleanupSwapchain()
{
    VkDevice device = m_context->backend()->device();

    m_framebuffers.clear();
    m_renderPass.reset();

    for (VkImageView imageView : m_imageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    // Prefer BGRA8 with sRGB color space
    for (const auto &format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // Fall back to BGRA8 UNORM
    for (const auto &format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            return format;
        }
    }

    // Use first available format
    return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    // Prefer mailbox (triple buffering) for smooth vsync
    for (const auto &mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }

    // FIFO is always available (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, const QSize &requestedSize)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(requestedSize.width()),
        static_cast<uint32_t>(requestedSize.height())};

    actualExtent.width = std::clamp(actualExtent.width,
                                    capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                     capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);

    return actualExtent;
}

bool VulkanSwapchain::createSwapchain(const QSize &size)
{
    VkPhysicalDevice physicalDevice = m_context->backend()->physicalDevice();
    VkDevice device = m_context->backend()->device();

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &capabilities);

    // Query surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, formats.data());

    // Query present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, presentModes.data());

    if (formats.empty() || presentModes.empty()) {
        qCWarning(KWIN_CORE) << "Surface doesn't support any formats or present modes";
        return false;
    }

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    VkExtent2D extent = chooseSwapExtent(capabilities, size);

    // Request one more image than minimum for triple buffering
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain);
    if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to create swapchain:" << result;
        return false;
    }

    m_format = surfaceFormat.format;
    m_extent = extent;

    // Get swapchain images
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &imageCount, m_images.data());

    qCDebug(KWIN_CORE) << "Created swapchain with" << imageCount << "images," << extent.width << "x" << extent.height << "format:" << surfaceFormat.format;
    return true;
}

bool VulkanSwapchain::createImageViews()
{
    VkDevice device = m_context->backend()->device();

    m_imageViews.resize(m_images.size(), VK_NULL_HANDLE);

    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &m_imageViews[i]);
        if (result != VK_SUCCESS) {
            qCWarning(KWIN_CORE) << "Failed to create image view:" << result;
            // Cleanup already-created image views (cleanupSwapchain handles VK_NULL_HANDLE)
            return false;
        }
    }

    return true;
}

bool VulkanSwapchain::createRenderPass()
{
    m_renderPass = VulkanRenderPass::createForPresentation(m_context, m_format);
    return m_renderPass != nullptr;
}

bool VulkanSwapchain::createFramebuffers()
{
    m_framebuffers.resize(m_imageViews.size());

    QSize fbSize(m_extent.width, m_extent.height);

    for (size_t i = 0; i < m_imageViews.size(); i++) {
        m_framebuffers[i] = VulkanFramebuffer::create(m_context, m_renderPass.get(),
                                                      m_imageViews[i], fbSize);
        if (!m_framebuffers[i]) {
            qCWarning(KWIN_CORE) << "Failed to create framebuffer" << i;
            return false;
        }
    }

    return true;
}

bool VulkanSwapchain::createSyncObjects()
{
    VkDevice device = m_context->backend()->device();

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            qCWarning(KWIN_CORE) << "Failed to create synchronization objects";
            return false;
        }
    }

    return true;
}

VulkanFramebuffer *VulkanSwapchain::framebuffer(uint32_t index) const
{
    if (index < m_framebuffers.size()) {
        return m_framebuffers[index].get();
    }
    return nullptr;
}

VulkanFramebuffer *VulkanSwapchain::currentFramebuffer() const
{
    return framebuffer(m_currentImageIndex);
}

uint32_t VulkanSwapchain::acquireNextImage(uint64_t timeout)
{
    VkDevice device = m_context->backend()->device();

    VkResult result = vkAcquireNextImageKHR(device, m_swapchain, timeout,
                                            m_imageAvailableSemaphores[m_currentFrame],
                                            VK_NULL_HANDLE, &m_currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        qCDebug(KWIN_CORE) << "Swapchain out of date, needs recreation";
        m_needsRecreation = true;
        return std::numeric_limits<uint32_t>::max();
    } else if (result == VK_SUBOPTIMAL_KHR) {
        qCDebug(KWIN_CORE) << "Swapchain suboptimal, needs recreation";
        m_needsRecreation = true;
    } else if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to acquire swapchain image:" << result << "(" << getVulkanResultString(result) << ")";
        return std::numeric_limits<uint32_t>::max();
    }

    if (result == VK_SUBOPTIMAL_KHR) {
        m_needsRecreation = true;
    }

    return m_currentImageIndex;
}

bool VulkanSwapchain::present()
{
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &m_currentImageIndex;

    qCDebug(KWIN_CORE) << "Presenting swapchain image" << m_currentImageIndex << "with semaphore" << signalSemaphores[0];
    VkResult result = vkQueuePresentKHR(m_context->backend()->graphicsQueue(), &presentInfo);
    qCDebug(KWIN_CORE) << "vkQueuePresentKHR result:" << result << "(" << getVulkanResultString(result) << ")";

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        qCDebug(KWIN_CORE) << "Swapchain out of date after present, needs recreation";
        m_needsRecreation = true;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        qCDebug(KWIN_CORE) << "Swapchain suboptimal after present, needs recreation";
        m_needsRecreation = true;
    } else if (result != VK_SUCCESS) {
        qCWarning(KWIN_CORE) << "Failed to present swapchain image:" << result << "(" << getVulkanResultString(result) << ")";
        return false;
    } else {
        qCDebug(KWIN_CORE) << "Successfully presented swapchain image" << m_currentImageIndex;
    }

    return true;
}

bool VulkanSwapchain::recreate(const QSize &newSize)
{
    VkDevice device = m_context->backend()->device();
    vkDeviceWaitIdle(device);

    cleanupSwapchain();

    if (!createSwapchain(newSize)) {
        return false;
    }

    if (!createImageViews()) {
        return false;
    }

    if (!createRenderPass()) {
        return false;
    }

    if (!createFramebuffers()) {
        return false;
    }

    m_needsRecreation = false;
    return true;
}

VkSemaphore VulkanSwapchain::imageAvailableSemaphore() const
{
    return m_imageAvailableSemaphores[m_currentFrame];
}

VkSemaphore VulkanSwapchain::renderFinishedSemaphore() const
{
    return m_renderFinishedSemaphores[m_currentFrame];
}

VkFence VulkanSwapchain::inFlightFence() const
{
    return m_inFlightFences[m_currentFrame];
}

void VulkanSwapchain::waitForFence()
{
    VkDevice device = m_context->backend()->device();
    vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
}

void VulkanSwapchain::resetFence()
{
    VkDevice device = m_context->backend()->device();
    vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]);
}

void VulkanSwapchain::advanceFrame()
{
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

} // namespace KWin
