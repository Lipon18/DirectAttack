#include "PCH.h"
#include "VulkanCommandPool.h"
#include "Renderer/RHI/Vulkan/Core/VulkanDevice.h"

#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    std::unique_ptr<VulkanCommandPool> VulkanCommandPool::Create(const VulkanDevice& device, uint32_t queueFamily,
                                                                 VkCommandPoolCreateFlags flags) {
        std::unique_ptr<VulkanCommandPool> pool(new VulkanCommandPool(device, queueFamily));

        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.flags = flags;
        info.queueFamilyIndex = queueFamily;

        if (VkResult r = vkCreateCommandPool(device.GetHandle(), &info, nullptr, &pool->m_pool); r != VK_SUCCESS) {
            DA_ERROR("vkCreateCommandPool failed: {}", string_VkResult(r));
            pool->m_pool = VK_NULL_HANDLE;
            return nullptr;
        }
        return pool;
    }

    bool VulkanCommandPool::Reset() {
        const VkResult r = vkResetCommandPool(m_device.GetHandle(), m_pool, 0);
        if (r != VK_SUCCESS) {
            DA_ERROR("vkResetCommandPool failed: {}", string_VkResult(r));
            return false;
        }
        return true;
    }

    VulkanCommandPool::~VulkanCommandPool() {
        // Destroying the pool also frees every command buffer allocated from it.
        if (m_pool != VK_NULL_HANDLE) vkDestroyCommandPool(m_device.GetHandle(), m_pool, nullptr);
    }
} // namespace DA
