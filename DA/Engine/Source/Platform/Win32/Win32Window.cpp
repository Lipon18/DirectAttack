#include "PCH.h"
#include "Win32Window.h"

#include <GLFW/glfw3.h>

namespace DA {
    namespace {
        void GlfwErrorCallback(int error, const char* description) {
            DA_ERROR("GLFW error {}: {}", error, description);
        }
    } // namespace

    std::vector<const char*> Win32Window::GetRequiredVulkanExtensions() {
        uint32_t count = 0;
        const char** exts = glfwGetRequiredInstanceExtensions(&count);
        return std::vector<const char*>(exts, exts + count);
    }

    bool Win32Window::Init(const WindowDesc& desc) {
        if (s_windowCount == 0) {
            glfwSetErrorCallback(GlfwErrorCallback);
            
            if (glfwInit() == GLFW_FALSE) {
                DA_FATAL("GLFW failed to initialize.");
                return false;
            }
            if (glfwVulkanSupported() == GLFW_FALSE) {
                DA_FATAL("GLFW could not find Vulkan support.");
                glfwTerminate();
                return false;
            }
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, desc.Resizeable ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        m_handle = glfwCreateWindow(static_cast<int>(desc.Width), static_cast<int>(desc.Height), desc.Title.c_str(),
                                    nullptr, nullptr);
        if (!m_handle) {
            DA_FATAL("GLFW failed to create the window.");
            if (s_windowCount == 0) glfwTerminate();
            return false;
        }
        ++s_windowCount;

        int fbWidth = 0;
        int fbHeight = 0;
        glfwGetFramebufferSize(m_handle, &fbWidth, &fbHeight);
        m_width = static_cast<uint32_t>(fbWidth);
        m_height = static_cast<uint32_t>(fbHeight);
        
        glfwSetWindowUserPointer(m_handle, this);
        glfwSetFramebufferSizeCallback(m_handle, FramebufferSizeCallback);
        glfwSetWindowCloseCallback(m_handle, CloseCallback);
        glfwSetWindowFocusCallback(m_handle, FocusCallback);

        glfwShowWindow(m_handle);
        DA_INFO("Window created: '{}' {}x{}", desc.Title, m_width, m_height);
        return true;
    }

    Win32Window::~Win32Window() {
        if (!m_handle) return;

        glfwDestroyWindow(m_handle);
        m_handle = nullptr;
        if (--s_windowCount == 0) glfwTerminate();
    }

    void Win32Window::PollEvents() {
        glfwPollEvents();
    }

    bool Win32Window::ShouldClose() const {
        return glfwWindowShouldClose(m_handle) == GLFW_TRUE;
    }

    void Win32Window::SetTitle(const std::string& title) {
        glfwSetWindowTitle(m_handle, title.c_str());
    }

    void Win32Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
        auto* self = static_cast<Win32Window*>(glfwGetWindowUserPointer(window));
        if (!self) return;
        self->m_width = static_cast<uint32_t>(width);    // 0 when minimized
        self->m_height = static_cast<uint32_t>(height);
        DA_INFO("Window resized: {}x{}", width, height);
        if (self->OnResize) self->OnResize(self->m_width, self->m_height);
    }

    void Win32Window::CloseCallback(GLFWwindow* window) {
        auto* self = static_cast<Win32Window*>(glfwGetWindowUserPointer(window));
        if (!self) return;
        if (self->OnClose) self->OnClose();
    }

    void Win32Window::FocusCallback(GLFWwindow* window, int focused) {
        auto* self = static_cast<Win32Window*>(glfwGetWindowUserPointer(window));
        if (!self) return;
        DA_INFO("Window focus: {}", focused == GLFW_TRUE ? "focused" : "lost focus");
        if (self->OnFocus) self->OnFocus(focused == GLFW_TRUE);
    }
} // namespace DA
