#include "PCH.h"
#include "VulkanBuffer.h"
#include "VulkanAllocator.h"
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandPool.h>
#include <Renderer/RHI/Vulkan/Core/VulkanDevice.h>
#include <Core/Log.h>

#include <vulkan/vk_enum_string_helper.h>

#include <include/vk_mem_alloc.h>

namespace DA {
    /*namespace {
        std::unique_ptr<VulkanBuffer>& Unused();
    }*/
    std::unique_ptr<VulkanBuffer> VulkanBuffer::CreateHostVisible(const VulkanAllocator& allocator,
                                                                  VkBufferUsageFlags usage, VkDeviceSize size) {
        std::unique_ptr<VulkanBuffer> buffer(new VulkanBuffer());

        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        if (VkResult r = vmaCreateBuffer(allocator.GetHandle(), &bufferInfo, &allocInfo, &buffer->m_buffer,
                                         &buffer->m_allocation, nullptr);
            r != VK_SUCCESS) {
            DA_ERROR("vmaCreateBuffer (host visible) failed: {}", string_VkResult(r));
            buffer->m_buffer = VK_NULL_HANDLE;
            return nullptr;
        }
        buffer->m_allocator = allocator.GetHandle();
        buffer->m_size = size;
        return buffer;
    }

    std::unique_ptr<VulkanBuffer> VulkanBuffer::CreateDeviceLocal(const VulkanDevice& device,
                                                                  const VulkanAllocator& allocator,
                                                                  VkBufferUsageFlags usage, const void* data,
                                                                  VkDeviceSize size) {
        auto staging = CreateHostVisible(allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size);
        if (!staging || !staging->Write(data, size)) return nullptr;

        std::unique_ptr<VulkanBuffer> buffer(new VulkanBuffer());

        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        if (VkResult r = vmaCreateBuffer(allocator.GetHandle(), &bufferInfo, &allocInfo, &buffer->m_buffer,
                                         &buffer->m_allocation, nullptr);
            r != VK_SUCCESS) {
            DA_ERROR("vmaCreateBuffer (device local) failed: {}", string_VkResult(r));
            buffer->m_buffer = VK_NULL_HANDLE;
            return nullptr;
        }
        buffer->m_allocator = allocator.GetHandle();
        buffer->m_size = size;

        auto pool = VulkanCommandPool::Create(device, device.GetGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        if (!pool) return nullptr;
        auto cmd = VulkanCommandBuffer::Create(device, *pool);
        if (!cmd || !cmd->Begin()) return nullptr;

        const VkBufferCopy region{0, 0, size};
        vkCmdCopyBuffer(cmd->GetHandle(), staging->GetHandle(), buffer->GetHandle(), 1, &region);
        if (!cmd->SubmitAndWait(device.GetGraphicsQueue())) {
            DA_ERROR("Staging upload failed.");
            return nullptr;
        }
        return buffer;
    }

    bool VulkanBuffer::Write(const void* data, VkDeviceSize size, VkDeviceSize offset) {
        if (offset + size > m_size) {
            DA_ERROR("VulkanBuffer::Write out of range.");
            return false;
        }
        const VkResult r = vmaCopyMemoryToAllocation(m_allocator, data, m_allocation, offset, size);
        if (r != VK_SUCCESS) {
            DA_ERROR("vmaCopyMemoryToAllocation failed: {}", string_VkResult(r));
            return false;
        }
        return true;
    }

    VulkanBuffer::~VulkanBuffer() {
        if (m_buffer != VK_NULL_HANDLE) vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
    }
} // namespace DA
