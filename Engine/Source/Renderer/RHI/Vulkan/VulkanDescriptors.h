#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <span>

namespace DA {
    class VulkanDevice;

    class DA_API VulkanDescriptorSetLayout {
    public:
        static std::unique_ptr<VulkanDescriptorSetLayout> Create(
            const VulkanDevice& device, std::span<const VkDescriptorSetLayoutBinding> bindings);

        ~VulkanDescriptorSetLayout();

        VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
        VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;

        VkDescriptorSetLayout GetHandle() const { return m_layout; }

    private:
        explicit VulkanDescriptorSetLayout(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;
        VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    };

    class DA_API VulkanDescriptorPool {
    public:
        static std::unique_ptr<VulkanDescriptorPool> Create(const VulkanDevice& device, uint32_t maxSets,
                                                            std::span<const VkDescriptorPoolSize> sizes);
        ~VulkanDescriptorPool();

        VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
        VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;

        // Returns VK_NULL_HANDLE on failure. Sets are freed when the pool is reset or destroyed.
        VkDescriptorSet Allocate(VkDescriptorSetLayout layout) const;

        VkDescriptorPool GetHandle() const { return m_pool; }

    private:
        explicit VulkanDescriptorPool(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;
        VkDescriptorPool m_pool = VK_NULL_HANDLE;
    };

    // Points `binding` of `set` at a uniform buffer. Do this before the set is used in a submitted command buffer.
    DA_API void WriteUniformBufferDescriptor(const VulkanDevice& device, VkDescriptorSet set, uint32_t binding,
                                             VkBuffer buffer, VkDeviceSize size);

    DA_API void WriteCombinedImageSamplerDescriptor(const VulkanDevice& device, VkDescriptorSet set, uint32_t binding,
                                                    VkImageView view, VkSampler sampler);
} // namespace DA
