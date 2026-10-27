#include "PCH.h"
#include "VulkanShaderModule.h"
#include <Renderer/RHI/Vulkan/Core/VulkanDevice.h>

#include <vulkan/vk_enum_string_helper.h>

#include <fstream>

namespace DA {
    std::unique_ptr<VulkanShaderModule> VulkanShaderModule::Create(const VulkanDevice& device,
                                                                   const std::string& spvPath) {
        std::ifstream file(spvPath, std::ios::binary | std::ios::ate);
        if (!file) {
            DA_ERROR("Cannot open shader '{}'. Was it compiled (build the DirectAttackShaders target)?", spvPath);
            return nullptr;
        }
        const std::streamsize size = file.tellg();
        if (size <= 0 || size % 4 != 0) {
            DA_ERROR("Shader '{}' is not valid SPIR-V (size {}).", spvPath, static_cast<long long>(size));
            return nullptr;
        }
        std::vector<uint32_t> code(static_cast<size_t>(size) / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(code.data()), size);

        std::unique_ptr<VulkanShaderModule> module(new VulkanShaderModule(device));
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = static_cast<size_t>(size);
        info.pCode = code.data();
        if (VkResult r = vkCreateShaderModule(device.GetHandle(), &info, nullptr, &module->m_module); r != VK_SUCCESS) {
            DA_ERROR("vkCreateShaderModule failed for '{}': {}", spvPath, string_VkResult(r));
            module->m_module = VK_NULL_HANDLE;
            return nullptr;
        }
        return module;
    }

    VulkanShaderModule::~VulkanShaderModule() {
        if (m_module != VK_NULL_HANDLE) vkDestroyShaderModule(m_device.GetHandle(), m_module, nullptr);
    }
} // namespace DA
