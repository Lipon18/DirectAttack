#pragma once

#include <Core/DA_API.h>
#include "TextureData.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace DA {
    DA_API bool IsKtx2(std::span<const uint8_t> bytes);
    // Returns nullopt on failure (reason is logged).
    DA_API std::optional<TextureData> ParseKtx2(std::span<const uint8_t> bytes, const std::string& debugName);
    DA_API std::optional<TextureData> LoadKtx2File(const std::string& path);
} // namespace DA
