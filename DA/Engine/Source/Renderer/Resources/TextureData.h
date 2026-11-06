#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace DA {
    struct TextureLevel {
        size_t Offset = 0; // into TextureData::Data, 16-byte aligned
        size_t Size = 0;
    };

    // GPU-ready texture: format plus premade mip levels (level 0 first). Move-only.
    struct TextureData {
        VkFormat Format = VK_FORMAT_UNDEFINED;
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<TextureLevel> Levels;
        std::vector<uint8_t> Data;
    };

    // The block data is identical for UNORM and SRGB variants, so the material decides the color space.
    inline VkFormat WithColorSpace(VkFormat f, bool srgb) {
        switch (f) {
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:
            return srgb ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
            return srgb ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            return srgb ? VK_FORMAT_BC1_RGBA_SRGB_BLOCK : VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
            return srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
        default:
            return f; // BC4/BC5 have no sRGB variant
        }
    }
} // namespace DA
