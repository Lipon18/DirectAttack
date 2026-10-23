#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

namespace DA {
    class VulkanInstance;
    class VulkanPhysicalDevice;

    class DA_API VulkanDevice {
    public:
        static std::unique_ptr<VulkanDevice> Create(const VulkanInstance& instance,
                                                    const VulkanPhysicalDevice& physicalDevice);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;

        VkDevice GetHandle() const { return m_device; }
        operator VkDevice() const { return m_device; }

        VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
        VkQueue GetPresentQueue() const { return m_presentQueue; }
        uint32_t GetGraphicsFamily() const { return m_graphicsFamily; }
        uint32_t GetPresentFamily() const { return m_presentFamily; }

        // Blocks until the GPU has finished all submitted work.
        void WaitIdle() const;

    private:
        VulkanDevice() = default;
        bool Init(const VulkanPhysicalDevice& physicalDevice);

        VkDevice m_device = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        VkQueue m_presentQueue = VK_NULL_HANDLE;
        uint32_t m_graphicsFamily = UINT32_MAX;
        uint32_t m_presentFamily = UINT32_MAX;
    };
} // namespace DA
