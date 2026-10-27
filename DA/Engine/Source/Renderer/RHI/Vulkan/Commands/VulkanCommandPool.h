#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

namespace DA {
    class VulkanDevice;

    class DA_API VulkanCommandPool {
    public:
        // Returns nullptr on failure (error is logged).
        // RESET_COMMAND_BUFFER lets individual buffers be reset. Drop it if you only ever call Reset() on the pool.
        static std::unique_ptr<VulkanCommandPool> Create(
            const VulkanDevice& device, uint32_t queueFamily,
            VkCommandPoolCreateFlags flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        ~VulkanCommandPool();

        VulkanCommandPool(const VulkanCommandPool&) = delete;
        VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

        // Resets every command buffer allocated from this pool at once (none may be in flight).
        bool Reset();

        VkCommandPool GetHandle() const { return m_pool; }
        operator VkCommandPool() const { return m_pool; }
        uint32_t GetQueueFamily() const { return m_queueFamily; }

    private:
        VulkanCommandPool(const VulkanDevice& device, uint32_t queueFamily)
            : m_device(device), m_queueFamily(queueFamily) {}

        const VulkanDevice& m_device;
        uint32_t m_queueFamily;
        VkCommandPool m_pool = VK_NULL_HANDLE;
    };
} // namespace DA
