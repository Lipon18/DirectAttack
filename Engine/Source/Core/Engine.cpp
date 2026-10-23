#include "PCH.h"
#include "Engine.h"
#include "Application.h"
#include "Log.h"
#include <Platform/Window.h>
#include <Renderer/RHI/Vulkan/VulkanInstance.h>
#include <Renderer/RHI/Vulkan/VulkanSurface.h>
#include <Renderer/RHI/Vulkan/VulkanPhysicalDevice.h>

#include "Core/EngineConfig.h"

#include <chrono>

namespace DA {
    Engine* Engine::s_instance = nullptr;

    Engine::Engine() {
        s_instance = this;
    }

    Engine::~Engine() {
        m_app.reset();

        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    bool Engine::Initialize(std::unique_ptr<Application> app) {
        if (m_initialized || !app) {
            DA_ERROR("Engine initialization failed: invalid state or application.");
            return false;
        }
        m_app = std::move(app);
        const auto& config = m_app->GetConfig();

        DA_INFO("{} Engine v{}", Config::EngineName, Config::Version);

        WindowDesc desc;
        desc.Title = config.Name;
        desc.Width = config.Width;
        desc.Height = config.Height;
        desc.Resizeable = config.Resizable;

        auto window = Window::Create(desc);
        if (!window) {
            DA_FATAL("Failed to create the application window.");
            m_app.reset();
            return false;
        }

        // Window close events stop the engine loop.
        window->OnClose = [this]() { RequestClose(); };

        // Store the window in the application engine state in a later
        // extension. For now, the engine owns it in this local implementation.
        // See the member declaration below for the persistent window.
        m_window = std::move(window);

        // Create the Vulkan instance
        // clang-format off
        VulkanInstanceDesc vk;
        vk.AppName = config.Name;
        vk.RequiredExtensions = Window::GetRequiredVulkanExtensions();
#ifndef  NDEBUG
        vk.EnableValidation = true;
#endif // ! NDEBUG
        m_vkInstance = VulkanInstance::Create(vk);
        if(!m_vkInstance) {m_window.reset(); m_app.reset(); return false;}
        // clang-format on

        m_vkSurface = VulkanSurface::Create(*m_vkInstance, *m_window);
        if (!m_vkSurface) {
            m_vkSurface.reset();
            m_window.reset();
            m_app.reset();
            return false;
        }

        m_vkPhysicalDevice = VulkanPhysicalDevice::Create(*m_vkInstance, *m_vkSurface);
        if (!m_vkPhysicalDevice) {
            m_vkSurface.reset();
            m_vkInstance.reset();
            m_window.reset();
            m_app.reset();
            return false;
        }

        if (!m_app->OnInit()) {
            DA_ERROR("Application initialization failed.");
            m_app.reset();
            m_window.reset();
            return false;
        }
        m_app->m_started = true;
        m_initialized = true;
        m_running = true;

        DA_INFO("Application '{}' initialized.", config.Name);
        return true;
    }

    int Engine::Run() {
        if (!m_initialized || !m_app || !m_window) {
            DA_ERROR("Cannot run the engine before successful initialization.");
            return -1;
        }

        using Clock = std::chrono::steady_clock;
        auto lastFrame = Clock::now();

        while (m_running && !m_window->ShouldClose()) {
            m_window->PollEvents();
            const auto now = Clock::now();
            const float deltaSeconds = std::chrono::duration<float>(now - lastFrame).count();
            lastFrame = now;
            m_app->OnUpdate(deltaSeconds);
        }
        if (m_app && !m_app->m_shutdownCalled) {
            m_app->OnShutdown();
            m_app->m_shutdownCalled = true;
        }

        DA_INFO("Application shut down.");
        m_running = false;

        return 0;
    }

    void Engine::RequestClose() {
        m_running = false;
    }

    Window* Engine::GetWindow() const {
        return m_window.get();
    }

} // namespace DA
