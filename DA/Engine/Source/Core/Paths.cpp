#include "PCH.h"
#include "Paths.h"
#include "Log.h"

namespace fs = std::filesystem;

namespace DA {
    void Paths::Init(const fs::path& engineContentRoot, const fs::path& projectContentRoot) {
        s_engineRoot = engineContentRoot.lexically_normal();
        s_projectRoot = projectContentRoot.lexically_normal();

        if (!fs::is_directory(s_engineRoot)) {
            DA_ERROR("Engine content root does not exist: [{}]", s_engineRoot.string());
        }
        if (!s_projectRoot.empty() && !fs::is_directory(s_projectRoot)) {
            DA_ERROR("Project content root does not exist: [{}]", s_projectRoot.string());
        }
    }

    std::string Paths::Resolve(const std::string& relativePath) {
        return Combine(s_projectRoot, relativePath);
    }

    std::string Paths::ResolveEngine(const std::string& relativePath) {
        return Combine(s_engineRoot, relativePath);
    }

    std::string Paths::ResolveShader(const std::string& shaderFileName) {
        return (fs::path(DA_SHADER_OUTPUT_DIR) / (shaderFileName + ".spv")).lexically_normal().generic_string();
    }

    std::string Paths::ResolveShader(EShaderLanguage language, const std::string& shaderFileName) {
        fs::path relativePath;
        switch (language) {
        case DA::EShaderLanguage::ESL_GLSL:
            relativePath = fs::path("GLSL") / shaderFileName;
            break;
        case DA::EShaderLanguage::ESL_SLANG:
            relativePath = fs::path("Slang") / shaderFileName;
            relativePath += ".slang";
            break;
        default:
            DA_ERROR("Unknown shader language.");
            return {};
        }
        // Compiled files always have an additional .spv extension.
        relativePath += ".spv";
        return (fs::path(DA_SHADER_OUTPUT_DIR) / relativePath).lexically_normal().generic_string();
    }

    std::string Paths::Combine(const fs::path& root, const std::string& relativePath) {
        /*fs::path rel{relativePath};
        if (rel.has_root_name()) return rel.generic_string();
        if (rel.has_root_directory()) rel = rel.relative_path();
        return (root / rel).lexically_normal().generic_string();*/

        fs::path rel{relativePath};
        if (rel.is_absolute() || rel.has_root_name()) {
            return rel.generic_string();
        }
        if (rel.has_root_directory()) {
            rel = rel.relative_path();
        }
        return (root / rel).lexically_normal().generic_string();
    }
} // namespace DA
