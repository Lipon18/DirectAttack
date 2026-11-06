#include "PCH.h"
#include "Ktx2Loader.h"
#include <Core/Log.h>

#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace DA {
    namespace {
        constexpr uint8_t Magic[12] = {0xAB, 'K', 'T', 'X', ' ', '2', '0', 0xBB, '\r', '\n', 0x1A, '\n'};

        struct BlockInfo {
            uint32_t W, H, Bytes;
        };

        std::optional<BlockInfo> GetBlockInfo(VkFormat f) {
            switch (f) {
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SRGB:
                return BlockInfo{1, 1, 4};
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
            case VK_FORMAT_BC4_UNORM_BLOCK:
                return BlockInfo{4, 4, 8};
            case VK_FORMAT_BC3_UNORM_BLOCK:
            case VK_FORMAT_BC3_SRGB_BLOCK:
            case VK_FORMAT_BC5_UNORM_BLOCK:
            case VK_FORMAT_BC7_UNORM_BLOCK:
            case VK_FORMAT_BC7_SRGB_BLOCK:
                return BlockInfo{4, 4, 16};
            default:
                return std::nullopt;
            }
        }

        // clang-format off
        template <class T> 
        T Read(std::span<const uint8_t> b, size_t offset) {
            T v;
            std::memcpy(&v, b.data() + offset, sizeof(T)); // KTX2 is little-endian, like Windows
            return v;
        }
        // clang-format on
    } // namespace

    bool IsKtx2(std::span<const uint8_t> bytes) {
        return bytes.size() >= sizeof(Magic) && std::memcmp(bytes.data(), Magic, sizeof(Magic)) == 0;
    }

    std::optional<TextureData> ParseKtx2(std::span<const uint8_t> bytes, const std::string& debugName) {
        constexpr size_t LevelIndexOffset = 80;
        if (!IsKtx2(bytes) || bytes.size() < LevelIndexOffset) {
            DA_WARN("KTX2 '{}': not a KTX2 file.", debugName);
            return std::nullopt;
        }
        const auto vkFormat = static_cast<VkFormat>(Read<uint32_t>(bytes, 12));
        const uint32_t width = Read<uint32_t>(bytes, 20);
        const uint32_t height = Read<uint32_t>(bytes, 24);
        const uint32_t depth = Read<uint32_t>(bytes, 28);
        const uint32_t layers = Read<uint32_t>(bytes, 32);
        const uint32_t faces = Read<uint32_t>(bytes, 36);
        const uint32_t levelCount = Read<uint32_t>(bytes, 40);
        const uint32_t supercompression = Read<uint32_t>(bytes, 44);
        if (vkFormat == VK_FORMAT_UNDEFINED) {
            DA_WARN("KTX2 '{}': Basis-compressed, not transcoded. Run `ktx transcode --target bc7`.", debugName);
            return std::nullopt;
        }
        if (supercompression != 0) {
            DA_WARN("KTX2 '{}': supercompression ({}) is not supported. Re-create it without --zstd.", debugName,
                    supercompression);
            return std::nullopt;
        }
        if (depth > 1 || layers > 1 || faces != 1) {
            DA_WARN("KTX2 '{}': only plain 2D textures are supported (no 3D, arrays or cubemaps).", debugName);
            return std::nullopt;
        }
        const auto block = GetBlockInfo(vkFormat);
        if (!block) {
            DA_WARN("KTX2 '{}': unsupported VkFormat {}.", debugName, static_cast<int>(vkFormat));
            return std::nullopt;
        }
        if (width == 0 || height == 0 || width > 16384 || height > 16384) {
            DA_WARN("KTX2 '{}': bad dimensions {}x{}.", debugName, width, height);
            return std::nullopt;
        }
        // levelCount 0 means "generate at load", which only makes sense for uncompressed data.
        if (levelCount == 0 || levelCount > static_cast<uint32_t>(std::bit_width(std::max(width, height)))) {
            DA_WARN("KTX2 '{}': bad level count {}. Create it with --generate-mipmap.", debugName, levelCount);
            return std::nullopt;
        }
        if (bytes.size() < LevelIndexOffset + size_t(levelCount) * 24) {
            DA_WARN("KTX2 '{}': truncated level index.", debugName);
            return std::nullopt;
        }

        TextureData tex;
        tex.Format = vkFormat;
        tex.Width = width;
        tex.Height = height;
        tex.Levels.reserve(levelCount);

        size_t cursor = 0;
        for (uint32_t i = 0; i < levelCount; ++i) {
            const size_t entry = LevelIndexOffset + size_t(i) * 24;
            const uint64_t offset = Read<uint64_t>(bytes, entry);
            const uint64_t length = Read<uint64_t>(bytes, entry + 8);

            const uint64_t w = std::max(1u, width >> i);
            const uint64_t h = std::max(1u, height >> i);
            const uint64_t expected = ((w + block->W - 1) / block->W) * ((h + block->H - 1) / block->H) * block->Bytes;
            if (length != expected || offset > bytes.size() || length > bytes.size() - offset) {
                DA_WARN("KTX2 '{}': level {} has inconsistent size or lies outside the file.", debugName, i);
                return std::nullopt;
            }

            cursor = (cursor + 15) & ~size_t(15); // aligned for any block size
            tex.Levels.push_back({cursor, static_cast<size_t>(length)});
            cursor += static_cast<size_t>(length);
        }

        tex.Data.resize(cursor);
        for (uint32_t i = 0; i < levelCount; ++i) {
            const uint64_t offset = Read<uint64_t>(bytes, LevelIndexOffset + size_t(i) * 24);
            std::memcpy(tex.Data.data() + tex.Levels[i].Offset, bytes.data() + offset, tex.Levels[i].Size);
        }
        return tex;
    }

    std::optional<TextureData> LoadKtx2File(const std::string& path) {
        std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file) return std::nullopt;
        const std::streamsize size = file.tellg();
        if (size <= 0) return std::nullopt;
        std::vector<uint8_t> bytes(static_cast<size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(bytes.data()), size);
        if (!file) return std::nullopt;
        return ParseKtx2(bytes, path);
    }
} // namespace DA
