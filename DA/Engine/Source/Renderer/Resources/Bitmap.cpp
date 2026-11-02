#include "PCH.h"
#include "Bitmap.h"
#include <Core/Log.h>

// clang-format off
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
    #ifdef _MSC_VER
        #pragma warning(push, 0)
#endif // _MSC_VER
#include <master/stb_image.h>
#include <master/stb_image_write.h>
    #ifdef _MSC_VER
        #pragma warning(pop)
#endif // _MSC_VER
// clang-format on

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace DA {
    Bitmap::Bitmap(uint32_t width, uint32_t height, PixelFormat format)
        : m_data(size_t(width) * height * BytesPerPixel(format), 0), m_width(width), m_height(height),
          m_format(format) {}

    std::optional<Bitmap> Bitmap::FromMemory(std::span<const uint8_t> encoded, const std::string& debugName) {
        if (encoded.empty() || encoded.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            DA_WARN("Image '{}': empty or too large to decode ({} bytes).", debugName, encoded.size());
            return std::nullopt;
        }
        const stbi_uc* src = encoded.data();
        const int len = static_cast<int>(encoded.size());

        // Header first: rejects garbage cheaply and lets us pick the format before decoding.
        int w = 0, h = 0, comp = 0;
        if (!stbi_info_from_memory(src, len, &w, &h, &comp) || w <= 0 || h <= 0) {
            DA_WARN("Image '{}': not a decodable image ({}).", debugName, stbi_failure_reason());
            return std::nullopt;
        }

        PixelFormat format = PixelFormat::RGBA8;
        void* pixels = nullptr;
        if (stbi_is_hdr_from_memory(src, len)) {
            format = PixelFormat::RGBA32F;
            pixels = stbi_loadf_from_memory(src, len, &w, &h, &comp, 4);
        } else if (stbi_is_16_bit_from_memory(src, len)) {
            format = PixelFormat::RGBA16;
            pixels = stbi_load_16_from_memory(src, len, &w, &h, &comp, 4);
        } else {
            pixels = stbi_load_from_memory(src, len, &w, &h, &comp, 4);
        }
        if (!pixels) {
            DA_WARN("Image '{}': decode failed ({}).", debugName, stbi_failure_reason());
            return std::nullopt;
        }

        Bitmap bitmap;
        bitmap.m_width = static_cast<uint32_t>(w);
        bitmap.m_height = static_cast<uint32_t>(h);
        bitmap.m_format = format;
        const size_t bytes = size_t(w) * size_t(h) * BytesPerPixel(format);
        bitmap.m_data.assign(static_cast<const uint8_t*>(pixels), static_cast<const uint8_t*>(pixels) + bytes);
        stbi_image_free(pixels); // stb's own deallocator, never delete[]
        return bitmap;
    }

    std::optional<Bitmap> Bitmap::FromFile(const std::string& path) {
        std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file) {
            DA_WARN("Image file not found: {}", path);
            return std::nullopt;
        }
        const std::streamsize size = file.tellg();
        if (size <= 0) {
            DA_WARN("Image file is empty: {}", path);
            return std::nullopt;
        }
        std::vector<uint8_t> encoded(static_cast<size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(encoded.data()), size);
        if (!file) {
            DA_WARN("Failed to read image file: {}", path);
            return std::nullopt;
        }
        return FromMemory(encoded, path);
    }

    bool Bitmap::WritePng(const std::string& path) const {
        if (!IsValid() || m_format != PixelFormat::RGBA8) {
            DA_WARN("WritePng('{}'): only valid RGBA8 bitmaps can be written.", path);
            return false;
        }
        std::vector<uint8_t> png;
        auto append = [](void* ctx, void* data, int size) {
            auto* out = static_cast<std::vector<uint8_t>*>(ctx);
            const auto* bytes = static_cast<const uint8_t*>(data);
            out->insert(out->end(), bytes, bytes + size);
        };
        if (!stbi_write_png_to_func(append, &png, int(m_width), int(m_height), 4, m_data.data(), int(m_width) * 4)) {
            DA_WARN("WritePng('{}'): encoding failed.", path);
            return false;
        }

        const std::filesystem::path file(path);
        if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path());
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
        if (!out) {
            DA_WARN("WritePng('{}'): could not write the file.", path);
            return false;
        }
        return true;
    }

    void Bitmap::FlipVertical() {
        const size_t row = size_t(m_width) * BytesPerPixel(m_format);
        for (uint32_t y = 0; y < m_height / 2; ++y) {
            std::swap_ranges(m_data.begin() + y * row, m_data.begin() + (y + 1) * row,
                             m_data.begin() + size_t(m_height - 1 - y) * row);
        }
    }
} // namespace DA
