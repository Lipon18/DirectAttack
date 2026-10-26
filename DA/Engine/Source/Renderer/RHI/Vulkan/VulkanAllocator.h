#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <memory>

// Same typedef VMA uses, so we don't have to include its header everywhere.
VK_DEFINE_HANDLE(VmaAllocator)

namespace DA {
    class VulkanInstance;
    class VulkanPhysicalDevice;
    class VulkanDevice;

    class DA_API VulkanAllocator {
    public:
        // Return nullptr on failure (error is logged).
        static std::unique_ptr<VulkanAllocator> Create(const VulkanInstance& instance,
                                                       const VulkanPhysicalDevice& physicalDevice,
                                                       const VulkanDevice& device);
        ~VulkanAllocator();

        VulkanAllocator(const VulkanAllocator&) = delete;
        VulkanAllocator& operator=(const VulkanAllocator&) = delete;

        VmaAllocator GetHandle() const { return m_allocator; }

    private:
        VulkanAllocator() = default;

        VmaAllocator m_allocator = VK_NULL_HANDLE;
    };
} // namespace DA
