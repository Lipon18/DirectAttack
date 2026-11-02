#pragma once

#include "Vertex.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace DA {
    struct AABB {
        glm::vec3 Min{std::numeric_limits<float>::max()};
        glm::vec3 Max{std::numeric_limits<float>::lowest()};

        void Expand(const glm::vec3& p) {
            Min = glm::min(Min, p);
            Max = glm::max(Max, p);
        }
        void Expand(const AABB& o) {
            if (!o.IsValid()) return;
            Expand(o.Min);
            Expand(o.Max);
        }
        bool IsValid() const { return Min.x <= Max.x; }
        glm::vec3 Center() const { return (Min + Max) * 0.5f; }
        glm::vec3 Size() const { return Max - Min; }

        // Bounds of the 8 transformed corners (conservative for rotations).
        AABB Transformed(const glm::mat4& m) const {
            AABB r;
            for (int i = 0; i < 8; ++i) {
                const glm::vec3 c{(i & 1) ? Max.x : Min.x, (i & 2) ? Max.y : Min.y, (i & 4) ? Max.z : Min.z};
                r.Expand(glm::vec3(m * glm::vec4(c, 1.0f)));
            }
            return r;
        }
    };

    // Scales and centers `bounds` so its largest side equals `targetSize`, centered on the origin.
    // Models come in every unit system; this makes any of them viewable.
    inline glm::mat4 FitToSize(const AABB& bounds, float targetSize) {
        if (!bounds.IsValid()) return glm::mat4(1.0f);
        const glm::vec3 size = bounds.Size();
        const float maxExtent = std::max({size.x, size.y, size.z});
        const float s = maxExtent > 0.0f ? targetSize / maxExtent : 1.0f;
        return glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(s)), -bounds.Center());
    }

    struct PrimitiveData {
        MeshData Mesh;
        AABB Bounds; // local space
        int32_t Material = -1;
    };

    // One drawn use of a primitive. A glTF mesh referenced by many nodes is stored once and instanced.
    struct ModelInstanceData {
        uint32_t Primitive = 0;
        glm::mat4 Transform{1.0f}; // node world transform
    };

    enum class EAlphaMode {
        EAM_OPAQUE,
        EAM_MASK,
        EAM_BLEND
    };

    // Either encoded bytes (GLB buffer view or data URI) or a resolved file path.
    struct ImageSourceData {
        std::string Name;
        std::string Path;
        std::vector<uint8_t> Encoded;
    };

    struct MaterialData {
        glm::vec4 BaseColorFactor{1.0f};
        glm::vec3 EmissiveFactor{0.0f};
        float Metallic = 1.0f;  // glTF defaults
        float Roughness = 1.0f;
        float AlphaCutoff = 0.5f;
        EAlphaMode Alpha = EAlphaMode::EAM_OPAQUE;
        bool DoubleSided = false;
        // Indices into ModelData::Images, -1 = none.
        int32_t BaseColorImage = -1, NormalImage = -1, MetallicRoughnessImage = -1, OcclusionImage = -1,
                EmissiveImage = -1;
    };

    struct ModelData {
        std::string Name;
        std::vector<PrimitiveData> Primitives;
        std::vector<ModelInstanceData> Instances;
        std::vector<ImageSourceData> Images;
        std::vector<MaterialData> Materials;
        AABB Bounds; // whole model, all instances
    };
} // namespace DA
