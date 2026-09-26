#pragma once
#include <atomic>
#include <filesystem>
#include <map>
#include <ostream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/bundled/fmt/ranges.h"

#include "utility/StablePath.hpp"

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
        enum class SerializationFormat : uint8_t { Json, Binary };

        enum class Mode : uint8_t { Parser_Cache, Parser_CC, Repl };

        enum class InitializationResult : uint8_t { Success, Help, Fail };

        /**
         * @brief Returns the normalized compile_commands.json content.
         */
        [[nodiscard]] const std::string& getCompileCommands() const;

        /**
         * @brief Returns compiler arguments appended after compile command filtering.
         */
        [[nodiscard]] const std::vector<std::string>& getAdditionalClangArgs() const;

        /**
         * @brief Returns compile command arguments that should be stripped before invoking Clang.
         */
        [[nodiscard]] const std::unordered_set<std::string>& getStripArgs() const;

        /**
         * @brief Selects clang's GNU-style arguments when true, or clang-cl's MSVC-style arguments when false.
         */
        [[nodiscard]] bool prefersClang() const;

        /**
         * @brief Returns true when the user wants the serialized files to indicate a declaration's name instead of the
         * name's hash
         */
        [[nodiscard]] bool prefersFullNameInFileName() const;

        /**
         * @brief Returns true when the user wants synchronous serialization.
         */
        [[nodiscard]] bool syncSerialization() const;

        /**
         * @brief Returns the output format.
         */
        [[nodiscard]] SerializationFormat getFormat() const;

        /**
         * @return The mode that the parser is in.
         */
        [[nodiscard]] Mode getMode() const;

        /**
         * @return The log level
         */
        [[nodiscard]] quill::LogLevel getLogLevel() const;

        /**
         * @brief Returns true when Unreal Engine-specific parsing extensions are enabled.
         */
        [[nodiscard]] bool unrealExtensionsEnabled() const;

        /**
         * @brief Returns the directory name that bounds upward file-system searches.
         * Only applicable to parse commands when unreal extensions are enabled
         */
        [[nodiscard]] const std::filesystem::path::string_type& getFileDelimiter() const;

        /**
         * @brief Returns the path to the log file. If empty, no log file should be written to. (all commands)
         */
        [[nodiscard]] const StablePath& getLog();

        /**
         * @brief Returns the directory to output serialized ASTs to.
         * Defaulted to ./Output on parse commands; may be empty on REPL command,
         * which indicates no output
         */
        [[nodiscard]] const StablePath& getOutputDirectory() const;

        /**
         * @brief Returns the version of files that is being parsed (all commands)
         */
        [[nodiscard]] const std::string& getVersion() const;

        /**
         * @brief Returns the cache.ast to use when the `parse` command is passed with its `ast` subcommand.
         * @return The StablePath of the .ast file
         */
        [[nodiscard]] const StablePath& getInputASTFile() const;
        /**
         * @brief Returns the cache.reflection to use when the `parse ast` command is passed with the optional --refl option
         * @return The StablePath of the .reflection file
         */
        [[nodiscard]] const StablePath& getInputReflFile() const;

        /**
         * @brief Writes a human-readable configuration summary to a stream.
         */
        friend std::ostream& operator<<(std::ostream& os, const Config& obj) { return os << obj.toString(); }

        std::string toString() const {
            return fmtquill::format("compile_commands={}\nprefer_clang={}\nprefer_full_name_in_file_name={}\n"
                                    "sync_serialization={}\nstrip_commands={}\n"
                                    "additional_clang_args={}\nenable_unreal_extensions={}\nfile_delimiter={}\nformat={}\n"
                                    "log={}\noutput_directory={}",
                                    compile_commands, prefer_clang, prefer_full_name_in_file_name, sync_serialization, strip_commands,
                                    additional_clang_args, enable_unreal_extensions, std::filesystem::path{file_delimiter}.string(),
                                    format_string_map.at(format), log.string(), output_directory.string());
        }

#ifdef UEM_TESTING
        void setFormatForTesting(SerializationFormat value) noexcept { format = value; }
        void setUnrealExtensionsForTesting(bool value) noexcept { enable_unreal_extensions = value; }
#endif

        /**
         * @brief Returns the process-wide configuration singleton.
         */
        static Config& getConfig();

        /**
         * @brief Copy construction is disabled because Config is a singleton.
         */
        Config(const Config& other) = delete;

        /**
         * @brief Move construction is disabled because Config is a singleton.
         */
        Config(Config&& other) noexcept = delete;

        /**
         * @brief Copy assignment is disabled because Config is a singleton.
         */
        Config& operator=(const Config& other) = delete;

        /**
         * @brief Move assignment is disabled because Config is a singleton.
         */
        Config& operator=(Config&& other) noexcept = delete;

    private:
        friend int ::main(int argc, char** argv);

        /**
         * @brief Constructs default configuration values before CLI parsing.
         */
        Config() = default;

        /**
         * @brief Throws if configuration is read before initialization completes.
         */
        void assertInitialized() const;

        /**
         * @brief Parses CLI arguments and initializes the process-wide configuration.
         *
         * @param argc Argument count from main.
         * @param argv Argument vector from main.
         * @return InitializationResult indicating result
         */
        static InitializationResult initialize(int argc, char** argv);

        /// @brief Reads a compile_commands.json file or returns inline JSON unchanged.
        static std::string loadCompileCommandsString(const std::string& in);

        bool                               prefer_clang{};
        bool                               sync_serialization{};
        bool                               prefer_full_name_in_file_name{};
        bool                               enable_unreal_extensions{};
        SerializationFormat                format = SerializationFormat::Json; // will be manipulated in initialize
        Mode                               mode{};
        quill::LogLevel                    log_level{};
        std::unordered_set<std::string>    strip_commands{};
        std::vector<std::string>           additional_clang_args{};
        StablePath                         log{};
        StablePath                         output_directory{};
        StablePath                         ast_file{};
        StablePath                         refl_file{};
        std::string                        compile_commands{};
        std::filesystem::path::string_type file_delimiter{std::filesystem::path{"UnrealEngine"}.native()};
        std::string                        version{};
        std::atomic_flag                   initialized{};

        inline static const std::map<std::string, SerializationFormat> string_format_map = {
            {"json", SerializationFormat::Json},
            {"binary", SerializationFormat::Binary}
        };

        inline static const std::map<SerializationFormat, std::string> format_string_map = {
            {SerializationFormat::Json, "json"},
            {SerializationFormat::Binary, "binary"}
        };

        inline static const std::map<std::string, quill::LogLevel> string_loglevel_map = {
            {"info", quill::LogLevel::Info},
            {"debug", quill::LogLevel::Debug},
            {"error", quill::LogLevel::Error},
            {"warn", quill::LogLevel::Warning},
            {"trace", quill::LogLevel::TraceL1},
            {"disabled", quill::LogLevel::None}
        };

        inline static const std::map<quill::LogLevel, std::string> log_level_map = {
            {quill::LogLevel::Info, "info"},
            {quill::LogLevel::Debug, "debug"},
            {quill::LogLevel::Error, "error"},
            {quill::LogLevel::Warning, "warn"},
            {quill::LogLevel::TraceL1, "trace"},
            {quill::LogLevel::None, "disabled"}
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
        [[nodiscard]] quill::Logger* getQuill() const;

        /**
         * @brief Reports whether the main logger has been initialized.
         */
        [[nodiscard]] bool isInitialized() const;

        /**
         * @brief Returns the process-wide logger singleton.
         */
        static Logger& getLogger();

        /**
         * @brief Copy construction is disabled because Logger is a singleton.
         */
        Logger(const Logger& other) = delete;

        /**
         * @brief Move construction is disabled because Logger is a singleton.
         */
        Logger(Logger&& other) noexcept = delete;

        /**
         * @brief Copy assignment is disabled because Logger is a singleton.
         */
        Logger& operator=(const Logger& other) = delete;

        /**
         * @brief Move assignment is disabled because Logger is a singleton.
         */
        Logger& operator=(Logger&& other) noexcept = delete;

    private:
        friend int ::main(int argc, char** argv);

        /**
         * @brief Constructs an uninitialized logger facade.
         */
        Logger() = default;

        /**
         * @brief Throws if the main logger is required before initialization.
         */
        void assertInitialized() const;

        /**
         * @brief Initializes Quill sinks, formatting, and the process-wide logger.
         *
         * @return 0 on success, otherwise -1.
         */
        static int initialize();

        quill::Logger* logger{};
    };

    namespace Logging {
        template <typename Message>
        decltype(auto) buildLogMessage(Message&& message) noexcept {
            return std::forward<Message>(message);
        }

        template <typename Format, typename... Args>
            requires(sizeof...(Args) > 0)
        std::string buildLogMessage(Format&& format, Args&&... args) {
            return fmtquill::format(fmtquill::runtime(fmtquill::string_view{std::forward<Format>(format)}), std::forward<Args>(args)...);
        }
    } // namespace Logging
}     // namespace UEMeta

/**
 * @brief Emits an informational log message through the UEMeta logger.
 */
#define UEM_INFO(...) LOG_INFO(::UEMeta::Logger::getLogger().getQuill(), "{}", ::UEMeta::Logging::buildLogMessage(__VA_ARGS__))

/**
 * @brief Emits a warning log message through the UEMeta logger.
 */
#define UEM_WARN(...) LOG_WARNING(::UEMeta::Logger::getLogger().getQuill(), "{}", ::UEMeta::Logging::buildLogMessage(__VA_ARGS__))

/**
 * @brief Emits a debug log message through the UEMeta logger.
 */
#ifdef DEBUG
#define UEM_DEBUG(...) LOG_DEBUG(::UEMeta::Logger::getLogger().getQuill(), "{}", ::UEMeta::Logging::buildLogMessage(__VA_ARGS__))
#else
#define UEM_DEBUG(...)
#endif
/**
 * @brief Emits an error log message through the UEMeta logger.
 */
#define UEM_ERROR(...) LOG_ERROR(::UEMeta::Logger::getLogger().getQuill(), "{}", ::UEMeta::Logging::buildLogMessage(__VA_ARGS__))
