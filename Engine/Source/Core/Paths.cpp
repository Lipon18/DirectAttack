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

    std::string Paths::Combine(const fs::path& root, const std::string& relativePath) {
        fs::path rel{relativePath};
        if (rel.has_root_name()) return rel.generic_string();
        if (rel.has_root_directory()) rel = rel.relative_path();
        return (root / rel).lexically_normal().generic_string();
    }
} // namespace DA
