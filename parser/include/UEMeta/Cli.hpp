#pragma once
#include <filesystem>
#include <ostream>
#include <string>
#include <utility>
#include <vector>
#include <atomic>
#include <map>
#include <unordered_set>

#include "quill/Logger.h"
#include "quill/LogMacros.h"
#include "quill/bundled/fmt/ranges.h"

#include "UEMeta/StablePath.hpp"

/**
 * @brief Application entry point declared so configuration and logging singletons can restrict initialization.
 */
int main(int argc, char** argv);

namespace UEMeta {
    /**
     * @brief Process-wide CLI configuration used by tool setup.
     */
    class Config {
    public:
        enum class SerializationFormat {
            json,
            binary
        };

        /**
         * @brief Returns the normalized compile_commands.json content.
         */
        [[nodiscard]] const std::string& CompileCommands() const;

        /**
         * @brief Returns the clang or clang-cl executable path.
         */
        [[nodiscard]] const StablePath& ClangPath() const;

        /**
         * @brief Returns compiler arguments appended after compile command filtering.
         */
        [[nodiscard]] const std::unordered_set<std::string>& AdditionalClangArgs() const;

        /**
         * @brief Returns compile command arguments that should be stripped before invoking Clang.
         */
        [[nodiscard]] const std::unordered_set<std::string>& StripArgs() const;

        /**
         * @brief Returns true when the user prefers `clang` over `clang-cl`.
         */
        [[nodiscard]] bool PrefersClang() const;

        /**
         * @brief Returns true when the user wants to dump to stdout.
         */
        [[nodiscard]] bool DumpToStdout() const;

        /**
         * @brief Returns the output format.
         */
        [[nodiscard]] SerializationFormat Format() const;

        /**
         * @brief Returns the set of potential starting points for output paths.
         */
        [[nodiscard]] const std::unordered_set<std::string>& PathBegin() const;

        /**
         * @brief Returns the path to the log file. If empty, no log file should be written to.
         */
        [[nodiscard]] const StablePath& Log();

        /**
         * @brief Returns the directory to output serialized ASTs to.
         */
        [[nodiscard]] const StablePath& OutputDirectory() const;

        /**
         * @brief Writes a human-readable configuration summary to a stream.
         */
        friend std::ostream & operator<<(std::ostream& os, const Config& obj) {
            return os << fmtquill::format("compile_commands={}\nprefer_clang={}\nstrip_commands={}\n"
                                          "additional_clang_args={}\npath_begin={}\nformat={}\nclang_path={}\n"
                                          "log={}\noutput_directory={}", obj.compile_commands, obj.prefer_clang, obj.strip_commands,
                                          obj.additional_clang_args, obj.path_begin, format_string_map.at(obj.format), obj.clang_path.string(), obj.log.string(),
                                          obj.output_directory.string());
        }

        /**
         * @brief Returns the process-wide configuration singleton.
         */
        static Config& GetConfig();

        /**
         * @brief Copy construction is disabled because Config is a singleton.
         */
        Config(const Config& Other) = delete;

        /**
         * @brief Move construction is disabled because Config is a singleton.
         */
        Config(Config&& Other) noexcept = delete;

        /**
         * @brief Copy assignment is disabled because Config is a singleton.
         */
        Config& operator=(const Config& Other) = delete;

        /**
         * @brief Move assignment is disabled because Config is a singleton.
         */
        Config& operator=(Config&& Other) noexcept = delete;

    private:
        friend int ::main(int argc, char** argv);

        /**
         * @brief Constructs default configuration values before CLI parsing.
         */
        Config() = default;

        /**
         * @brief Throws if configuration is read before initialization completes.
         */
        void AssertInitialized() const;

        /**
         * @brief Parses CLI arguments and initializes the process-wide configuration.
         *
         * @param argc Argument count from main.
         * @param argv Argument vector from main.
         * @return 0 on success, otherwise a CLI or initialization error code.
         */
        static int Initialize(int argc, char** argv);

        bool prefer_clang{};
        bool dump_to_stdout{};
        SerializationFormat format = SerializationFormat::json; // will be manipulated in Initialize
        std::unordered_set<std::string> strip_commands{};
        std::unordered_set<std::string> additional_clang_args{};
        std::unordered_set<std::string> path_begin{};
        StablePath clang_path{};
        StablePath log{};
        StablePath output_directory{};
        std::string compile_commands{};

        std::atomic_flag initialized{};

        inline static const std::map<std::string, SerializationFormat> string_format_map = {
            {"json", SerializationFormat::json},
            {"binary", SerializationFormat::binary}
        };

        inline static const std::map<SerializationFormat, std::string> format_string_map = {
            {SerializationFormat::json, "json"},
            {SerializationFormat::binary, "binary"}
        };
    };

    /**
     * @brief Process-wide logger facade used by logging macros and bootstrap code.
     */
    class Logger {
    public:
        /**
         * @brief Returns the initialized Quill logger or a bootstrap fallback logger.
         */
        [[nodiscard]] quill::Logger* GetQuill() const;

        /**
         * @brief Reports whether the main logger has been initialized.
         */
        [[nodiscard]] bool IsInitialized() const;

        /**
         * @brief Returns the process-wide logger singleton.
         */
        static Logger& GetLogger();

        /**
         * @brief Copy construction is disabled because Logger is a singleton.
         */
        Logger(const Logger& Other) = delete;

        /**
         * @brief Move construction is disabled because Logger is a singleton.
         */
        Logger(Logger&& Other) noexcept = delete;

        /**
         * @brief Copy assignment is disabled because Logger is a singleton.
         */
        Logger& operator=(const Logger& Other) = delete;

        /**
         * @brief Move assignment is disabled because Logger is a singleton.
         */
        Logger& operator=(Logger&& Other) noexcept = delete;

    private:
        friend int ::main(int argc, char** argv);

        /**
         * @brief Constructs an uninitialized logger facade.
         */
        Logger() = default;

        /**
         * @brief Throws if the main logger is required before initialization.
         */
        void AssertInitialized() const;

        /**
         * @brief Initializes Quill sinks, formatting, and the process-wide logger.
         *
         * @return 0 on success, otherwise -1.
         */
        static int Initialize();

        quill::Logger* logger{};
    };

    namespace Logging {
        template <typename Message>
        decltype(auto) BuildLogMessage(Message&& message) noexcept {
            return std::forward<Message>(message);
        }

        template <typename Format, typename... Args>
            requires(sizeof...(Args) > 0)
        std::string BuildLogMessage(Format&& format, Args&&... args) {
            return fmtquill::format(fmtquill::runtime(fmtquill::string_view{std::forward<Format>(format)}),
                                    std::forward<Args>(args)...);
        }
    }
}

/**
 * @brief Emits an informational log message through the UEMeta logger.
 */
#define UEM_INFO(...) LOG_INFO(::UEMeta::Logger::GetLogger().GetQuill(), "{}", ::UEMeta::Logging::BuildLogMessage(__VA_ARGS__))

/**
 * @brief Emits a warning log message through the UEMeta logger.
 */
#define UEM_WARN(...) LOG_WARNING(::UEMeta::Logger::GetLogger().GetQuill(), "{}", ::UEMeta::Logging::BuildLogMessage(__VA_ARGS__))

/**
 * @brief Emits a debug log message through the UEMeta logger.
 */
#define UEM_DEBUG(...) LOG_DEBUG(::UEMeta::Logger::GetLogger().GetQuill(), "{}", ::UEMeta::Logging::BuildLogMessage(__VA_ARGS__))

/**
 * @brief Emits an error log message through the UEMeta logger.
 */
#define UEM_ERROR(...) LOG_ERROR(::UEMeta::Logger::GetLogger().GetQuill(), "{}", ::UEMeta::Logging::BuildLogMessage(__VA_ARGS__))
