#include <gtest/gtest.h>
#include <Platform/Window.h>

class WindowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Any pre-test setup if necessary
    }

    void TearDown() override {
        // Ensure any resources are cleaned up
    }
};

TEST_F(WindowTest, WindowCreationAndBasicProperties) {
    DA::WindowDesc desc;
    desc.Title = "Test Window";
    desc.Width = 1280;
    desc.Height = 720;
    desc.Resizeable = true;

    auto window = DA::Window::Create(desc);
    ASSERT_NE(window, nullptr) << "Failed to create DA::Window.";

    // Verify initial dimensions and native handle
    EXPECT_EQ(window->GetWidth(), 1280);
    EXPECT_EQ(window->GetHeight(), 720);
    EXPECT_NE(window->GetNativeHandle(), nullptr);
    EXPECT_FALSE(window->ShouldClose());
}

TEST_F(WindowTest, TitleModification) {
    DA::WindowDesc desc;
    desc.Title = "Original Title";
    auto window = DA::Window::Create(desc);
    ASSERT_NE(window, nullptr);

    // Verify setting title doesn't crash
    EXPECT_NO_THROW({ window->SetTitle("Updated Game Engine Title"); });
}

TEST_F(WindowTest, VulkanExtensionsAreProvided) {
    auto extensions = DA::Window::GetRequiredVulkanExtensions();

    // GLFW should at least return surface and platform-specific surface extensions on Windows
    EXPECT_FALSE(extensions.empty());

    bool foundSurfaceExtension = false;
    for (const char* ext : extensions) {
        if (std::string(ext) == "VK_KHR_surface") {
            foundSurfaceExtension = true;
            break;
        }
    }
    EXPECT_TRUE(foundSurfaceExtension) << "Required Vulkan surface extension was not returned by the window backend.";
}

TEST_F(WindowTest, EventCallbackRegistration) {
    DA::WindowDesc desc;
    auto window = DA::Window::Create(desc);
    ASSERT_NE(window, nullptr);

    bool resizeTriggered = false;
    bool focusTriggered = false;
    bool closeTriggered = false;

    // Register test callbacks
    window->OnResize = [&](uint32_t width, uint32_t height) {
        resizeTriggered = true;
        EXPECT_EQ(width, 1024);
        EXPECT_EQ(height, 768);
    };
    window->OnFocus = [&](bool focused) { focusTriggered = true; };
    window->OnClose = [&]() { closeTriggered = true; };

    // Verify callbacks can be assigned without throwing
    SUCCEED() << "Successfully assigned window event callbacks.";

}
