#pragma once
#include <filesystem>
#include <ostream>
#include <vector>

#ifdef WIN32
#define UEM_DEFAULT_CLANG_CL_PATH std::filesystem::current_path() / "Clang" / "clang-cl.exe"
#define UEM_DEFAULT_CLANG_PATH std::filesystem::current_path() / "Clang" / "clang.exe"
#else
#define UEM_DEFAULT_CLANG_CL_PATH std::filesystem::current_path() / "Clang" / "clang-cl"
#define UEM_DEFAULT_CLANG_PATH std::filesystem::current_path() / "Clang" / "clang"
#endif

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
        [[nodiscard]] const std::filesystem::path& CppPath() const;
        [[nodiscard]] const std::filesystem::path& CcPath() const;
        [[nodiscard]] const std::filesystem::path& OutPath() const;
        [[nodiscard]] const FileSplitStrategy& SplitStrategy() const;
        [[nodiscard]] const std::vector<std::filesystem::path>& PdPaths() const;
        [[nodiscard]] const std::filesystem::path& ClangPath() const;

        friend std::ostream & operator<<(std::ostream& os, const Config& obj) {
            std::vector<std::string> pd_paths{};
            for (const auto& path : obj.pd_paths) pd_paths.push_back(path.string());
            return os << std::format("cpp_path={}\ncc_path={}\nout_path={}\nsplit_strategy={}"
                                     "\nclang_path={}\nno_cl={}\npd_paths={}",
                obj.cpp_path.string(), obj.cc_path.string(), obj.out_path.string(), static_cast<int>(obj.split_strategy),
                obj.ClangPath().string(), obj.no_cl,pd_paths);
        }

        static Config& GetConfig();
    private:
        friend int ::main(int argc, char** argv);
        Config() = default;

        static int Initialize(int argc, char** argv);

        std::filesystem::path cpp_path{}, cc_path{}, out_path{};
        std::filesystem::path clang_path = UEM_DEFAULT_CLANG_CL_PATH;
        bool no_cl = false;
        FileSplitStrategy split_strategy{FileSplitStrategy::Default};
        std::vector<std::filesystem::path> pd_paths{};
    };
}