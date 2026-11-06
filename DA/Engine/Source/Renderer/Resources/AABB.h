#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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