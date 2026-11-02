#include "PCH.h"
#include "gltfLoader.h"
#include <Core/Log.h>

// clang-format off
// Exactly one .cpp in the project defines the implementation.
#define CGLTF_IMPLEMENTATION
#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif
#include <cgltf.h>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif // _MSC_VER
// clang-format on

#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string_view>

namespace DA {
    namespace {
        struct CgltfDeleter {
            void operator()(cgltf_data* d) const { cgltf_free(d); }
        };
        using CgltfPtr = std::unique_ptr<cgltf_data, CgltfDeleter>;

        const cgltf_attribute* FindAttribute(const cgltf_primitive& prim, cgltf_attribute_type type) {
            for (cgltf_size i = 0; i < prim.attributes_count; ++i) {
                const cgltf_attribute& a = prim.attributes[i];
                if (a.type == type && a.index == 0) return &a;
            }
            return nullptr;
        }

        // Unpacks the whole accessor to floats (handles strides, normalized ints, sparse accessors).
        // Returns the component count per element, or 0 on failure.
        size_t Unpack(const cgltf_accessor& acc, std::vector<float>& scratch) {
            const size_t comps = cgltf_num_components(acc.type);
            const size_t n = acc.count * comps;
            scratch.resize(n);
            return cgltf_accessor_unpack_floats(&acc, scratch.data(), n) >= n ? comps : 0;
        }

        void GenerateNormals(MeshData& mesh) {
            for (Vertex& v : mesh.Vertices)
                v.Normal = glm::vec3(0.0f);
            for (size_t i = 0; i + 2 < mesh.Indices.size(); i += 3) {
                Vertex& a = mesh.Vertices[mesh.Indices[i]];
                Vertex& b = mesh.Vertices[mesh.Indices[i + 1]];
                Vertex& c = mesh.Vertices[mesh.Indices[i + 2]];
                // Unnormalized cross product = area-weighted contribution.
                const glm::vec3 n = glm::cross(b.Position - a.Position, c.Position - a.Position);
                a.Normal += n;
                b.Normal += n;
                c.Normal += n;
            }
            for (Vertex& v : mesh.Vertices) {
                const float len2 = glm::dot(v.Normal, v.Normal);
                v.Normal = len2 > 0.0f ? v.Normal / std::sqrt(len2) : glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        bool ConvertPrimitive(const cgltf_primitive& src, PrimitiveData& out, std::vector<float>& scratch) {
            if (src.type != cgltf_primitive_type_triangles) {
                DA_WARN("glTF: skipping a non-triangle primitive (mode {}).", static_cast<int>(src.type));
                return false;
            }
            const cgltf_attribute* pos = FindAttribute(src, cgltf_attribute_type_position);
            if (!pos || pos->data->count == 0) return false;

            const size_t count = pos->data->count;
            MeshData& mesh = out.Mesh;
            mesh.Vertices.assign(count, Vertex{glm::vec3(0.0f), glm::vec3(0.0f), glm::vec2(0.0f), glm::vec3(1.0f)});

            if (Unpack(*pos->data, scratch) != 3) {
                DA_WARN("glTF: unreadable POSITION accessor.");
                return false;
            }
            for (size_t i = 0; i < count; ++i) {
                const glm::vec3 p{scratch[i * 3], scratch[i * 3 + 1], scratch[i * 3 + 2]};
                mesh.Vertices[i].Position = p;
                out.Bounds.Expand(p);
            }

            bool hasNormals = false;
            if (const cgltf_attribute* a = FindAttribute(src, cgltf_attribute_type_normal);
                a && a->data->count == count) {
                if (Unpack(*a->data, scratch) == 3) {
                    for (size_t i = 0; i < count; ++i)
                        mesh.Vertices[i].Normal = {scratch[i * 3], scratch[i * 3 + 1], scratch[i * 3 + 2]};
                    hasNormals = true;
                }
            }

            if (const cgltf_attribute* a = FindAttribute(src, cgltf_attribute_type_texcoord);
                a && a->data->count == count) {
                if (Unpack(*a->data, scratch) == 2) {
                    for (size_t i = 0; i < count; ++i)
                        mesh.Vertices[i].UV = {scratch[i * 2], scratch[i * 2 + 1]};
                }
            }

            if (const cgltf_attribute* a = FindAttribute(src, cgltf_attribute_type_color);
                a && a->data->count == count) {
                const size_t comps = Unpack(*a->data, scratch); // vec3 or vec4
                if (comps >= 3) {
                    for (size_t i = 0; i < count; ++i)
                        mesh.Vertices[i].Color = {scratch[i * comps], scratch[i * comps + 1], scratch[i * comps + 2]};
                }
            }

            if (src.indices) {
                const size_t n = src.indices->count;
                mesh.Indices.resize(n);
                // Signature of cgltf >= 1.12. Older versions have no out_component_size parameter.
                if (cgltf_accessor_unpack_indices(src.indices, mesh.Indices.data(), sizeof(uint32_t), n) < n) {
                    DA_WARN("glTF: unreadable index accessor.");
                    return false;
                }
            } else {
                mesh.Indices.resize(count);
                for (size_t i = 0; i < count; ++i)
                    mesh.Indices[i] = static_cast<uint32_t>(i);
            }

            if (mesh.Indices.empty() || mesh.Indices.size() % 3 != 0) {
                DA_WARN("glTF: index count {} is not a multiple of 3.", mesh.Indices.size());
                return false;
            }
            uint32_t maxIndex = 0;
            for (uint32_t idx : mesh.Indices)
                maxIndex = std::max(maxIndex, idx);
            if (maxIndex >= count) { // a bad file must not reach the GPU
                DA_WARN("glTF: index {} out of range ({} vertices).", maxIndex, count);
                return false;
            }

            if (!hasNormals) GenerateNormals(mesh);
            return true;
        }

        int32_t TextureImage(const cgltf_data* raw, const cgltf_texture_view& view) {
            if (!view.texture || !view.texture->image) return -1;
            return static_cast<int32_t>(view.texture->image - raw->images);
        }

        MaterialData ConvertMaterial(const cgltf_data* raw, const cgltf_material& m) {
            MaterialData out;
            if (m.has_pbr_metallic_roughness) {
                const auto& p = m.pbr_metallic_roughness;
                out.BaseColorFactor = {p.base_color_factor[0], p.base_color_factor[1], p.base_color_factor[2],
                                       p.base_color_factor[3]};
                out.Metallic = p.metallic_factor;
                out.Roughness = p.roughness_factor;
                out.BaseColorImage = TextureImage(raw, p.base_color_texture);
                out.MetallicRoughnessImage = TextureImage(raw, p.metallic_roughness_texture);
            }
            out.NormalImage = TextureImage(raw, m.normal_texture);
            out.OcclusionImage = TextureImage(raw, m.occlusion_texture);
            out.EmissiveImage = TextureImage(raw, m.emissive_texture);
            out.EmissiveFactor = {m.emissive_factor[0], m.emissive_factor[1], m.emissive_factor[2]};
            if (m.has_emissive_strength) out.EmissiveFactor *= m.emissive_strength.emissive_strength;
            out.Alpha = m.alpha_mode == cgltf_alpha_mode_mask    ? EAlphaMode::EAM_MASK
                        : m.alpha_mode == cgltf_alpha_mode_blend ? EAlphaMode::EAM_BLEND
                                                                 : EAlphaMode::EAM_OPAQUE;
            out.AlphaCutoff = m.alpha_cutoff;
            out.DoubleSided = m.double_sided != 0;
            return out;
        }

        ImageSourceData ConvertImage(const cgltf_options& options, const cgltf_image& img,
                                     const std::filesystem::path& directory) {
            ImageSourceData out;
            out.Name = img.name ? img.name : "";

            if (img.buffer_view) { // GLB-embedded
                const auto* bytes =
                    static_cast<const uint8_t*>(img.buffer_view->buffer->data) + img.buffer_view->offset;
                out.Encoded.assign(bytes, bytes + img.buffer_view->size);
            } else if (img.uri && std::strncmp(img.uri, "data:", 5) == 0) { // data URI
                const char* comma = std::strchr(img.uri, ',');
                if (comma && comma - img.uri >= 7 && std::strncmp(comma - 7, ";base64", 7) == 0) {
                    const char* b64 = comma + 1;
                    const size_t len = std::strlen(b64);
                    size_t pad = 0;
                    while (pad < len && b64[len - 1 - pad] == '=')
                        ++pad;
                    void* decoded = nullptr;
                    const size_t size = len / 4 * 3 - pad;
                    if (cgltf_load_buffer_base64(&options, size, b64, &decoded) == cgltf_result_success) {
                        const auto* bytes = static_cast<const uint8_t*>(decoded);
                        out.Encoded.assign(bytes, bytes + size);
                        std::free(decoded);
                    }
                }
            } else if (img.uri) { // external file, percent-decoded and relative to the model
                std::string uri = img.uri;
                cgltf_decode_uri(uri.data());
                uri.resize(std::strlen(uri.c_str()));
                out.Path = (directory / uri).generic_string();
            }
            return out;
        }
    } // namespace

    DA_API std::optional<ModelData> Loadgltf(const std::string& path) {
        const auto start = std::chrono::steady_clock::now();

        // Note: cgltf opens files with fopen, so non-ASCII Windows paths are a known limitation for now.
        cgltf_options options{};
        cgltf_data* raw = nullptr;
        if (cgltf_result r = cgltf_parse_file(&options, path.c_str(), &raw); r != cgltf_result_success) {
            DA_ERROR("glTF: failed to parse '{}' (cgltf result {}).", path, static_cast<int>(r));
            return std::nullopt;
        }
        CgltfPtr data(raw);

        for (cgltf_size i = 0; i < raw->extensions_required_count; ++i) {
            const std::string_view ext = raw->extensions_required[i];
            if (ext == "KHR_draco_mesh_compression" || ext == "EXT_meshopt_compression") {
                DA_ERROR("glTF: '{}' requires {} which is not supported yet. Re-export it uncompressed.", path, ext);
                return std::nullopt;
            }
        }

        if (cgltf_result r = cgltf_load_buffers(&options, raw, path.c_str()); r != cgltf_result_success) {
            DA_ERROR("glTF: failed to load buffers for '{}' (cgltf result {}).", path, static_cast<int>(r));
            return std::nullopt;
        }
        if (cgltf_result r = cgltf_validate(raw); r != cgltf_result_success) {
            DA_ERROR("glTF: '{}' failed validation (cgltf result {}).", path, static_cast<int>(r));
            return std::nullopt;
        }

        ModelData model;
        model.Name = std::filesystem::path(path).stem().string();
        const std::filesystem::path directory = std::filesystem::path(path).parent_path();
        for (cgltf_size i = 0; i < raw->materials_count; ++i)
            model.Materials.push_back(ConvertMaterial(raw, raw->materials[i]));
        for (cgltf_size i = 0; i < raw->images_count; ++i)
            model.Images.push_back(ConvertImage(options, raw->images[i], directory));

        struct MeshRange {
            uint32_t First = 0;
            uint32_t Count = 0;
            bool Converted = false;
        };
        std::vector<MeshRange> ranges(raw->meshes_count);
        std::vector<float> scratch; // reused for every accessor

        // Collect the nodes of the default scene (or all root nodes if there is no scene) with an explicit stack.
        std::vector<const cgltf_node*> stack;
        if (const cgltf_scene* scene = raw->scene ? raw->scene : (raw->scenes_count > 0 ? &raw->scenes[0] : nullptr)) {
            for (cgltf_size i = 0; i < scene->nodes_count; ++i)
                stack.push_back(scene->nodes[i]);
        } else {
            for (cgltf_size i = 0; i < raw->nodes_count; ++i)
                if (!raw->nodes[i].parent) stack.push_back(&raw->nodes[i]);
        }

        uint64_t instancedTriangles = 0;
        while (!stack.empty()) {
            const cgltf_node* node = stack.back();
            stack.pop_back();
            for (cgltf_size i = 0; i < node->children_count; ++i)
                stack.push_back(node->children[i]);
            if (!node->mesh) continue;

            MeshRange& range = ranges[cgltf_mesh_index(raw, node->mesh)];
            if (!range.Converted) {
                range.Converted = true;
                range.First = static_cast<uint32_t>(model.Primitives.size());
                for (cgltf_size p = 0; p < node->mesh->primitives_count; ++p) {
                    PrimitiveData prim;
                    if (ConvertPrimitive(node->mesh->primitives[p], prim, scratch)) {
                        const cgltf_material* mat = node->mesh->primitives[p].material;
                        prim.Material = mat ? static_cast<int32_t>(mat - raw->materials) : -1;
                        model.Primitives.push_back(std::move(prim));
                        ++range.Count;
                    }
                }
            }
            if (range.Count == 0) continue;

            float m[16];
            cgltf_node_transform_world(node, m);
            const glm::mat4 world = glm::make_mat4(m);
            for (uint32_t i = 0; i < range.Count; ++i) {
                const uint32_t primIndex = range.First + i;
                model.Instances.push_back({primIndex, world});
                model.Bounds.Expand(model.Primitives[primIndex].Bounds.Transformed(world));
                instancedTriangles += model.Primitives[primIndex].Mesh.Indices.size() / 3;
            }
        }

        if (model.Instances.empty()) {
            DA_ERROR("glTF: '{}' contains no triangle geometry.", path);
            return std::nullopt;
        }

        uint64_t vertices = 0;
        uint64_t triangles = 0;
        for (const PrimitiveData& p : model.Primitives) {
            vertices += p.Mesh.Vertices.size();
            triangles += p.Mesh.Indices.size() / 3;
        }
        const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        DA_INFO("glTF '{}': {} primitives, {} instances, {} vertices, {} triangles stored ({} drawn), {:.0f} ms.",
                model.Name, model.Primitives.size(), model.Instances.size(), vertices, triangles, instancedTriangles,
                ms);
        return model;
    }
} // namespace DA
