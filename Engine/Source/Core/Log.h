/**
 * @file Log.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "DA_API.h"
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

#include <string_view>
#include <utility>
#include <vector>

namespace DA {
    enum class ELogLevel : unsigned char {
        ELL_TRACE,
        ELL_DEBUG,
        ELL_INFO,
        ELL_WARNING,
        ELL_ERROR,
        ELL_FATAL
    };

    class DA_API Log {
    public:
        static void Initialize();
        static void Shutdown();

        static void Write(ELogLevel level, std::string_view category, std::string_view message);
        static void Initialize(const std::vector<spdlog::sink_ptr>& customSinks);

        template <typename... Args>
        static void Print(ELogLevel level, std::string_view category, fmt::format_string<Args...> format,
                          Args&&... args) {
            Write(level, category, fmt::format(format, std::forward<Args>(args)...));
        }

        Log() = delete;
    };
} // namespace DA

// Logging Macros

// Default category for the module being compiled. Each target overrides this in CMake.
#ifndef DA_LOG_CATEGORY
#define DA_LOG_CATEGORY Core
#endif

// Two-step stringify so a macro (DA_LOG_CATEGORY) is expanded BEFORE it becomes a string.
#define DA_STRINGIFY_IMPL(x) #x
#define DA_STRINGIFY(x) DA_STRINGIFY_IMPL(x)

// clang-format off
#define DA_LOG(Category, Level, ...) \
    ::DA::Log::Print(::DA::ELogLevel::ELL_##Level, DA_STRINGIFY(Category), __VA_ARGS__)

#define DA_TRACE(...) DA_LOG(DA_LOG_CATEGORY, TRACE,   __VA_ARGS__)
#define DA_DEBUG(...) DA_LOG(DA_LOG_CATEGORY, DEBUG,   __VA_ARGS__)
#define DA_INFO(...)  DA_LOG(DA_LOG_CATEGORY, INFO,    __VA_ARGS__)
#define DA_WARN(...)  DA_LOG(DA_LOG_CATEGORY, WARNING, __VA_ARGS__)
#define DA_ERROR(...) DA_LOG(DA_LOG_CATEGORY, ERROR,   __VA_ARGS__)
#define DA_FATAL(...) DA_LOG(DA_LOG_CATEGORY, FATAL,   __VA_ARGS__)
// clang-format on