#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

namespace DA {
    class VulkanDevice;

    class DA_API VulkanSemaphore {
    public:
        static std::unique_ptr<VulkanSemaphore> Create(const VulkanDevice& device);
        ~VulkanSemaphore();

        VulkanSemaphore(const VulkanSemaphore&) = delete;
        VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

        VkSemaphore GetHandle() const { return m_semaphore; }

    private:
        explicit VulkanSemaphore(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;
        VkSemaphore m_semaphore = VK_NULL_HANDLE;
    };

    class DA_API VulkanFence {
    public:
        // Create signaled for fences you Wait() on before their first use (per-frame fences).
        static std::unique_ptr<VulkanFence> Create(const VulkanDevice& device, bool signaled = false);
        ~VulkanFence();

        VulkanFence(const VulkanFence&) = delete;
        VulkanFence& operator=(const VulkanFence&) = delete;

        // Returns true once the fence is signaled, false on timeout or error.
        bool Wait(uint64_t timeoutNs = UINT64_MAX) const;
        bool Reset() const;

        VkFence GetHandle() const { return m_fence; }

    private:
        explicit VulkanFence(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;
        VkFence m_fence = VK_NULL_HANDLE;
    };
} // namespace DA
