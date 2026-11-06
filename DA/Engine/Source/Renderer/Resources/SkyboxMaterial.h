#pragma once

#include <Core/DA_API.h>
#include "VulkanCubeMap.h"

#include <glm/glm.hpp>

#include <memory>

namespace DA {
    class DA_API SkyboxMaterial {
    public:
        explicit SkyboxMaterial(std::shared_ptr<VulkanCubeMap> image = nullptr,
                                const glm::vec4& baseColor = glm::vec4(1.0f));

        const std::shared_ptr<VulkanCubeMap>& GetImage() const { return m_image; }
        void SetImage(std::shared_ptr<VulkanCubeMap> image) { m_image = std::move(image); }
        const glm::vec4& GetBaseColor() const { return m_baseColor; }
        void SetBaseColor(const glm::vec4& color) { m_baseColor = color; }
        float GetBlend() const { return m_blend; }
        void SetBlend(float blend) { m_blend = glm::clamp(blend, 0.0f, 1.0f); }
        const glm::vec4& GetFogColor() const { return m_fogColor; }
        void SetFogColor(const glm::vec4& color) { m_fogColor = color; }
        const glm::vec2& GetFogLimits() const { return m_fogLimits; }
        void SetFogLimits(const glm::vec2& limits) { m_fogLimits = limits; }

    private:
        std::shared_ptr<VulkanCubeMap> m_image;
        glm::vec4 m_baseColor{1.0f};
        float m_blend = 1.0f;
        glm::vec4 m_fogColor{0.5f, 0.5f, 0.5f, 1.0f};
        glm::vec2 m_fogLimits{-10000.0f, -10000.0f};
    };
} // namespace DA
