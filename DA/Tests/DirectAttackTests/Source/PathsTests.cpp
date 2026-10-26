#include <gtest/gtest.h>

#include <Core/Paths.h>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace DA::Tests {
    class PathsTests : public ::testing::Test {
    protected:
        fs::path m_tempRoot;
        fs::path m_engineRoot;
        fs::path m_projectRoot;

        void SetUp() override {
            m_tempRoot = fs::temp_directory_path() / "DA_PathsTests";
            m_engineRoot = m_tempRoot / "Engine";
            m_projectRoot = m_tempRoot / "Project";

            fs::remove_all(m_tempRoot);
            fs::create_directories(m_engineRoot);
            fs::create_directories(m_projectRoot);

            Paths::Init(m_engineRoot, m_projectRoot);
        }

        void TearDown() override { fs::remove_all(m_tempRoot); }

        static std::string Normalize(const fs::path& path) { return path.lexically_normal().generic_string(); }
    };

    TEST_F(PathsTests, ResolveUsesProjectRoot) {
        EXPECT_EQ(Paths::Resolve("Textures/Logo.png"), Normalize(m_projectRoot / "Textures" / "Logo.png"));
    }

    TEST_F(PathsTests, ResolveEngineUsesEngineRoot) {
        EXPECT_EQ(Paths::ResolveEngine("Shaders/Quad.vert.spv"), Normalize(m_engineRoot / "Shaders" / "Quad.vert.spv"));
    }

    TEST_F(PathsTests, ResolveNormalizesDotSegments) {
        EXPECT_EQ(Paths::Resolve("Textures/../Shaders/./Quad.vert.spv"),
                  Normalize(m_projectRoot / "Shaders" / "Quad.vert.spv"));
    }

    TEST_F(PathsTests, ResolveEngineNormalizesDotSegments) {
        EXPECT_EQ(Paths::ResolveEngine("Textures/../Shaders/./Quad.frag.spv"),
                  Normalize(m_engineRoot / "Shaders" / "Quad.frag.spv"));
    }

    TEST_F(PathsTests, ResolvePreservesAbsolutePaths) {
        const fs::path absolute = m_tempRoot / "External" / "asset.png";
        EXPECT_EQ(Paths::Resolve(absolute.generic_string()), absolute.generic_string());
    }

    TEST_F(PathsTests, ResolveEnginePreservesAbsolutePaths) {
        const fs::path absolute = m_tempRoot / "External" / "shader.spv";
        EXPECT_EQ(Paths::ResolveEngine(absolute.generic_string()), absolute.generic_string());
    }

    TEST_F(PathsTests, EmptyProjectRootResolvesRelativeToCurrentDirectory) {
        Paths::Init(m_engineRoot);
        EXPECT_EQ(Paths::Resolve("Assets/Texture.png"), Normalize(fs::path("Assets/Texture.png")));
    }
} // namespace DA::Tests
