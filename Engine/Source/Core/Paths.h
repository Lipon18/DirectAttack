#pragma once

#include "Core/DA_API.h"

#include <filesystem>
#include <string>

namespace DA {
    class DA_API Paths {
    public:
        static void Init(const std::filesystem::path& engineContentRoot,
                         const std::filesystem::path& projectContentRoot = {});

        // Relative to the project content root. Absolute paths pass through untouched.
        static std::string Resolve(const std::string& relativePath);

        // Relative to the engine content root.
        static std::string ResolveEngine(const std::string& relativePath);

        static const std::filesystem::path& GetProjectRoot() { return s_projectRoot; }
        static const std::filesystem::path& GetEngineRoot() { return s_engineRoot; }

    private:
        static std::string Combine(const std::filesystem::path& root, const std::string& relativePath);

        static inline std::filesystem::path s_engineRoot{};
        static inline std::filesystem::path s_projectRoot{};
    };
} // namespace DA
