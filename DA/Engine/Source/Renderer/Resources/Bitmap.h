#pragma once

#include <Core/DA_API.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace DA {
    enum class PixelFormat {
        RGBA8,
        RGBA16,
        RGBA32F
    };

    constexpr size_t BytesPerPixel(PixelFormat f) {
        switch (f) {
        case DA::PixelFormat::RGBA8:
            return 4;
        case DA::PixelFormat::RGBA16:
            return 8;
        case DA::PixelFormat::RGBA32F:
            return 16;
        }
        return 0;
    }

    // CPU-side image, always 4 channels, rows top-to-bottom, tightly packed. Move-only: images get big.
    class DA_API Bitmap {
    public:
        Bitmap() = default;
        // Zero-filled (transparent black).
        Bitmap(uint32_t width, uint32_t height, PixelFormat format = PixelFormat::RGBA8);

        Bitmap(const Bitmap&) = delete;
        Bitmap& operator=(const Bitmap&) = delete;
        Bitmap(Bitmap&&) noexcept = default;
        Bitmap& operator=(Bitmap&&) noexcept = default;

        // Decodes PNG/JPG/BMP/TGA/... (and Radiance .hdr as RGBA32F; 16-bit PNGs as RGBA16).
        // `debugName` only appears in log messages. Returns nullopt on failure (logged).
        static std::optional<Bitmap> FromMemory(std::span<const uint8_t> encoded, const std::string& debugName);
        static std::optional<Bitmap> FromFile(const std::string& path);

        // 8-bit RGBA only. Returns false on failure (logged).
        bool WritePng(const std::string& path) const;

        void FlipVertical();

        bool IsValid() const { return m_width > 0 && m_height > 0 && !m_data.empty(); }
        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }
        PixelFormat GetFormat() const { return m_format; }
        size_t GetByteSize() const { return m_data.size(); }

        const uint8_t* Data() const { return m_data.data(); }
        uint8_t* Data() { return m_data.data(); }

        // Pixel access for RGBA8 bitmaps (generators, tests). x/y must be in range.
        uint8_t* Pixel8(uint32_t x, uint32_t y) { return m_data.data() + (size_t(y) * m_width + x) * 4; }

    private:
        std::vector<uint8_t> m_data;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        PixelFormat m_format = PixelFormat::RGBA8;
    };
} // namespace DA
