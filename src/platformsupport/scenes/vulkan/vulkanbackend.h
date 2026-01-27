/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "core/renderbackend.h"

#include <QRegion>
#include <memory>
#include <vulkan/vulkan.h>

namespace KWin
{
class Output;
class VulkanBackend;
class VulkanContext;
class VulkanSwapchain;

/**
 * @brief The VulkanBackend creates and holds the Vulkan context and is responsible for rendering.
 *
 * The VulkanBackend is an abstract base class used by the scene system to abstract away the
 * differences between various Vulkan implementations.
 *
 * A concrete implementation has to create and release the Vulkan context in a way so that the
 * scene renderer does not have to care about it.
 *
 * This class handles Vulkan instance, device, and swapchain management.
 */
class KWIN_EXPORT VulkanBackend : public RenderBackend
{
    Q_OBJECT

public:
    VulkanBackend();
    virtual ~VulkanBackend();

    virtual void init() = 0;
    CompositingType compositingType() const override final;
    bool checkGraphicsReset() override final;

    virtual bool makeCurrent() = 0;
    virtual void doneCurrent() = 0;
    virtual VulkanContext *vulkanContext() const = 0;

    /**
     * @brief Whether the creation of the Backend failed.
     *
     * The scene should test whether the Backend got constructed correctly. If this method
     * returns @c true, the scene should not try to start the rendering.
     *
     * @return bool @c true if the creation of the Backend failed, @c false otherwise.
     */
    bool isFailed() const
    {
        return m_failed;
    }

    /**
     * @brief Get the Vulkan instance handle
     */
    VkInstance instance() const
    {
        return m_instance;
    }

    /**
     * @brief Get the physical device handle
     */
    VkPhysicalDevice physicalDevice() const
    {
        return m_physicalDevice;
    }

    /**
     * @brief Get the logical device handle
     */
    VkDevice device() const
    {
        return m_device;
    }

    /**
     * @brief Get the graphics queue family index
     */
    uint32_t graphicsQueueFamily() const
    {
        return m_graphicsQueueFamily;
    }

    /**
     * @brief Get the graphics queue handle
     */
    VkQueue graphicsQueue() const
    {
        return m_graphicsQueue;
    }

    /**
     * @brief Copy a region of pixels
     */
    void copyPixels(const QRegion &region, const QSize &screenSize);

    /**
     * @brief Check if external fence fd export is supported (VK_KHR_external_fence_fd)
     */
    bool supportsExternalFenceFd() const
    {
        return m_supportsExternalFenceFd;
    }

    /**
     * @brief Get the vkGetFenceFdKHR function pointer (only valid if supportsExternalFenceFd() returns true)
     */
    PFN_vkGetFenceFdKHR vkGetFenceFdKHR() const
    {
        return m_vkGetFenceFdKHR;
    }

protected:
    /**
     * @brief Sets the backend initialization to failed.
     *
     * This method should be called by the concrete subclass in case the initialization failed.
     * The given @p reason is logged as a warning.
     *
     * @param reason The reason why the initialization failed.
     */
    void setFailed(const QString &reason);

    /**
     * @brief Initialize the Vulkan instance
     */
    bool createInstance(const QList<const char *> &requiredExtensions);

    /**
     * @brief Select and initialize the physical device
     */
    bool selectPhysicalDevice();

    /**
     * @brief Create the logical device
     */
    bool createDevice(const QList<const char *> &requiredDeviceExtensions);

    /**
     * @brief Cleanup Vulkan resources
     */
    void cleanup();

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;

private:
    /**
     * @brief Whether the initialization failed, of course default to @c false.
     */
    bool m_failed = false;

    // Debug messenger for validation layers
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    // External fence fd support
    bool m_supportsExternalFenceFd = false;
    PFN_vkGetFenceFdKHR m_vkGetFenceFdKHR = nullptr;
};

} // namespace KWin
