#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <span>

namespace DA {
    class VulkanDevice;
    class VulkanCommandPool;

    // Describes one semaphore wait or signal in a submit, with the pipeline stage it applies to.
    inline VkSemaphoreSubmitInfo MakeSemaphoreSubmitInfo(VkSemaphore semaphore, VkPipelineStageFlags2 stageMask) {
        VkSemaphoreSubmitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
        info.semaphore = semaphore;
        info.stageMask = stageMask;
        return info;
    }

    class DA_API VulkanCommandBuffer {
    public:
        // Returns nullptr on failure (error is logged).
        // The pool must outlive this object (declare the buffer after the pool, so it is destroyed first).
        static std::unique_ptr<VulkanCommandBuffer> Create(
            const VulkanDevice& device, const VulkanCommandPool& pool,
            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        ~VulkanCommandBuffer();

        VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
        VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

        // oneTimeSubmit = true when you record again before every submit (the usual per-frame pattern).
        bool Begin(bool oneTimeSubmit = true);
        bool End();

        // Needs a pool created with RESET_COMMAND_BUFFER, otherwise reset the pool instead.
        bool Reset();

        // Ends recording if needed, then submits with vkQueueSubmit2. The caller owns fence reset.
        VkResult Submit(VkQueue queue, std::span<const VkSemaphoreSubmitInfo> waits = {},
                        std::span<const VkSemaphoreSubmitInfo> signals = {}, VkFence fence = VK_NULL_HANDLE);

        // Submits and blocks until the GPU finishes. For one-off work (uploads), never the frame loop.
        bool SubmitAndWait(VkQueue queue);

        VkCommandBuffer GetHandle() const { return m_buffer; }
        operator VkCommandBuffer() const { return m_buffer; }
        bool IsRecording() const { return m_recording; }

    private:
        VulkanCommandBuffer(const VulkanDevice& device, const VulkanCommandPool& pool)
            : m_device(device), m_pool(pool) {}

        const VulkanDevice& m_device;
        const VulkanCommandPool& m_pool;
        VkCommandBuffer m_buffer = VK_NULL_HANDLE;
        bool m_recording = false;
    };
} // namespace DA
