#include "PCH.h"
#include "VulkanCommandBuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanDevice.h"

#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    std::unique_ptr<VulkanCommandBuffer> VulkanCommandBuffer::Create(const VulkanDevice& device,
                                                                     const VulkanCommandPool& pool,
                                                                     VkCommandBufferLevel level) {
        std::unique_ptr<VulkanCommandBuffer> cmd(new VulkanCommandBuffer(device, pool));

        VkCommandBufferAllocateInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        info.commandPool = pool.GetHandle();
        info.level = level;
        info.commandBufferCount = 1;

        if (VkResult r = vkAllocateCommandBuffers(device.GetHandle(), &info, &cmd->m_buffer); r != VK_SUCCESS) {
            DA_ERROR("vkAllocateCommandBuffers failed: {}", string_VkResult(r));
            cmd->m_buffer = VK_NULL_HANDLE;
            return nullptr;
        }
        return cmd;
    }

    bool VulkanCommandBuffer::Begin(bool oneTimeSubmit) {
        if (m_recording) return true;

        VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (oneTimeSubmit) info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (VkResult r = vkBeginCommandBuffer(m_buffer, &info); r != VK_SUCCESS) {
            DA_ERROR("vkBeginCommandBuffer failed: {}", string_VkResult(r));
            return false;
        }
        m_recording = true;
        return true;
    }

    bool VulkanCommandBuffer::End() {
        if (!m_recording) return true;

        m_recording = false;
        if (VkResult r = vkEndCommandBuffer(m_buffer); r != VK_SUCCESS) {
            DA_ERROR("vkEndCommandBuffer failed: {}", string_VkResult(r));
            return false;
        }
        return true;
    }

    bool VulkanCommandBuffer::Reset() {
        m_recording = false;
        if (VkResult r = vkResetCommandBuffer(m_buffer, 0); r != VK_SUCCESS) {
            DA_ERROR("vkResetCommandBuffer failed: {}", string_VkResult(r));
            return false;
        }
        return true;
    }

    VkResult VulkanCommandBuffer::Submit(VkQueue queue, std::span<const VkSemaphoreSubmitInfo> waits,
                                         std::span<const VkSemaphoreSubmitInfo> signals, VkFence fence) {
        if (!End()) return VK_ERROR_UNKNOWN;

        VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        cmdInfo.commandBuffer = m_buffer;
        VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submit.waitSemaphoreInfoCount = static_cast<uint32_t>(waits.size());
        submit.pWaitSemaphoreInfos = waits.data();
        submit.commandBufferInfoCount = 1;
        submit.pCommandBufferInfos = &cmdInfo;
        submit.signalSemaphoreInfoCount = static_cast<uint32_t>(signals.size());
        submit.pSignalSemaphoreInfos = signals.data();
        return vkQueueSubmit2(queue, 1, &submit, fence);
    }

    bool VulkanCommandBuffer::SubmitAndWait(VkQueue queue) {
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VkFence fence = VK_NULL_HANDLE;
        if (VkResult r = vkCreateFence(m_device.GetHandle(), &fenceInfo, nullptr, &fence); r != VK_SUCCESS) {
            DA_ERROR("vkCreateFence failed: {}", string_VkResult(r));
            return false;
        }
        bool ok = Submit(queue, {}, {}, fence) == VK_SUCCESS;
        if (ok) ok = vkWaitForFences(m_device.GetHandle(), 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
        vkDestroyFence(m_device.GetHandle(), fence, nullptr);
        return ok;
    }

    VulkanCommandBuffer::~VulkanCommandBuffer() {
        if (m_buffer != VK_NULL_HANDLE) {
            const VkCommandBuffer buffer = m_buffer;
            vkFreeCommandBuffers(m_device.GetHandle(), m_pool.GetHandle(), 1, &buffer);
        }
    }
} // namespace DA
