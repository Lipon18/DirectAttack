#include "PCH.h"
#include "VulkanPhysicalDevice.h"
#include "VulkanInstance.h"
#include "VulkanSurface.h"
#include <Core/Log.h>

namespace DA {
    namespace {
        struct Candidate {
            VkPhysicalDevice Device = VK_NULL_HANDLE;
            uint32_t Score = 0;
            VulkanQueueFamilies Queues;
        };

        bool SupportsExtensions(VkPhysicalDevice device, const char*& missing) {
            uint32_t count = 0;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
            std::vector<VkExtensionProperties> available(count);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

            for (const char* required : VulkanPhysicalDevice::RequiredExtensions()) {
                const bool found = std::ranges::any_of(available, [&](const VkExtensionProperties& e) {
                    return std::strcmp(e.extensionName, required) == 0;
                });
                if (!found) {
                    missing = required;
                    return false;
                }
            }
            return true;
        }

        bool SupportsFeatures(VkPhysicalDevice device) {
            VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
            VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            f2.pNext = &f13;
            vkGetPhysicalDeviceFeatures2(device, &f2);
            return f13.dynamicRendering == VK_TRUE && f13.synchronization2 == VK_TRUE;
        }

        // Prefers one family that does both graphics and present (the common case).
        // Falls back to separate families.
        bool FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, VulkanQueueFamilies& out) {
            uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

            VulkanQueueFamilies result;
            for (uint32_t i = 0; i < count; ++i) {
                const bool graphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
                if (graphics && present) {
                    out = {i, i};
                    return true;
                }
                if (graphics && result.Graphics == UINT32_MAX) result.Graphics = i;
                if (present && result.Present == UINT32_MAX) result.Present = i;
            }
            if (result.Graphics == UINT32_MAX || result.Present == UINT32_MAX) return false;
            out = result;
            return true;
        }

        bool SurfaceUsable(VkPhysicalDevice device, VkSurfaceKHR surface) {
            uint32_t formats = 0;
            uint32_t modes = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formats, nullptr);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modes, nullptr);
            return formats > 0 && modes > 0;
        }

        std::optional<Candidate> Evaluate(VkPhysicalDevice device, VkSurfaceKHR surface) {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(device, &props);
            const char* name = props.deviceName;
            if (props.apiVersion < VK_API_VERSION_1_3) {
                DA_INFO("GPU '{}' rejected: API {}.{} < 1.3", name, VK_API_VERSION_MAJOR(props.apiVersion),
                        VK_API_VERSION_MINOR(props.apiVersion));
                return std::nullopt;
            }
            const char* missing = nullptr;
            if (!SupportsExtensions(device, missing)) {
                DA_INFO("GPU '{}' rejected: missing extension {}", name, missing);
                return std::nullopt;
            }
            if (!SupportsFeatures(device)) {
                DA_INFO("GPU '{}' rejected: needs dynamicRendering and synchronization2", name);
                return std::nullopt;
            }

            Candidate candidate;
            candidate.Device = device;
            if (!FindQueueFamilies(device, surface, candidate.Queues)) {
                DA_INFO("GPU '{}' rejected: no graphics+present queue families", name);
                return std::nullopt;
            }
            if (!SurfaceUsable(device, surface)) {
                DA_INFO("GPU '{}' rejected: surface has no formats or present modes", name);
                return std::nullopt;
            }

            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) candidate.Score += 1000;
            candidate.Score += props.limits.maxImageDimension2D;
            return candidate;
        }

        const char* DeviceTypeName(VkPhysicalDeviceType type) {
            switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                return "Integrated";
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                return "Discrete";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                return "Virtual";
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                return "CPU";
            default:
                return "Other";
            }
        }
    } // namespace

    const std::vector<const char*>& VulkanPhysicalDevice::RequiredExtensions() {
        static const std::vector<const char*> extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        return extensions;
    }

    std::unique_ptr<VulkanPhysicalDevice> VulkanPhysicalDevice::Create(const VulkanInstance& instance,
                                                                       const VulkanSurface& surface) {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance.GetHandle(), &count, nullptr);
        if (count == 0) {
            DA_FATAL("No Vulkan-capable GPU found.");
            return nullptr;
        }
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance.GetHandle(), &count, devices.data());

        std::optional<Candidate> best;
        for (VkPhysicalDevice device : devices) {
            auto candidate = Evaluate(device, surface.GetHandle());
            if (candidate && (!best || candidate->Score > best->Score)) best = candidate;
        }
        if (!best) {
            DA_FATAL("No suitable GPU found (need Vulkan 1.3, swapchain, dynamic rendering, synchronization2).");
            return nullptr;
        }

        std::unique_ptr<VulkanPhysicalDevice> result(new VulkanPhysicalDevice());
        result->m_handle = best->Device;
        result->m_queueFamilies = best->Queues;
        vkGetPhysicalDeviceProperties(result->m_handle, &result->m_properties);
        vkGetPhysicalDeviceMemoryProperties(result->m_handle, &result->m_memoryProperties);
        const auto& p = result->m_properties;
        DA_INFO("Selected GPU: '{}' ({}, API {}.{}.{}), graphics family {}, present family {}", p.deviceName,
                DeviceTypeName(p.deviceType), VK_API_VERSION_MAJOR(p.apiVersion), VK_API_VERSION_MINOR(p.apiVersion),
                VK_API_VERSION_PATCH(p.apiVersion), result->m_queueFamilies.Graphics, result->m_queueFamilies.Present);
        return result;
    }

} // namespace DA
