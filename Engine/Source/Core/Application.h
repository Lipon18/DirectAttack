/**
 * @file Application.h
 * @brief TODO: describe this file.
 *
 * Copyright (c) 2026 YourName
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "DA_API.h"

#include <cstdint>
#include <string>

namespace DA {
    class Engine;

    struct ApplicationConfig {
        std::string Name = "DirectAttack Application";
        uint32_t Width = 1280;
        uint32_t Height = 720;
        bool Resizable = true;
    };

    class DA_API Application {
        friend class Engine;

    public:
        explicit Application(ApplicationConfig config = {});
        virtual ~Application() = default;

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        const ApplicationConfig& GetConfig() const { return m_config; }

        // Access the engine managing this application.
        Engine& GetEngine() const;

    protected:
        // Called once after the engine creates the window.
        virtual bool OnInit() { return true; }

        // Called every frame. Delta time is in seconds.
        virtual void OnUpdate(float deltaSeconds) {}

        // Called once before the application is destroyed.
        virtual void OnShutdown() {}

    private:
        ApplicationConfig m_config;
        bool m_started = false;
        bool m_shutdownCalled = false;
    };
} // namespace DA
