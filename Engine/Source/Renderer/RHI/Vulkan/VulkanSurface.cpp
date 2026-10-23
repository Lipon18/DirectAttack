#include "PCH.h"
#include "VulkanSurface.h"
#include "VulkanInstance.h"
#include <Core/Log.h>
#include <Platform/Window.h>

#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    std::unique_ptr<VulkanSurface> VulkanSurface::Create(const VulkanInstance& instance, const Window& window) {
        std::unique_ptr<VulkanSurface> surface(new VulkanSurface(instance));
        if (!surface->Init(window)) return nullptr;
        return surface;
    }

    bool VulkanSurface::Init(const Window& window) {
        // The only GLFW backend exists today, so the native handle is a GLFWwindow*.
        // When a second backend appears, turn this into a virtual on Window.
        auto* glfwWindow = static_cast<GLFWwindow*>(window.GetNativeHandle());
        if (!glfwWindow) {
            DA_FATAL("Cannot create a Vulkan surface: window has no native handle.");
            return false;
        }

        const VkResult result = glfwCreateWindowSurface(m_instance.GetHandle(), glfwWindow, nullptr, &m_surface);
        if (result != VK_SUCCESS) {
            DA_FATAL("glfwCreateWindowSurface failed: {}", string_VkResult(result));
            m_surface = VK_NULL_HANDLE;
            return false;
        }
        DA_INFO("Vulkan surface created.");
        return true;
    }

    VulkanSurface::~VulkanSurface() {
        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance.GetHandle(), m_surface, nullptr);
        }
    }
} // namespace DA
