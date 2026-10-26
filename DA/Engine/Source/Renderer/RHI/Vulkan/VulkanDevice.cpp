#include "PCH.h"
#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "VulkanPhysicalDevice.h"
#include <Core/Log.h>

#include <vulkan/vk_enum_string_helper.h>

#include <set>

namespace DA {
    std::unique_ptr<VulkanDevice> VulkanDevice::Create(const VulkanInstance& instance,
                                                       const VulkanPhysicalDevice& physicalDevice) {
        std::unique_ptr<VulkanDevice> device(new VulkanDevice());
        if (!device->Init(physicalDevice)) return nullptr;
        return device;
    }

    bool VulkanDevice::Init(const VulkanPhysicalDevice& physicalDevice) {
        const VulkanQueueFamilies& families = physicalDevice.GetQueueFamilies();
        m_graphicsFamily = families.Graphics;
        m_presentFamily = families.Present;

        // One queue per *unique* family (the same family may serve both roles).
        const std::set<uint32_t> uniqueFamilies = {m_graphicsFamily, m_presentFamily};
        const float priority = 1.0f;

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            info.queueFamilyIndex = family;
            info.queueCount = 1;
            info.pQueuePriorities = &priority;
            queueInfos.push_back(info);
        }

        // What the GPU supports (for optional core features).
        VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        vkGetPhysicalDeviceFeatures2(physicalDevice.GetHandle(), &supported);

        // What we turn on. PhysicalDevice already guaranteed the required ones exist.
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;
        VkPhysicalDeviceFeatures2 enabled{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        enabled.pNext = &features13;
        enabled.features.samplerAnisotropy = supported.features.samplerAnisotropy;

        const auto& extensions = VulkanPhysicalDevice::RequiredExtensions();

        VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        createInfo.pNext = &enabled;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (VkResult r = vkCreateDevice(physicalDevice.GetHandle(), &createInfo, nullptr, &m_device); r != VK_SUCCESS) {
            DA_FATAL("vkCreateDevice failed: {}", string_VkResult(r));
            m_device = VK_NULL_HANDLE;
            return false;
        }
        vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);
        DA_INFO("Vulkan device created (graphics family {}, present family {}, anisotropy {}).", m_graphicsFamily,
                m_presentFamily, enabled.features.samplerAnisotropy ? "on" : "off");
        m_anisotropyEnabled = enabled.features.samplerAnisotropy == VK_TRUE;
        m_maxAnisotropy = physicalDevice.GetProperties().limits.maxSamplerAnisotropy;
        return true;
    }

    void VulkanDevice::WaitIdle() const {
        if (m_device != VK_NULL_HANDLE) vkDeviceWaitIdle(m_device);
    }

    VulkanDevice::~VulkanDevice() {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
            vkDestroyDevice(m_device, nullptr);
        }
    }
} // namespace DA
