/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "kwin_export.h"

#include <QByteArray>
#include <memory>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace KWin
{

class VulkanContext;
class VulkanBackend;

/**
 * @brief Shader traits matching OpenGL ShaderTraits for feature parity.
 */
enum class ShaderTrait {
    MapTexture = 1 << 0, ///< Sample from texture
    UniformColor = 1 << 1, ///< Use uniform color
    Modulate = 1 << 2, ///< Apply opacity/brightness modulation
    AdjustSaturation = 1 << 3, ///< Adjust color saturation
    TransformColorspace = 1 << 4, ///< Transform color space (HDR, etc.)
    RoundedCorners = 1 << 5, ///< Apply rounded corner clipping
    Border = 1 << 6, ///< Render border
};

Q_DECLARE_FLAGS(ShaderTraits, ShaderTrait)
Q_DECLARE_OPERATORS_FOR_FLAGS(ShaderTraits)

/**
 * @brief Vulkan graphics pipeline wrapper.
 *
 * Encapsulates a VkPipeline with its layout and descriptor set layouts.
 */
class KWIN_EXPORT VulkanPipeline
{
public:
    ~VulkanPipeline();

    // Non-copyable
    VulkanPipeline(const VulkanPipeline &) = delete;
    VulkanPipeline &operator=(const VulkanPipeline &) = delete;

    /**
     * @brief Create a pipeline for the given traits and render pass.
     */
    static std::unique_ptr<VulkanPipeline> create(VulkanContext *context, VkRenderPass renderPass,
                                                  ShaderTraits traits,
                                                  const QByteArray &vertexShaderSpirv,
                                                  const QByteArray &fragmentShaderSpirv);

    /**
     * @brief Check if the pipeline is valid.
     */
    bool isValid() const
    {
        return m_pipeline != VK_NULL_HANDLE;
    }

    /**
     * @brief Get the Vulkan pipeline handle.
     */
    VkPipeline pipeline() const
    {
        return m_pipeline;
    }

    /**
     * @brief Get the pipeline layout.
     */
    VkPipelineLayout layout() const
    {
        return m_layout;
    }

    /**
     * @brief Get the descriptor set layout.
     */
    VkDescriptorSetLayout descriptorSetLayout() const
    {
        return m_descriptorSetLayout;
    }

    /**
     * @brief Get the shader traits this pipeline was created for.
     */
    ShaderTraits traits() const
    {
        return m_traits;
    }

    /**
     * @brief Bind this pipeline to a command buffer.
     */
    void bind(VkCommandBuffer cmd) const;

private:
    VulkanPipeline(VulkanContext *context, ShaderTraits traits);

    bool createDescriptorSetLayout();
    bool createPipelineLayout();
    bool createPipeline(VkRenderPass renderPass,
                        const QByteArray &vertexShaderSpirv,
                        const QByteArray &fragmentShaderSpirv);
    VkShaderModule createShaderModule(const QByteArray &spirv);
    void cleanup();

    VulkanContext *m_context;
    ShaderTraits m_traits;

    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
};

/**
 * @brief Push constants structure matching the vertex shader.
 */
struct VulkanPushConstants
{
    float mvp[16]; // 4x4 matrix
    float textureMatrix[16]; // 4x4 matrix
};

/**
 * @brief Uniform buffer structure matching the fragment shader.
 */
struct VulkanUniforms
{
    float uniformColor[4];
    float opacity;
    float brightness;
    float saturation;
    float _pad1;
    float primaryBrightness[3];
    float _pad2;

    // Rounded corners
    float geometryBox[4]; // x, y, width, height
    float borderRadius[4]; // topLeft, topRight, bottomRight, bottomLeft

    // Border
    float borderThickness;
    float _pad3[3];
    float borderColor[4];

    // Color management
    int sourceTransferFunction;
    float _pad4[3];
    float sourceTransferParams[2];
    float _pad5[2];
    int destTransferFunction;
    float _pad6[3];
    float destTransferParams[2];
    float _pad7[2];
    float colorimetryTransform[16];
    float sourceReferenceLuminance;
    float maxTonemappingLuminance;
    float destReferenceLuminance;
    float maxDestLuminance;
    float destToLMS[16];
    float lmsToDest[16];
};

} // namespace KWin
