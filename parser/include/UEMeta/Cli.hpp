#pragma once
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>
#include <atomic>

#include "quill/Logger.h"
#include "quill/LogMacros.h"
#include "quill/bundled/fmt/ranges.h"

#include "UEMeta/StablePath.hpp"

#ifdef WIN32
/**
 * @brief Default bundled clang-cl executable path on Windows.
 */
#define UEM_DEFAULT_CLANG_CL_PATH std::filesystem::current_path() / "Clang" / "clang-cl.exe"

/**
 * @brief Default bundled clang executable path on Windows.
 */
#define UEM_DEFAULT_CLANG_PATH std::filesystem::current_path() / "Clang" / "clang.exe"
#else
/**
 * @brief Default bundled clang-cl executable path on non-Windows platforms.
 */
#define UEM_DEFAULT_CLANG_CL_PATH std::filesystem::current_path() / "Clang" / "clang-cl"

/**
 * @brief Default bundled clang executable path on non-Windows platforms.
 */
#define UEM_DEFAULT_CLANG_PATH std::filesystem::current_path() / "Clang" / "clang"
#endif

/**
 * @brief Default compiler arguments removed from Unreal compile command entries before Clang runs.
 */
#define UEM_DEFAULT_STRIP_LIST std::vector<std::string>{"/Yu", "/Fp", "/experimental:log{1}"}

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
        /**
         * @brief Returns the C++ translation unit path.
         */
        [[nodiscard]] const StablePath& CppPath() const;

        /**
         * @brief Returns the compile_commands.json path.
         */
        [[nodiscard]] const StablePath& CcPath() const;

        /**
         * @brief Returns the clang or clang-cl executable path.
         */
        [[nodiscard]] const StablePath& ClangPath() const;

        /**
         * @brief Returns compiler arguments appended after compile command filtering.
         */
        [[nodiscard]] const std::vector<std::string>& AdditionalClangArgs() const;

        /**
         * @brief Returns compile command arguments that should be stripped before invoking Clang.
         */
        [[nodiscard]] const std::vector<std::string>& StripArgs() const;

        /**
         * @brief Writes a human-readable configuration summary to a stream.
         */
        friend std::ostream & operator<<(std::ostream& os, const Config& obj) {
            return os << fmtquill::format("cpp_path={}\ncc_path={}"
                                     "\nclang_path={}\nno_cl={}\nadditional_clang_args={}\n"
                                     "strip_args={}",
                obj.cpp_path.string(), obj.cc_path.string(),
                obj.ClangPath().string(), obj.no_cl, obj.additional_clang_args, obj.strip_args);
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

        StablePath cpp_path{}, cc_path{};
        StablePath clang_path{};
        bool no_cl = false;
        std::vector<std::string> additional_clang_args{};
        std::vector<std::string> strip_args{};

        std::atomic_flag initialized{};
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
}

/**
 * @brief Emits an informational log message through the UEMeta logger.
 */
#define UEM_INFO(fmt, ...) LOG_INFO(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)

/**
 * @brief Emits a warning log message through the UEMeta logger.
 */
#define UEM_WARN(fmt, ...) LOG_WARNING(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)

/**
 * @brief Emits a debug log message through the UEMeta logger.
 */
#define UEM_DEBUG(fmt, ...) LOG_DEBUG(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)

/**
 * @brief Emits an error log message through the UEMeta logger.
 */
#define UEM_ERROR(fmt, ...) LOG_ERROR(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)
