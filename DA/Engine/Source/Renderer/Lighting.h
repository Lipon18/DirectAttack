#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace DA {
    constexpr uint32_t MaxDynamicLights = 8;

    struct DirectionalLight {
        glm::vec3 Direction{-0.4f, -1.0f, -0.3f}; // the way the light travels (from the sun toward the ground)
        glm::vec3 Color{1.0f, 0.95f, 0.85f};
        float Intensity = 1.0f;
    };

    struct FogSettings {
        bool Enabled = false;
        glm::vec3 Color{0.6f, 0.7f, 0.85f}; // match the sky horizon
        float Start = 40.0f;
        float End = 150.0f;
    };

    struct Environment {
        glm::vec3 Ambient{0.35f, 0.35f, 0.4f};
        DirectionalLight Sun;
        FogSettings Fog;
    };

    struct PointLight {
        glm::vec3 Position{0.0f};
        float Radius = 10.0f; // influence reaches exactly 0 at this distance
        glm::vec3 Color{1.0f};
        float Intensity = 1.0f;
    };

    struct SpotLight {
        glm::vec3 Position{0.0f};
        glm::vec3 Direction{0.0f, -1.0f, 0.0f};
        float Radius = 15.0f;
        glm::vec3 Color{1.0f};
        float Intensity = 1.0f;
        float InnerAngleDeg = 20.0f; // full brightness inside this cone
        float OuterAngleDeg = 30.0f; // zero outside this cone
    };

    // GPU layout, must match `Light` in Lit.frag (std140, 48 bytes).
    // Point lights have CosOuter = -1, which the shader reads as "no cone".
    struct GpuLight {
        glm::vec4 PositionRadius; // xyz = position, w = radius
        glm::vec4 ColorInner;     // rgb = color * intensity, w = cos(inner angle)
        glm::vec4 DirectionOuter; // xyz = direction the light points, w = cos(outer angle)
    };

    inline GpuLight ToGpu(const PointLight& l) {
        return {{l.Position, l.Radius}, {l.Color * l.Intensity, -1.0f}, {0.0f, -1.0f, 0.0f, -1.0f}};
    }

    inline GpuLight ToGpu(const SpotLight& l) {
        const glm::vec3 dir = glm::normalize(l.Direction);
        return {{l.Position, l.Radius},
                {l.Color * l.Intensity, glm::cos(glm::radians(l.InnerAngleDeg))},
                {dir, glm::cos(glm::radians(l.OuterAngleDeg))}};
    }
}; // namespace DA
