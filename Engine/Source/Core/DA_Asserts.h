/**
 * @file DA_Asserts.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "Log.h"
#include <cstdlib>

// clang-format off
#if defined(_MSC_VER)
    #define DA_DEBUGBREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
    #define DA_DEBUGBREAK() __builtin_trap()
#else
    #define DA_DEBUGBREAK() std::abort()
#endif

#if !defined(DA_BUILD_RELEASE)
    #define DA_CHECK(expr)                                              \
        do {                                                            \
            if (!(expr)) {                                              \
                DA_ERROR("Assertion failed: {}", #expr);                \
                DA_DEBUGBREAK();                                        \
            }                                                           \
        } while (0)

    // Usage: DA_CHECKF(x > 0, "x must be positive, got {}", x);
    #define DA_CHECKF(expr, ...)                                        \
        do {                                                            \
            if (!(expr)) {                                              \
                DA_ERROR("Assertion failed: {}", #expr);                \
                DA_ERROR(__VA_ARGS__);                                  \
                DA_DEBUGBREAK();                                        \
            }                                                           \
        } while (0)
#else
    #define DA_CHECK(expr)          (void)(expr)
    #define DA_CHECKF(expr, ...)    (void)(expr)
#endif

#define DA_VERIFY(expr)         DA_CHECK(expr)
#define DA_VERIFYF(expr, ...)   DA_CHECKF(expr, __VA_ARGS__)
// clang-format on