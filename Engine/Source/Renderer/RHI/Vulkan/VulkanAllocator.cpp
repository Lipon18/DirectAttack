#include "PCH.h"
#include "VulkanAllocator.h"
#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "VulkanPhysicalDevice.h"

#include <vulkan/vk_enum_string_helper.h>

#include <include/vk_mem_alloc.h>

namespace DA {
    std::unique_ptr<VulkanAllocator> VulkanAllocator::Create(const VulkanInstance& instance,
                                                             const VulkanPhysicalDevice& physicalDevice,
                                                             const VulkanDevice& device) {
        std::unique_ptr<VulkanAllocator> allocator(new VulkanAllocator());

        VmaAllocatorCreateInfo info{};
        info.instance = instance.GetHandle();
        info.physicalDevice = physicalDevice.GetHandle();
        info.device = device.GetHandle();
        // Older VMA release only know up to 1.3, so clamp.
        info.vulkanApiVersion = std::min(instance.GetApiVersion(), static_cast<uint32_t>(VK_API_VERSION_1_3));
        if (VkResult r = vmaCreateAllocator(&info, &allocator->m_allocator); r != VK_SUCCESS) {
            DA_ERROR("vmaCreateAllocator failed: {}", string_VkResult(r));
            allocator->m_allocator = VK_NULL_HANDLE;
            return nullptr;
        }
        DA_INFO("VMA allocator created.");
        return allocator;
    }

    VulkanAllocator::~VulkanAllocator() {
        if (m_allocator != VK_NULL_HANDLE) vmaDestroyAllocator(m_allocator);
    }
} // namespace DA
