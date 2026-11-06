#pragma once

#include <Core/DA_API.h>
#include "ModelData.h"
#include "VulkanMesh.h"
#include "VulkanMaterial.h"

#include <glm/glm.hpp>

#include <memory>
#include <span>
#include <vector>

namespace DA {
    class VulkanDevice;
    class VulkanAllocator;

    // Primitives uploaded once, drawn per instance. Must outlive any frame in flight that draws it.
    class DA_API VulkanModel {
    public:
        struct Instance {
            const VulkanMesh* Mesh = nullptr;
            const VulkanMaterial* Material = nullptr;
            glm::mat4 Transform{1.0f};
        };

        // Consumes `data`: each primitive's CPU copy is freed right after its upload to keep peak memory low.
        static std::unique_ptr<VulkanModel> Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                   ModelData&& data,
                                                   std::vector<std::unique_ptr<VulkanMaterial>> materials);

        // Builds a model from already-uploaded parts (used by the async loader).
        static std::unique_ptr<VulkanModel> Assemble(std::vector<std::unique_ptr<VulkanMesh>> meshes,
                                                     std::vector<std::unique_ptr<VulkanMaterial>> materials,
                                                     std::span<const int32_t> primitiveMaterial,
                                                     std::span<const ModelInstanceData> instances, const AABB& bounds);

        VulkanModel(const VulkanModel&) = delete;
        VulkanModel& operator=(const VulkanModel&) = delete;

        std::span<const Instance> GetInstances() const { return m_instances; }
        const AABB& GetBounds() const { return m_bounds; }

    private:
        VulkanModel() = default;

        std::vector<std::unique_ptr<VulkanMesh>> m_meshes;
        std::vector<std::unique_ptr<VulkanMaterial>> m_materials;
        std::vector<Instance> m_instances;
        AABB m_bounds;
    };
} // namespace DA
