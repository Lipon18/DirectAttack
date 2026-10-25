#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

namespace DA {
    class VulkanDevice;

    struct VulkanGraphicsPipelineDesc {
        // Full paths to compiled .spv files (use Paths::ResolveEngine).
        std::string VertexShader;
        std::string FragmentShader;

        // Must match the attachment you render into (dynamic rendering has no render pass to infer it from).
        VkFormat ColorFormat = VK_FORMAT_UNDEFINED;
        VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;
        VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
        VkFrontFace FrontFace = VK_FRONT_FACE_CLOCKWISE;
        bool BlendEnable = false;

        // Empty until we have vertex buffers and descriptors.
        std::vector<VkVertexInputBindingDescription> VertexBindings;
        std::vector<VkVertexInputAttributeDescription> VertexAttributes;
        std::vector<VkDescriptorSetLayout> SetLayouts;
        std::vector<VkPushConstantRange> PushConstants;
    };

    class DA_API VulkanGraphicsPipeline {
    public:
        static std::unique_ptr<VulkanGraphicsPipeline> Create(const VulkanDevice& device,
                                                              const VulkanGraphicsPipelineDesc& desc);
        ~VulkanGraphicsPipeline();

        VulkanGraphicsPipeline(const VulkanGraphicsPipeline&) = delete;
        VulkanGraphicsPipeline& operator=(const VulkanGraphicsPipeline&) = delete;

        void Bind(VkCommandBuffer cmd) const { vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline); }

        VkPipeline GetHandle() const { return m_pipeline; }
        VkPipelineLayout GetLayout() const { return m_layout; }

    private:
        explicit VulkanGraphicsPipeline(const VulkanDevice& device) : m_device(device) {}
        bool Init(const VulkanGraphicsPipelineDesc& desc);

        const VulkanDevice& m_device;
        VkPipelineLayout m_layout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
} // namespace DA
