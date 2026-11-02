#include "PCH.h"
#include "VulkanModel.h"
#include <Core/Log.h>

namespace DA {
    std::unique_ptr<VulkanModel> VulkanModel::Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                     ModelData&& data,
                                                     std::vector<std::unique_ptr<VulkanMaterial>> materials) {
        std::unique_ptr<VulkanModel> model(new VulkanModel());
        model->m_materials = std::move(materials);
        model->m_meshes.reserve(data.Primitives.size());
        std::vector<int32_t> primitiveMaterial;
        primitiveMaterial.reserve(data.Primitives.size());

        for (PrimitiveData& prim : data.Primitives) {
            auto mesh = VulkanMesh::Create(device, allocator, prim.Mesh.Vertices, prim.Mesh.Indices);
            if (!mesh) {
                DA_ERROR("VulkanModel '{}': mesh upload failed.", data.Name);
                return nullptr;
            }
            // Free the CPU copy now; vector::clear would keep the capacity.
            std::vector<Vertex>().swap(prim.Mesh.Vertices);
            std::vector<uint32_t>().swap(prim.Mesh.Indices);
            primitiveMaterial.push_back(prim.Material);
            model->m_meshes.push_back(std::move(mesh));
        }

        model->m_instances.reserve(data.Instances.size());
        for (const ModelInstanceData& inst : data.Instances) {
            const int32_t mi = primitiveMaterial[inst.Primitive];
            const VulkanMaterial* material = (mi >= 0 && static_cast<size_t>(mi) < model->m_materials.size())
                                                 ? model->m_materials[mi].get()
                                                 : nullptr;
            model->m_instances.push_back({model->m_meshes[inst.Primitive].get(), material, inst.Transform});
        }
        model->m_bounds = data.Bounds;
        return model;
    }
} // namespace DA
