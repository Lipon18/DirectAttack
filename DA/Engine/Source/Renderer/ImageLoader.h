#pragma once

#include <Core/DA_API.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace DA {
    struct ImageData {
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<uint8_t> Pixels;
    };

    // Decodes PNG, JPG, BMP, TGA and other formats stb_image supports. Returns nullopt on failure (reason is logged).
    DA_API std::optional<ImageData> LoadImageFile(const std::string& path);
} // namespace DA
