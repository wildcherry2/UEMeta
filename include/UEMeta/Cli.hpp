#pragma once
#include <filesystem>
#include <vector>

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

        static Config& GetConfig();
    private:
        friend int ::main(int argc, char** argv);
        Config() = default;

        static int Initialize(int argc, char** argv);

        std::filesystem::path cpp_path{}, cc_path{}, out_path{}, clang_path{};
        FileSplitStrategy split_strategy{FileSplitStrategy::Default};
        std::vector<std::filesystem::path> pd_paths{};
    };
}