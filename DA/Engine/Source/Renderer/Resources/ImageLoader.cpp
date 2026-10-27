#include "PCH.h"
#include "ImageLoader.h"

// Exactly one .cpp in the whole project may define the implementation.
// clang-format off
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif // _MSC_VER
#include <master/stb_image.h>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif // _MSC_VER
// clang-format on

#include <cstring>
#include <filesystem>
#include <fstream>

namespace DA {
    std::optional<ImageData> LoadImageFile(const std::string& path) {
        // Read the file ourselves so non-ASCII Windows paths work (stbi_load uses fopen with a narrow string).
        std::ifstream file(std::filesystem::path(path), std::ios::binary | std::ios::ate);
        if (!file) {
            DA_WARN("Image file not found: {}", path);
            return std::nullopt;
        }
        const std::streamsize fileSize = file.tellg();
        if (fileSize <= 0) {
            DA_WARN("Image file is empty: {}", path);
            return std::nullopt;
        }
        std::vector<stbi_uc> encoded(static_cast<size_t>(fileSize));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(encoded.data()), fileSize);
        if (!file) {
            DA_WARN("Failed to read image file: {}", path);
            return std::nullopt;
        }
        int width = 0;
        int height = 0;
        int channelsInFile = 0;
        // Always ask for 4 channels: missing alpha becomes 255, grayscale expands to RGB.
        stbi_uc* pixels = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height,
                                                &channelsInFile, 4);
        if (!pixels) {
            DA_WARN("stb_image failed to decode '{}': {}", path, stbi_failure_reason());
            return std::nullopt;
        }
        ImageData data;
        data.Width = static_cast<uint32_t>(width);
        data.Height = static_cast<uint32_t>(height);
        data.Pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        std::memcpy(data.Pixels.data(), pixels, data.Pixels.size());
        stbi_image_free(pixels);
        return data;
    }
} // namespace DA
