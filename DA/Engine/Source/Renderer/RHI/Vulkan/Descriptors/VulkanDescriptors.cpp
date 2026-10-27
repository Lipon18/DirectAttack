#include "PCH.h"
#include "VulkanDescriptors.h"
#include "Renderer/RHI/Vulkan/Core/VulkanDevice.h"

#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    std::unique_ptr<VulkanDescriptorSetLayout> VulkanDescriptorSetLayout::Create(
        const VulkanDevice& device, std::span<const VkDescriptorSetLayoutBinding> bindings) {
        std::unique_ptr<VulkanDescriptorSetLayout> layout(new VulkanDescriptorSetLayout(device));

        VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        info.bindingCount = static_cast<uint32_t>(bindings.size());
        info.pBindings = bindings.data();

        if (VkResult r = vkCreateDescriptorSetLayout(device.GetHandle(), &info, nullptr, &layout->m_layout);
            r != VK_SUCCESS) {
            DA_ERROR("vkCreateDescriptorSetLayout failed: {}", string_VkResult(r));
            layout->m_layout = VK_NULL_HANDLE;
            return nullptr;
        }
        return layout;
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
        if (m_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_device.GetHandle(), m_layout, nullptr);
    }

    std::unique_ptr<VulkanDescriptorPool> VulkanDescriptorPool::Create(const VulkanDevice& device, uint32_t maxSets,
                                                                       std::span<const VkDescriptorPoolSize> sizes) {
        std::unique_ptr<VulkanDescriptorPool> pool(new VulkanDescriptorPool(device));

        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets = maxSets;
        info.poolSizeCount = static_cast<uint32_t>(sizes.size());
        info.pPoolSizes = sizes.data();

        if (VkResult r = vkCreateDescriptorPool(device.GetHandle(), &info, nullptr, &pool->m_pool); r != VK_SUCCESS) {
            DA_ERROR("vkCreateDescriptorPool failed: {}", string_VkResult(r));
            pool->m_pool = VK_NULL_HANDLE;
            return nullptr;
        }
        return pool;
    }

    VkDescriptorSet VulkanDescriptorPool::Allocate(VkDescriptorSetLayout layout) const {
        VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        info.descriptorPool = m_pool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &layout;

        VkDescriptorSet set = VK_NULL_HANDLE;
        if (VkResult r = vkAllocateDescriptorSets(m_device.GetHandle(), &info, &set); r != VK_SUCCESS) {
            DA_ERROR("vkAllocateDescriptorSets failed: {}", string_VkResult(r));
            return VK_NULL_HANDLE;
        }
        return set;
    }

    VulkanDescriptorPool::~VulkanDescriptorPool() {
        // Destroying the pool frees every set allocated from it.
        if (m_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_device.GetHandle(), m_pool, nullptr);
    }

    void WriteUniformBufferDescriptor(const VulkanDevice& device, VkDescriptorSet set, uint32_t binding,
                                      VkBuffer buffer, VkDeviceSize size) {
        VkDescriptorBufferInfo bufferInfo{buffer, 0, size};

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(device.GetHandle(), 1, &write, 0, nullptr);
    }

    void WriteCombinedImageSamplerDescriptor(const VulkanDevice& device, VkDescriptorSet set, uint32_t binding,
        VkImageView view, VkSampler sampler) {
        VkDescriptorImageInfo imageInfo{sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device.GetHandle(), 1, &write, 0, nullptr);
    }
} // namespace DA
