#include <gtest/gtest.h>
#include <Core/Log.h>
#include <spdlog/sinks/base_sink.h>

#include <fstream>
#include <vector>
#include <string>
#include <mutex>

struct CapturedLogMessage {
    spdlog::level::level_enum level;
    std::string formattedMessage;
};

template <typename Mutex> class MemorySink : public spdlog::sinks::base_sink<Mutex> {
public:
    std::vector<CapturedLogMessage> messages;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        messages.push_back({msg.level, std::string(formatted.data(), formatted.size())});
    }

    void flush_() override {}
};

using MemorySink_mt = MemorySink<std::mutex>;

class LogTest : public ::testing::Test {
protected:
    void SetUp() override { DA::Log::Initialize(); }

    void TearDown() override { DA::Log::Shutdown(); }
};

class MemoryLogTest : public ::testing::Test {
protected:
    std::shared_ptr<MemorySink_mt> m_memorySink;

    void SetUp() override {
        m_memorySink = std::make_shared<MemorySink_mt>();
        // Initialize Log with ONLY our memory sink for isolated testing
        DA::Log::Initialize({m_memorySink});
    }

    void TearDown() override { DA::Log::Shutdown(); }
};

TEST_F(LogTest, DirectWriteAndPrint) {
    EXPECT_NO_THROW({ DA::Log::Write(DA::ELogLevel::ELL_INFO, "Core", "Direct write test message."); });
    EXPECT_NO_THROW(
        { DA::Log::Print(DA::ELogLevel::ELL_WARNING, "Renderer", "Shader compilation failed with code: {}", 404); });
}

TEST_F(LogTest, LoggingMacros) {
    EXPECT_NO_THROW({
        DA_TRACE("Trace level telemetry: {}", 0.123);
        DA_DEBUG("Debugging physics step: dt={}", 0.016);
        DA_INFO("System initialized successfully.");
        DA_WARN("Memory usage high: {}%", 85);
        DA_ERROR("Failed to load asset: {}", "player_mesh.fbx");
        DA_FATAL("Unrecoverable GPU crash: {}", "Out of VRAM");
    });
}

TEST_F(LogTest, FileSinkOutputVerification) {
    // Generate a unique token to verify disk persistence via the file sink
    std::string uniqueToken = "DA_GTest_Marker_" + std::to_string(rand());
    DA::Log::Print(DA::ELogLevel::ELL_INFO, "TestCategory", "{}", uniqueToken);

    // Shutdown forces a flush to ensure all buffered sinks write out to disk
    DA::Log::Shutdown();

    // Verify the log file contains our unique message
    std::ifstream logFile("Logs/DirectAttack.log");
    ASSERT_TRUE(logFile.is_open()) << "Failed to open DirectAttack.log for verification.";

    std::string line;
    bool foundToken = false;
    while (std::getline(logFile, line)) {
        if (line.find(uniqueToken) != std::string::npos) {
            foundToken = true;
            break;
        }
    }
    logFile.close();

    EXPECT_TRUE(foundToken) << "Unique log marker was not found in the file sink output.";

    // Re-initialize so TearDown() doesn't encounter a null logger state
    DA::Log::Initialize();
}

TEST_F(MemoryLogTest, CapturesExactLogMessagesAndLevels) {
    DA_INFO("Player {} spawned at coordinates ({}, {})", "Hero", 100, 250);
    DA_WARN("Low health warning: {}%", 15);

    ASSERT_EQ(m_memorySink->messages.size(), 2);

    // Verify Info level and content
    EXPECT_EQ(m_memorySink->messages[0].level, spdlog::level::info);
    EXPECT_NE(m_memorySink->messages[0].formattedMessage.find("Player Hero spawned at coordinates (100, 250)"),
              std::string::npos);

    // Verify Warning level and content
    EXPECT_EQ(m_memorySink->messages[1].level, spdlog::level::warn);
    EXPECT_NE(m_memorySink->messages[1].formattedMessage.find("Low health warning: 15%"), std::string::npos);
}

TEST_F(MemoryLogTest, DirectWriteCapturesCategoryAndMessage) {
    DA::Log::Write(DA::ELogLevel::ELL_ERROR, "Physics", "RigidBody collision detection failed.");

    ASSERT_EQ(m_memorySink->messages.size(), 1);
    EXPECT_EQ(m_memorySink->messages[0].level, spdlog::level::err);
    EXPECT_NE(m_memorySink->messages[0].formattedMessage.find("[Physics] RigidBody collision detection failed."),
              std::string::npos);
}