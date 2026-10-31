/**
 * @file Win32Window.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <Platform/Window.h>

struct GLFWwindow;

namespace DA {
    class Win32Window final : public Window {
    public:
        Win32Window() = default;
        ~Win32Window() override;

        Win32Window(const Win32Window&) = delete;
        Win32Window& operator=(const Win32Window&) = delete;

        bool Init(const WindowDesc& desc);

        // Vulkan instance extensions required by GLFW.
        static std::vector<const char*> GetRequiredVulkanExtensions();

        void PollEvents() override;
        bool ShouldClose() const override;
        void SetTitle(const std::string& title) override;

        uint32_t GetWidth() const override { return m_width; }
        uint32_t GetHeight() const override { return m_height; }
        void* GetNativeHandle() const override { return m_handle; }

        bool IsKeyDown(int key) const override;
        bool IsMouseButtonDown(int button) const override;
        void GetCursorPosition(double& x, double& y) const override;
        void SetCursorCaptured(bool captured) override;

    private:
        // GLFW callbacks (static, they find 'this' through the user pointer).
        static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void CloseCallback(GLFWwindow* window);
        static void FocusCallback(GLFWwindow* window, int focused);

        GLFWwindow* m_handle = nullptr;
        uint32_t m_width = 0;
        uint32_t m_height = 0;

        // GLFW is global: init on first window, terminate when the last one dies.
        static inline int s_windowCount = 0;
    };
} // namespace DA
