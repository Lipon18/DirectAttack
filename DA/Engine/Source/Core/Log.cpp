#include "PCH.h"
#include "Log.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace DA {
    namespace {
        std::shared_ptr<spdlog::logger>& InternalLogger() {
            static std::shared_ptr<spdlog::logger> s_logger;
            return s_logger;
        }

        spdlog::level::level_enum ToSpdlogLevel(ELogLevel level) {
            switch (level) {
            case DA::ELogLevel::ELL_TRACE:
                return spdlog::level::trace;
            case DA::ELogLevel::ELL_DEBUG:
                return spdlog::level::debug;
            case DA::ELogLevel::ELL_INFO:
                return spdlog::level::info;
            case DA::ELogLevel::ELL_WARNING:
                return spdlog::level::warn;
            case DA::ELogLevel::ELL_ERROR:
                return spdlog::level::err;
            case DA::ELogLevel::ELL_FATAL:
                return spdlog::level::critical;
            }
            return spdlog::level::info;
        }
    } // namespace

    void Log::Initialize() {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/DirectAttack.log", true);
        Initialize({consoleSink, fileSink});

       /* auto& logger = InternalLogger();
        if (logger) return;

        try {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("Logs/DirectAttack.log", true);

            std::vector<spdlog::sink_ptr> sinks = {consoleSink, fileSink};

            logger = std::make_shared<spdlog::logger>("DirectAttack", sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::trace);
            logger->set_pattern("[%d.%m.%Y][%^%l%$]%v");
            logger->flush_on(spdlog::level::warn);

            spdlog::register_logger(logger);

            DA_INFO("DirectAttack Initialized.");
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Failed to initialize DirectAttack Logger: " << ex.what() << '\n';
        }*/
    }

    void Log::Shutdown() {
        auto& logger = InternalLogger();
        if (!logger) return;

        DA_INFO("DirectAttack Logger shutting down.");
        logger->flush();
        spdlog::drop("DirectAttack");
        logger.reset();
    }

    void Log::Write(ELogLevel level, std::string_view category, std::string_view message) {
        auto& logger = InternalLogger();
        if (!logger) return;

        logger->log(ToSpdlogLevel(level), "[{}] {}", category, message);
        if (level == ELogLevel::ELL_FATAL) {
            logger->flush();
        }
    }
    void Log::Initialize(const std::vector<spdlog::sink_ptr>& customSinks) {
        auto& logger = InternalLogger();
        if (logger) return;

        try {
            logger = std::make_shared<spdlog::logger>("DirectAttack", customSinks.begin(), customSinks.end());
            logger->set_level(spdlog::level::trace);
            logger->set_pattern("[%d.%m.%Y][%^%l%$][%v"); // Match your pattern
            logger->flush_on(spdlog::level::warn);

            spdlog::register_logger(logger);
        } catch (const spdlog::spdlog_ex& ex) {
            std::cerr << "Failed to initialize DirectAttack Logger: " << ex.what() << '\n';
        }
    }
} // namespace DA
