#pragma once

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace DA {
    struct Vertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 UV;
        glm::vec3 Color;
    };
    
     static_assert(sizeof(Vertex) == 44, "Vertex must be tightly packed to match the pipeline layout");

    // CPU-side mesh, produced by primitive generators now and by loaders (glTF) later.
    struct MeshData {
        std::vector<Vertex> Vertices;
        std::vector<uint32_t> Indices;
    };

    // Pipeline vertex layout matching Vertex (locations 0..3 = position, normal, uv, color).
    inline VkVertexInputBindingDescription VertexBinding() {
        return {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    }

    inline std::vector<VkVertexInputAttributeDescription> VertexAttributes() {
        return {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Position))},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Normal))},
            {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, UV))},
            {3, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Color))},
        };
    }
} // namespace DA
