#include "PCH.h"
#include "SkyboxMaterial.h"

namespace DA {
    SkyboxMaterial::SkyboxMaterial(std::shared_ptr<VulkanCubeMap> image, const glm::vec4& baseColor)
        : m_image(std::move(image)), m_baseColor(baseColor) {}
} // namespace DA
