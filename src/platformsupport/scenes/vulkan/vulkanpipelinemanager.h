/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "vulkanpipeline.h"

#include <QByteArray>
#include <QStack>
#include <map>
#include <memory>

namespace KWin
{

class VulkanContext;

/**
 * @brief Manages Vulkan pipelines with trait-based caching.
 *
 * Similar to OpenGL's ShaderManager, this class caches pipelines based on
 * shader traits to avoid redundant pipeline creation.
 */
class KWIN_EXPORT VulkanPipelineManager
{
public:
    explicit VulkanPipelineManager(VulkanContext *context);
    ~VulkanPipelineManager();

    /**
     * @brief Get or create a pipeline for the given traits.
     *
     * Pipelines are cached and reused for the same trait combinations.
     */
    VulkanPipeline *pipeline(ShaderTraits traits);

    /**
     * @brief Set the render pass to use for pipeline creation.
     *
     * Must be called before requesting pipelines.
     */
    void setRenderPass(VkRenderPass renderPass);

    /**
     * @brief Get the current render pass.
     */
    VkRenderPass renderPass() const
    {
        return m_renderPass;
    }

    /**
     * @brief Push a pipeline onto the binding stack.
     */
    void pushPipeline(VulkanPipeline *pipeline);

    /**
     * @brief Pop the top pipeline from the binding stack.
     */
    VulkanPipeline *popPipeline();

    /**
     * @brief Get the currently bound pipeline.
     */
    VulkanPipeline *currentPipeline() const;

    /**
     * @brief Clear all cached pipelines.
     *
     * Call this when the render pass changes.
     */
    void clearCache();

    /**
     * @brief Load compiled shader SPIR-V from resources or files.
     */
    bool loadShaders();

    /**
     * @brief Check if shaders are loaded.
     */
    bool shadersLoaded() const
    {
        return m_shadersLoaded;
    }

private:
    VulkanContext *m_context;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;

    std::map<ShaderTraits, std::unique_ptr<VulkanPipeline>> m_pipelines;
    QStack<VulkanPipeline *> m_pipelineStack;

    QByteArray m_vertexShaderSpirv;
    QByteArray m_fragmentShaderSpirv;
    bool m_shadersLoaded = false;
};

/**
 * @brief RAII helper for pipeline binding.
 */
class KWIN_EXPORT VulkanPipelineBinder
{
public:
    VulkanPipelineBinder(VulkanPipelineManager *manager, ShaderTraits traits);
    ~VulkanPipelineBinder();

    VulkanPipeline *pipeline() const
    {
        return m_pipeline;
    }

private:
    VulkanPipelineManager *m_manager;
    VulkanPipeline *m_pipeline;
};

} // namespace KWin
