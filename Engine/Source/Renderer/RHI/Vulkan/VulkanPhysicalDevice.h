#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace DA {
    class VulkanInstance;
    class VulkanSurface;

    struct VulkanQueueFamilies {
        uint32_t Graphics = UINT32_MAX;
        uint32_t Present = UINT32_MAX;
    };

    class DA_API VulkanPhysicalDevice {
    public:
        // Picks the best suitable GPU for the surface. Returns nullptr on failure (reasons are logged).
        static std::unique_ptr<VulkanPhysicalDevice> Create(const VulkanInstance& instance,
                                                            const VulkanSurface& surface);

        // Device extensions every supported GPU must have. The logical device enables exactly these.
        static const std::vector<const char*>& RequiredExtensions();

        VkPhysicalDevice GetHandle() const { return m_handle; }
        operator VkPhysicalDevice() const { return m_handle; }

        const VkPhysicalDeviceProperties& GetProperties() const { return m_properties; }
        const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_memoryProperties; }
        const VulkanQueueFamilies& GetQueueFamilies() const { return m_queueFamilies; }
        std::string GetName() const { return m_properties.deviceName; }

    private:
        VulkanPhysicalDevice() = default;

        // No ownership: VkPhysicalDevice is owned by the instance, so there is no destructor work.
        VkPhysicalDevice m_handle = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties m_properties{};
        VkPhysicalDeviceMemoryProperties m_memoryProperties{};
        VulkanQueueFamilies m_queueFamilies;
    };
} // namespace DA
