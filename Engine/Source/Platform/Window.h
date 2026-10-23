/**
 * @file Window.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <Core/DA_API.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace DA {
    struct WindowDesc {
        std::string Title = "DirectAttack";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool Resizeable = true;
    };

    // Platform-independent window interface. Implementations live in Platform/<Platform>/.
    class DA_API Window {
    public:
        // Event callbacks. Assign whatever you need; unset callbacks are ignored.
        std::function<void(uint32_t width, uint32_t height)> OnResize; // framebuffer size in pixels
        std::function<void()> OnClose;
        std::function<void(bool focused)> OnFocus;

        virtual ~Window() = default;

        // Creates the window for the current platform. Returns nullptr on failure.
        static std::unique_ptr<Window> Create(const WindowDesc& desc);

        // Instance extensions the windowing backend needs for surface creation. Call after a window exists.
        static std::vector<const char*> GetRequiredVulkanExtensions();

        virtual void PollEvents() = 0;
        virtual bool ShouldClose() const = 0;
        virtual void SetTitle(const std::string& title) = 0;

        // Framebuffer size in pixels (what a Vulkan swapchain needs). 0x0 when minimized.
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        // GLFWwindow* on the GLFW backend. Used later to create the Vulkan surface.
        virtual void* GetNativeHandle() const = 0;
    };
} // namespace DA
