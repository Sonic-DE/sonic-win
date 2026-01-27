/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "vulkanpipelinemanager.h"
#include "utils/common.h"
#include "vulkancontext.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>

namespace KWin
{

VulkanPipelineManager::VulkanPipelineManager(VulkanContext *context)
    : m_context(context)
{
    loadShaders();
}

VulkanPipelineManager::~VulkanPipelineManager()
{
    clearCache();
}

bool VulkanPipelineManager::loadShaders()
{
    // Try to load pre-compiled SPIR-V shaders
    // These should be compiled at build time using glslc

    // Look for shaders in standard locations
    QStringList searchPaths = {
        QStringLiteral(":/shaders/vulkan/"),
        QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kwin/shaders/vulkan"), QStandardPaths::LocateDirectory),
        QStringLiteral("/usr/share/kwin/shaders/vulkan/"),
        QStringLiteral("/usr/local/share/kwin/shaders/vulkan/"),
    };

    // Add paths relative to the executable for development builds
    QString appDir = QCoreApplication::applicationDirPath();
    searchPaths.prepend(appDir + QStringLiteral("/../shaders/vulkan/"));
    searchPaths.prepend(appDir + QStringLiteral("/shaders/vulkan/"));

    QString vertPath;
    QString fragPath;

    for (const QString &basePath : searchPaths) {
        if (basePath.isEmpty()) {
            continue;
        }

        QString base = basePath;
        if (!base.endsWith(QLatin1Char('/'))) {
            base += QLatin1Char('/');
        }

        QString vPath = base + QStringLiteral("basic.vert.spv");
        QString fPath = base + QStringLiteral("main.frag.spv");

        if (QFile::exists(vPath) && QFile::exists(fPath)) {
            vertPath = vPath;
            fragPath = fPath;
            qCDebug(KWIN_CORE) << "Found Vulkan shaders at:" << base;
            break;
        }
    }

    if (vertPath.isEmpty() || fragPath.isEmpty()) {
        qCWarning(KWIN_CORE) << "Could not find Vulkan SPIR-V shaders in any of:" << searchPaths;
        // Try to provide more specific error information
        for (const QString &basePath : searchPaths) {
            if (basePath.isEmpty()) {
                continue;
            }

            QString base = basePath;
            if (!base.endsWith(QLatin1Char('/'))) {
                base += QLatin1Char('/');
            }

            QString vPath = base + QStringLiteral("basic.vert.spv");
            QString fPath = base + QStringLiteral("main.frag.spv");

            if (!QFile::exists(vPath)) {
                qCWarning(KWIN_CORE) << "Vertex shader not found:" << vPath;
            }
            if (!QFile::exists(fPath)) {
                qCWarning(KWIN_CORE) << "Fragment shader not found:" << fPath;
            }
        }
        m_shadersLoaded = false;
        return false;
    }

    QFile vertFile(vertPath);
    if (!vertFile.open(QIODevice::ReadOnly)) {
        qCWarning(KWIN_CORE) << "Failed to open vertex shader:" << vertPath << "Error:" << vertFile.errorString();
        return false;
    }
    m_vertexShaderSpirv = vertFile.readAll();
    if (m_vertexShaderSpirv.isEmpty()) {
        qCWarning(KWIN_CORE) << "Vertex shader file is empty:" << vertPath;
        vertFile.close();
        return false;
    }
    vertFile.close();

    QFile fragFile(fragPath);
    if (!fragFile.open(QIODevice::ReadOnly)) {
        qCWarning(KWIN_CORE) << "Failed to open fragment shader:" << fragPath << "Error:" << fragFile.errorString();
        return false;
    }
    m_fragmentShaderSpirv = fragFile.readAll();
    if (m_fragmentShaderSpirv.isEmpty()) {
        qCWarning(KWIN_CORE) << "Fragment shader file is empty:" << fragPath;
        fragFile.close();
        return false;
    }
    fragFile.close();

    m_shadersLoaded = true;
    qCDebug(KWIN_CORE) << "Vulkan shaders loaded successfully";
    return true;
}

void VulkanPipelineManager::setRenderPass(VkRenderPass renderPass)
{
    if (m_renderPass != renderPass) {
        // Render pass changed, need to recreate all pipelines
        clearCache();
        m_renderPass = renderPass;
    }
}

VulkanPipeline *VulkanPipelineManager::pipeline(ShaderTraits traits)
{
    if (m_renderPass == VK_NULL_HANDLE) {
        qCWarning(KWIN_CORE) << "Cannot get pipeline: render pass not set";
        return nullptr;
    }

    if (!m_shadersLoaded) {
        qCWarning(KWIN_CORE) << "Cannot get pipeline: shaders not loaded";
        return nullptr;
    }

    // Check cache
    auto it = m_pipelines.find(traits);
    if (it != m_pipelines.end()) {
        return it->second.get();
    }

    // Create new pipeline
    auto newPipeline = VulkanPipeline::create(m_context, m_renderPass, traits,
                                              m_vertexShaderSpirv, m_fragmentShaderSpirv);
    if (!newPipeline) {
        qCWarning(KWIN_CORE) << "Failed to create pipeline for traits:" << static_cast<int>(traits);
        // Try to create a fallback pipeline with minimal traits
        ShaderTraits fallbackTraits = ShaderTrait::MapTexture;
        if (traits & ShaderTrait::UniformColor) {
            fallbackTraits |= ShaderTrait::UniformColor;
        }
        // Only try fallback if it's different from the requested traits
        if (fallbackTraits != traits) {
            qCDebug(KWIN_CORE) << "Trying fallback pipeline with traits:" << static_cast<int>(fallbackTraits);
            auto fallbackPipeline = VulkanPipeline::create(m_context, m_renderPass, fallbackTraits,
                                                           m_vertexShaderSpirv, m_fragmentShaderSpirv);
            if (fallbackPipeline) {
                VulkanPipeline *result = fallbackPipeline.get();
                m_pipelines[traits] = std::move(fallbackPipeline);
                qCDebug(KWIN_CORE) << "Created fallback Vulkan pipeline for traits:" << static_cast<int>(traits);
                return result;
            }
        }
        return nullptr;
    }

    VulkanPipeline *result = newPipeline.get();
    m_pipelines[traits] = std::move(newPipeline);

    qCDebug(KWIN_CORE) << "Created Vulkan pipeline for traits:" << static_cast<int>(traits);
    return result;
}

void VulkanPipelineManager::pushPipeline(VulkanPipeline *pipeline)
{
    m_pipelineStack.push(pipeline);
}

VulkanPipeline *VulkanPipelineManager::popPipeline()
{
    if (m_pipelineStack.isEmpty()) {
        return nullptr;
    }
    return m_pipelineStack.pop();
}

VulkanPipeline *VulkanPipelineManager::currentPipeline() const
{
    if (m_pipelineStack.isEmpty()) {
        return nullptr;
    }
    return m_pipelineStack.top();
}

void VulkanPipelineManager::clearCache()
{
    m_pipelines.clear();
    m_pipelineStack.clear();
}

// VulkanPipelineBinder implementation

VulkanPipelineBinder::VulkanPipelineBinder(VulkanPipelineManager *manager, ShaderTraits traits)
    : m_manager(manager)
    , m_pipeline(manager->pipeline(traits))
{
    if (m_pipeline) {
        m_manager->pushPipeline(m_pipeline);
    }
}

VulkanPipelineBinder::~VulkanPipelineBinder()
{
    if (m_pipeline) {
        m_manager->popPipeline();
    }
}

} // namespace KWin
