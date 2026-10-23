/**
 * @file Engine.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "Core/DA_API.h"

#include <memory>

namespace DA {
    class Application;
    class Window;
    class VulkanInstance;
    class VulkanSurface;
    class VulkanPhysicalDevice;

    class DA_API Engine {
    public:
        Engine();
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Initializes the engine and the supplied application.
        bool Initialize(std::unique_ptr<Application> app);

        // Runs until the window closes or the application requests shutdown.
        int Run();

        // Requests the main loop to stop.
        void RequestClose();

        bool IsRunning() const { return m_running; }

        Application* GetApp() const { return m_app.get(); }
        Window* GetWindow() const;

        static Engine* Get() { return s_instance; }

    private:
        static Engine* s_instance;

        std::unique_ptr<Window> m_window;
        std::unique_ptr<VulkanInstance> m_vkInstance;
        std::unique_ptr<VulkanSurface> m_vkSurface;
        std::unique_ptr<VulkanPhysicalDevice> m_vkPhysicalDevice;
        std::unique_ptr<Application> m_app;
        bool m_running = false;
        bool m_initialized = false;
    };
} // namespace DA
