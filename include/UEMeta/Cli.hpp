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
#define UEM_DEFAULT_CLANG_CL_PATH std::filesystem::current_path() / "Clang" / "clang-cl.exe"
#define UEM_DEFAULT_CLANG_PATH std::filesystem::current_path() / "Clang" / "clang.exe"
#else
#define UEM_DEFAULT_CLANG_CL_PATH std::filesystem::current_path() / "Clang" / "clang-cl"
#define UEM_DEFAULT_CLANG_PATH std::filesystem::current_path() / "Clang" / "clang"
#endif

#define UEM_DEFAULT_STRIP_LIST std::vector<std::string>{"/Yu", "/Fp", "/experimental:log{1}"}

int main(int argc, char** argv);

namespace UEMeta {
    enum class FileSplitStrategy {
        Default,
        ByClass,
        ByParentDirectory,
        ByFile,
        Monofile
    };

    class Config {
    public:
        [[nodiscard]] const StablePath& CppPath() const;
        [[nodiscard]] const StablePath& CcPath() const;
        [[nodiscard]] const StablePath& OutPath() const;
        [[nodiscard]] const FileSplitStrategy& SplitStrategy() const;
        [[nodiscard]] const std::vector<StablePath>& PdPaths() const;
        [[nodiscard]] const StablePath& ClangPath() const;
        [[nodiscard]] const std::vector<std::string>& AdditionalClangArgs() const;
        [[nodiscard]] const std::vector<std::string>& StripArgs() const;
        [[nodiscard]] const std::vector<std::string>& PathDelimiters() const;
        [[nodiscard]] const std::vector<std::string>& PathBlacklist() const;

        friend std::ostream & operator<<(std::ostream& os, const Config& obj) {
            std::vector<std::string> pd_paths{};
            for (const auto& path : obj.pd_paths) pd_paths.push_back(path.string());
            return os << fmtquill::format("cpp_path={}\ncc_path={}\nout_path={}\nsplit_strategy={}"
                                     "\nclang_path={}\nno_cl={}\npd_paths={}\nadditional_clang_args={}\n"
                                     "strip_args={}\npath_delimiters={}\npath_blacklist={}",
                obj.cpp_path.string(), obj.cc_path.string(), obj.out_path.string(), static_cast<int>(obj.split_strategy),
                obj.ClangPath().string(), obj.no_cl, pd_paths, obj.additional_clang_args, obj.strip_args, obj.path_delimiters,
                obj.path_blacklist);
        }

        static Config& GetConfig();

        Config(const Config& Other) = delete;
        Config(Config&& Other) noexcept = delete;
        Config& operator=(const Config& Other) = delete;
        Config& operator=(Config&& Other) noexcept = delete;

    private:
        friend int ::main(int argc, char** argv);
        Config() = default;

        void AssertInitialized() const;
        static int Initialize(int argc, char** argv);

        StablePath cpp_path{}, cc_path{}, out_path{};
        StablePath clang_path{};
        bool no_cl = false;
        FileSplitStrategy split_strategy{FileSplitStrategy::Default};
        std::vector<StablePath> pd_paths{}; //replace with set?
        std::vector<std::string> additional_clang_args{};
        std::vector<std::string> strip_args{};
        std::vector<std::string> path_delimiters{"Unreal", "UnrealEngine"}; //replace with set?
        std::vector<std::string> path_blacklist{}; //replace with set?

        std::atomic_flag initialized{};
    };

    class Logger {
    public:
        [[nodiscard]] quill::Logger* GetQuill() const;
        [[nodiscard]] bool IsInitialized() const;

        static Logger& GetLogger();

        Logger(const Logger& Other) = delete;
        Logger(Logger&& Other) noexcept = delete;
        Logger& operator=(const Logger& Other) = delete;
        Logger& operator=(Logger&& Other) noexcept = delete;

    private:
        friend int ::main(int argc, char** argv);
        Logger() = default;

        void AssertInitialized() const;
        static int Initialize();

        quill::Logger* logger{};
    };
}

#define UEM_INFO(fmt, ...) LOG_INFO(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)
#define UEM_WARN(fmt, ...) LOG_WARNING(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)
#define UEM_DEBUG(fmt, ...) LOG_DEBUG(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)
#define UEM_ERROR(fmt, ...) LOG_ERROR(::UEMeta::Logger::GetLogger().GetQuill(), fmt, ##__VA_ARGS__)
