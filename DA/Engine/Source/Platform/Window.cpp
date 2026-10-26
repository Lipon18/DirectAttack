#include "PCH.h"
#include "Window.h"

// clang-format off
#if defined(_WIN32)
    #include "Win32/Win32Window.h"
#endif

namespace DA {
    std::unique_ptr<Window> Window::Create(const WindowDesc& desc) {
#if defined(_WIN32)
        auto window = std::make_unique<Win32Window>();
        if(!window->Init(desc)) return nullptr;
        return window;
#else
        #error "No window implementation for this platform yet."
#endif
    }
    std::vector<const char*> Window::GetRequiredVulkanExtensions() {
#if defined(_WIN32)
        return Win32Window::GetRequiredVulkanExtensions();
#endif
    }
    // clang-format on
} // namespace DA
