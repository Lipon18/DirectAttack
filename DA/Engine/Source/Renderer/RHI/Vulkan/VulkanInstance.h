#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace DA {
    struct VulkanInstanceDesc {
        std::string AppName = "DA Application";
        uint32_t AppVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
        // Platform/window-system extensions (e.g. VK_KHR_surface, VK_KHR_win32_surface).
        std::vector<const char*> RequiredExtensions;
        bool EnableValidation = false;
    };

    class DA_API VulkanInstance {
    public:
        // Returns nullptr on failure (error is logged).
        static std::unique_ptr<VulkanInstance> Create(const VulkanInstanceDesc& desc);
        ~VulkanInstance();

        VulkanInstance(const VulkanInstance&) = delete;
        VulkanInstance& operator=(const VulkanInstance&) = delete;

        VkInstance GetHandle() const { return m_instance; }
        operator VkInstance() const { return m_instance; }

        uint32_t GetApiVersion() const { return m_apiVersion; }
        bool IsValidationEnabled() const { return m_validationEnabled; }

    private:
        VulkanInstance() = default;
        bool Init(const VulkanInstanceDesc& desc);

        VkInstance m_instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
        uint32_t m_apiVersion = 0;
        bool m_validationEnabled = false;
    };
} // namespace DA
