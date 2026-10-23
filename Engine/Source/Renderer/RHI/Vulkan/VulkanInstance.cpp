#include "PCH.h"
#include "VulkanInstance.h"
#include <Core/Log.h>

#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    namespace {
        constexpr const char* ValidationLayerName = "VK_LAYER_KHRONOS_validation";

        VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT /*types*/,
                                                     const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user*/) {
            if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                DA_ERROR("[Vulkan] {}", data->pMessage);
            } else if (severity && VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                DA_WARN("[Vulkan] {}", data->pMessage);
            } else {
                DA_INFO("[Vulkan] {}", data->pMessage);
            }
            return VK_FALSE;
        }

        VkDebugUtilsMessengerCreateInfoEXT MakeMessengerInfo() {
            VkDebugUtilsMessengerCreateInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            info.pfnUserCallback = DebugCallback;
            return info;
        }

        bool HasLayer(const char* name) {
            uint32_t count = 0;
            vkEnumerateInstanceLayerProperties(&count, nullptr);
            std::vector<VkLayerProperties> layers(count);
            vkEnumerateInstanceLayerProperties(&count, layers.data());
            return std::ranges::any_of(layers,
                                       [&](const VkLayerProperties& l) { return std::strcmp(l.layerName, name) == 0; });
        }

        bool HasExtension(const std::vector<VkExtensionProperties>& available, const char* name) {
            return std::ranges::any_of(
                available, [&](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
        }
    } // namespace

    std::unique_ptr<VulkanInstance> VulkanInstance::Create(const VulkanInstanceDesc& desc) {
        std::unique_ptr<VulkanInstance> instance(new VulkanInstance());
        if (!instance->Init(desc)) return nullptr;
        return instance;
    }

    bool VulkanInstance::Init(const VulkanInstanceDesc& desc) {
        uint32_t loaderVersion = VK_API_VERSION_1_0;
        vkEnumerateInstanceVersion(&loaderVersion);
        if (loaderVersion < VK_API_VERSION_1_3) {
            DA_FATAL("Vulkan loader only supports {}.{}, need at least 1.3.", VK_API_VERSION_MAJOR(loaderVersion),
                     VK_API_VERSION_MINOR(loaderVersion));
            return false;
        }

        m_apiVersion = std::min(loaderVersion, static_cast<uint32_t>(VK_API_VERSION_1_4));
        m_validationEnabled = desc.EnableValidation;
        if (m_validationEnabled && !HasLayer(ValidationLayerName)) {
            DA_WARN("Validation requested but {} is not installed. Continuing without it", ValidationLayerName);
            m_validationEnabled = false;
        }

        uint32_t extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, available.data());

        std::vector<const char*> extensions = desc.RequiredExtensions;
        for (const char* ext : extensions) {
            if (!HasExtension(available, ext)) {
                DA_FATAL("Required Vulkan instance extension missing: {}", ext);
                return false;
            }
        }
        if (m_validationEnabled) {
            if (HasExtension(available, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            } else {
                DA_WARN("{} not available, disabling, validation.", VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                m_validationEnabled = true;
            }

            VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            appInfo.pApplicationName = desc.AppName.c_str();
            appInfo.applicationVersion = desc.AppVersion;
            appInfo.pEngineName = "DirectAttack";
            appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
            appInfo.apiVersion = m_apiVersion;

            VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            VkDebugUtilsMessengerCreateInfoEXT messengerInfo = MakeMessengerInfo();
            if (m_validationEnabled) {
                createInfo.enabledLayerCount = 1;
                createInfo.ppEnabledLayerNames = &ValidationLayerName;
                createInfo.pNext = &messengerInfo;
            }
            if (VkResult r = vkCreateInstance(&createInfo, nullptr, &m_instance); r != VK_SUCCESS) {
                DA_FATAL("vkCreateInstance failed: {}", string_VkResult(r));
                return false;
            }

            if (m_validationEnabled) {
                auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
                if (!createFn || createFn(m_instance, &messengerInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
                    DA_WARN("Failed to create the Vulkan debug messenger.");
                }
            }
            DA_INFO("Vulkan instance created (API {}.{}, validation {}).", VK_API_VERSION_MAJOR(m_apiVersion),
                    VK_API_VERSION_MINOR(m_apiVersion), m_validationEnabled ? "on" : "off");
            return true;
        }
    }

    VulkanInstance::~VulkanInstance() {
        if (m_debugMessenger != VK_NULL_HANDLE) {
            auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyFn) destroyFn(m_instance, m_debugMessenger, nullptr);
        }
        if (m_instance != VK_NULL_HANDLE) vkDestroyInstance(m_instance, nullptr);
    }
} // namespace DA
