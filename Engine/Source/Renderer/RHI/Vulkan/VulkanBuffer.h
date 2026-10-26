#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <memory>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace DA {
    class VulkanDevice;
    class VulkanAllocator;

    class DA_API VulkanBuffer {
    public:
        // GPU-only memory, filled once through a staging buffer (blocks until the copy finishes).
        // Use for vertex/index/static data. TRANSFER_DST is added to `usage` automatically.
        static std::unique_ptr<VulkanBuffer> CreateDeviceLocal(const VulkanDevice& device,
                                                               const VulkanAllocator& allocator,
                                                               VkBufferUsageFlags usage, const void* data,
                                                               VkDeviceSize size);
        // CPU-writable memory for data that changes often (uniforms, staging).
        static std::unique_ptr<VulkanBuffer> CreateHostVisible(const VulkanAllocator& allocator,
                                                               VkBufferUsageFlags usage, VkDeviceSize size);
        ~VulkanBuffer();

        VulkanBuffer(const VulkanBuffer&) = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;

        // Host-visible buffers only. Handles flushing if the memory isn't coherent.
        bool Write(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

        VkBuffer GetHandle() const { return m_buffer; }
        VkDeviceSize GetSize() const { return m_size; }

    private:
        VulkanBuffer() = default;

        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VmaAllocation m_allocation = VK_NULL_HANDLE;
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkDeviceSize m_size = 0;
    };
} // namespace DA
