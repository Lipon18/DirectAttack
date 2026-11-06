#pragma once

#include <Renderer/Resources/AABB.h>

#include <glm/glm.hpp>

namespace DA {
    // Six planes (xyz = inward normal, w = offset). Expects Vulkan-style clip space (depth 0..1).
    struct Frustum {
        glm::vec4 Planes[6];

        static Frustum FromViewProj(const glm::mat4& m) {
            auto row = [&](int r) { return glm::vec4(m[0][r], m[1][r], m[2][r], m[3][r]); };
            const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
            Frustum f;
            f.Planes[0] = r3 + r0; // left
            f.Planes[1] = r3 - r0; // right
            f.Planes[2] = r3 + r1; // bottom/top (the Y flip only swaps which is which)
            f.Planes[3] = r3 - r1;
            f.Planes[4] = r2;      // near (0..1 depth)
            f.Planes[5] = r3 - r2; // far
            for (glm::vec4& p : f.Planes)
                p /= glm::length(glm::vec3(p));
            return f;
        }

        // Conservative: never rejects something visible. Handles any affine model matrix.
        bool Intersects(const AABB& local, const glm::mat4& model) const {
            if (!local.IsValid()) return true;
            const glm::vec3 center = glm::vec3(model * glm::vec4(local.Center(), 1.0f));
            const glm::vec3 ext = local.Size() * 0.5f;
            const glm::vec3 e = glm::abs(glm::vec3(model[0])) * ext.x + glm::abs(glm::vec3(model[1])) * ext.y +
                                glm::abs(glm::vec3(model[2])) * ext.z;
            for (const glm::vec4& p : Planes) {
                const glm::vec3 n{p};
                if (glm::dot(n, center) + p.w + glm::dot(glm::abs(n), e) < 0.0f) return false;
            }
            return true;
        }
    };
} // namespace DA
