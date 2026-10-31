#pragma once

#include <Core/DA_API.h>
#include "Vertex.h"
#include <Renderer/RHI/Vulkan/Memory/VulkanBuffer.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <span>

namespace DA {
    class VulkanDevice;
    class VulkanAllocator;

    // GPU vertex + index buffers. Must outlive any frame in flight that draws it.
    class DA_API VulkanMesh {
    public:
        static std::unique_ptr<VulkanMesh> Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                  std::span<const Vertex> vertices, std::span<const uint32_t> indices);

        VulkanMesh(const VulkanMesh&) = delete;
        VulkanMesh& operator=(const VulkanMesh&) = delete;

        void Bind(VkCommandBuffer cmd) const;
        void Draw(VkCommandBuffer cmd) const;

        uint32_t GetIndexCount() const { return m_indexCount; }

    private:
        VulkanMesh() = default;

        // Both buffers must be destroyed before the allocator (the owner of the mesh guarantees this).
        std::unique_ptr<VulkanBuffer> m_vertexBuffer;
        std::unique_ptr<VulkanBuffer> m_indexBuffer;
        uint32_t m_indexCount = 0;
    };

} // namespace DA
