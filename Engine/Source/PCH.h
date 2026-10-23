/**
 * @file PCH.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

// Precompiled header for the DirectAttack engine.
//
// Only put headers here that are (a) included in many files and (b) rarely change.
// Do NOT add engine headers (Log.h, Window.h, ...) or GLFW/Vulkan headers: every
// change to anything in here recompiles the whole engine.
//
// CMake force-includes this file into every engine source file (/FI on MSVC),
// so .cpp files do not need to #include "PCH.h" themselves.

// Standard library
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// Engine
#include <Core/Log.h>