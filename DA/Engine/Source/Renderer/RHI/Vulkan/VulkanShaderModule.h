#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>

namespace DA {
    class VulkanDevice;

    class DA_API VulkanShaderModule {
    public:
        static std::unique_ptr<VulkanShaderModule> Create(const VulkanDevice& device, const std::string& spvPath);
        ~VulkanShaderModule();

        VulkanShaderModule(const VulkanShaderModule&) = delete;
        VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;

        VkShaderModule GetHandle() const { return m_module; }

    private:
        explicit VulkanShaderModule(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;
        VkShaderModule m_module = VK_NULL_HANDLE;
    };
} // namespace DA
