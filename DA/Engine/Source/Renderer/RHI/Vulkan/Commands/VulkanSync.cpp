#include "PCH.h"
#include "VulkanSync.h"
#include "Renderer/RHI/Vulkan/Core/VulkanDevice.h"

#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    std::unique_ptr<VulkanSemaphore> VulkanSemaphore::Create(const VulkanDevice& device) {
        std::unique_ptr<VulkanSemaphore> sem(new VulkanSemaphore(device));

        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (VkResult r = vkCreateSemaphore(device.GetHandle(), &info, nullptr, &sem->m_semaphore); r != VK_SUCCESS) {
            DA_ERROR("vkCreateSemaphore failed: {}", string_VkResult(r));
            sem->m_semaphore = VK_NULL_HANDLE;
            return nullptr;
        }
        return sem;
    }

    VulkanSemaphore::~VulkanSemaphore() {
        if (m_semaphore != VK_NULL_HANDLE) vkDestroySemaphore(m_device.GetHandle(), m_semaphore, nullptr);
    }

    std::unique_ptr<VulkanFence> VulkanFence::Create(const VulkanDevice& device, bool signaled) {
        std::unique_ptr<VulkanFence> fence(new VulkanFence(device));

        VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (signaled) info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (VkResult r = vkCreateFence(device.GetHandle(), &info, nullptr, &fence->m_fence); r != VK_SUCCESS) {
            DA_ERROR("vkCreateFence failed: {}", string_VkResult(r));
            fence->m_fence = VK_NULL_HANDLE;
            return nullptr;
        }
        return fence;
    }

    bool VulkanFence::Wait(uint64_t timeoutNs) const {
        return vkWaitForFences(m_device.GetHandle(), 1, &m_fence, VK_TRUE, timeoutNs) == VK_SUCCESS;
    }

    bool VulkanFence::Reset() const {
        return vkResetFences(m_device.GetHandle(), 1, &m_fence) == VK_SUCCESS;
    }

    VulkanFence::~VulkanFence() {
        if (m_fence != VK_NULL_HANDLE) vkDestroyFence(m_device.GetHandle(), m_fence, nullptr);
    }
} // namespace DA
