#include "PCH.h"
#include "VulkanMesh.h"
#include <Core/Log.h>

namespace DA {
    std::unique_ptr<VulkanMesh> VulkanMesh::Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                   std::span<const Vertex> vertices,
                                                   std::span<const uint32_t> indices) {
        if (vertices.empty() || indices.empty()) {
            DA_ERROR("VulkanMesh::Create: empty vertex or index data.");
            return nullptr;
        }

        std::unique_ptr<VulkanMesh> mesh(new VulkanMesh());
        mesh->m_vertexBuffer = VulkanBuffer::CreateDeviceLocal(device, allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                               vertices.data(), vertices.size_bytes());
        mesh->m_indexBuffer = VulkanBuffer::CreateDeviceLocal(device, allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                              indices.data(), indices.size_bytes());
        if (!mesh->m_vertexBuffer || !mesh->m_indexBuffer) return nullptr;

        mesh->m_indexCount = static_cast<uint32_t>(indices.size());
        return mesh;
    }

    void VulkanMesh::Bind(VkCommandBuffer cmd) const {
        const VkBuffer vertexBuffer = m_vertexBuffer->GetHandle();
        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT32);
    }

    void VulkanMesh::Draw(VkCommandBuffer cmd) const {
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
    }
} // namespace DA
