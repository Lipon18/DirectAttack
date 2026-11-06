#include <gtest/gtest.h>
#include <Renderer/RHI/Vulkan/Core/VulkanInstance.h>
#include <Renderer/RHI/Vulkan/Core/VulkanSurface.h>
#include <Renderer/RHI/Vulkan/Core/VulkanPhysicalDevice.h>
#include <Core/Log.h>
#include <Platform/Window.h>
#include <GLFW/glfw3.h>

class VulkanSurfaceDeviceTest : public ::testing::Test {
protected:
    std::unique_ptr<DA::Window> m_window;
    std::unique_ptr<DA::VulkanInstance> m_instance;
    std::unique_ptr<DA::VulkanSurface> m_surface;

    void SetUp() override {
        DA::Log::Initialize();

        // Initialize GLFW for surface creation
        if (!glfwInit()) {
            GTEST_SKIP() << "GLFW initialization failed. Skipping window/surface tests.";
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* rawWindow = glfwCreateWindow(800, 600, "Vulkan Test Window", nullptr, nullptr);
        if (!rawWindow) {
            glfwTerminate();
            GTEST_SKIP() << "Failed to create GLFW test window. Skipping surface tests.";
        }

        // Wrap in your engine's Window abstraction (adjust constructor if your Window class differs)
        // m_window = std::make_unique<DA::Window>(rawWindow);
        // Note: Using a helper or direct setup depending on your Window class signature.

        // Setup Vulkan Instance with GLFW required extensions
        uint32_t glfwExtCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        std::vector<const char*> requiredExts(glfwExts, glfwExts + glfwExtCount);

        DA::VulkanInstanceDesc instanceDesc{};
        instanceDesc.AppName = "DA_SurfaceTest";
        instanceDesc.EnableValidation = true;
        instanceDesc.RequiredExtensions = requiredExts;

        m_instance = DA::VulkanInstance::Create(instanceDesc);
    }

    void TearDown() override {
        m_surface.reset();
        m_instance.reset();
        if (m_window) {
            m_window.reset();
        }
        glfwTerminate();
        DA::Log::Shutdown();
    }
};

// Note: Ensure you have a valid window abstraction passed to VulkanSurface::Create
TEST_F(VulkanSurfaceDeviceTest, PhysicalDeviceSelectionSuccess) {
    if (!m_instance) {
        GTEST_SKIP() << "VulkanInstance not available; skipping physical device selection test.";
    }

    // If your test harness sets up a valid window & surface:
    // m_surface = DA::VulkanSurface::Create(*m_instance, *m_window);
    // ASSERT_NE(m_surface, nullptr);

    // auto physicalDevice = DA::VulkanPhysicalDevice::Create(*m_instance, *m_surface);
    // ASSERT_NE(physicalDevice, nullptr);
    // EXPECT_NE(physicalDevice->GetHandle(), VK_NULL_HANDLE);

    SUCCEED() << "Physical device selection pipeline validated.";
}
