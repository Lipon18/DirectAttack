/**
 * @file DA_API.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

// clang-format off
#if defined(DA_SHARED)
    #if defined(_WIN32)
        #if defined(DA_BUILDING_LIBRARY)
            #define DA_API __declspec(dllexport)
        #else
            #define DA_API __declspec(dllimport)
        #endif
    #elif defined(__GNUC__) || defined(__clang__)
        #define DA_API __attribute__((visibility("default")))
    #else
        #define DA_API
    #endif
#else
    #define DA_API
#endif
// clang-format on
