/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Joseph <author@example.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

// VMA configuration - must be defined before including vk_mem_alloc.h
// This header should be included ONCE in a .cpp file with VMA_IMPLEMENTATION defined

#define VMA_VULKAN_VERSION 1002000 // Vulkan 1.2
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

// Optionally enable debug features in debug builds
#ifndef NDEBUG
#define VMA_DEBUG_LOG(format, ...)                            \
    do {                                                      \
        fprintf(stderr, "[VMA] " format "\n", ##__VA_ARGS__); \
    } while (0)
#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 1
#endif

// Suppress warnings from third-party VMA header
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "thirdparty/vk_mem_alloc.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
