#pragma once

#include <Renderer/RHI/Vulkan/Resources/VulkanImage.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>

namespace DA {
    // Layout of the material push-constant fields (see Lit.frag).
    struct MaterialParams {
        static constexpr uint32_t FlagNormalMap = 1u;
        static constexpr uint32_t FlagAlphaMask = 2u;
        static constexpr uint32_t FlagUnlit = 4u;
        static constexpr uint32_t FlagNoFog = 8u;

        glm::vec4 BaseColor{1.0f};
        glm::vec4 Emissive{0.0f};
        float Metallic = 0.0f;
        float Roughness = 1.0f;
        float AlphaCutoff = 0.5f;
        uint32_t Flags = 0;
    };

    // Created by VulkanRenderer::CreateMaterial. The descriptor set belongs to the renderer's pool.
    struct VulkanMaterial {
        enum Slot : uint32_t {
            BaseColor,
            Normal,
            MetallicRoughness,
            Occlusion,
            Emissive,
            SlotCount
        };

        std::array<std::shared_ptr<VulkanImage>, SlotCount> Textures; // keeps the images alive
        VkDescriptorSet Set = VK_NULL_HANDLE;
        MaterialParams Params;
        bool DoubleSided = false;
        bool Blend = false;
    };
} // namespace DA
