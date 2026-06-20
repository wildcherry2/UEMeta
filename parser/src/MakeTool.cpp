#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <compare>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "UEMeta/MakeTool.hpp"
#include "UEMeta/Cli.hpp"
#include "UEMeta/StablePath.hpp"

using namespace clang::tooling;

/// @brief One-entry compilation database used after locating the configured source file.
class SingleCommandCompilationDatabase final : public CompilationDatabase {
public:
    /// @brief Stores the compile command selected from compile_commands.json.
    explicit SingleCommandCompilationDatabase(CompileCommand command) : command(std::move(command)) {}

    /// @brief Returns the stored command when Clang asks for the configured source file.
    std::vector<CompileCommand> getCompileCommands(llvm::StringRef file_path) const override {
        const UEMeta::StablePath requested{file_path.str()};
        const UEMeta::StablePath stored{command.Filename};
        if (!std::is_eq(requested <=> stored)) {
            return {};
        }

        return {command};
    }

    /// @brief Returns the single file covered by this database.
    std::vector<std::string> getAllFiles() const override {
        return {command.Filename};
    }

    /// @brief Returns the single compile command covered by this database.
    std::vector<CompileCommand> getAllCompileCommands() const override {
        return {command};
    }

private:
    CompileCommand command;
};

/// @brief Removes configured Unreal build arguments before Clang parses the translation unit.
static CommandLineArguments StripUnneededUnrealBuildArgs(const CommandLineArguments& args) {
    CommandLineArguments out{};
    const auto& ignored_options = UEMeta::Config::GetConfig().StripArgs();

    int skip_count = 0;
    for (const std::string& arg : args) {
        if (skip_count) {
            --skip_count;
            continue;
        }

        if (const auto opt =
          std::ranges::find_if(ignored_options, [&arg](auto& opt) { return opt.starts_with(arg); });
          opt != ignored_options.end()) {
            if (opt->ends_with('}') && opt->length() >= 3) {
                const auto begin_skip_index = opt->find_last_of('{');
                if (begin_skip_index == std::string::npos) continue;
                const auto skip_count_str = opt->substr(begin_skip_index + 1, opt->length() - 1);
                try { skip_count = std::stoi(std::string{skip_count_str}); } catch (...) {
                    skip_count = 0;
                    UEM_WARN("Failed to parse skip argument count '{}' from option '{}'", skip_count_str, *opt);
                    continue;
                }
                UEM_INFO("Skipping {} arguments from option {}", skip_count, arg);
            }
            continue;
        }
        out.push_back(arg);
    }

    return out;
}

/// @brief Loads compile_commands.json and selects the entry for the configured source file.
static std::optional<CompileCommand> LoadCompileCommand(const std::string& cc_path) {
    try {
        const auto& cpp_path = UEMeta::Config::GetConfig().CppPath();
        const auto cpp_path_string = cpp_path.string();

        std::string error{};
        auto db = JSONCompilationDatabase::loadFromFile(cc_path, error, JSONCommandLineSyntax::AutoDetect);
        if (!db) {
            UEM_ERROR("(llvm) Failed to load compile commands at \"{}\": {}", cc_path, error);
            return std::nullopt;
        }

        for (auto command : db->getAllCompileCommands()) {
            const UEMeta::StablePath candidate_path{command.Filename};
            if (!std::is_eq(candidate_path <=> cpp_path)) {
                continue;
            }

            if (command.CommandLine.empty()) {
                UEM_ERROR("(llvm) Found command for file \"{}\", but its command line is empty.", cpp_path_string);
                return std::nullopt;
            }

            return command;
        }

        UEM_ERROR("(llvm) Failed to find matching entry in compile_commands.json for file {}", cpp_path_string);
    } catch (std::exception& ex) {
        UEM_ERROR("(llvm) Failed to load compile commands at \"{}\" with error: {}", cc_path, ex.what());
    } catch (...) {
        UEM_ERROR("(llvm) Failed to load compile commands at \"{}\" with unknown error.", cc_path);
    }
    return std::nullopt;
}

/// @brief Creates the ClangTool and argument adjusters for the configured translation unit.
std::unique_ptr<UEMeta::ToolData> UEMeta::MakeTool() {
    try {
        auto command = LoadCompileCommand(Config::GetConfig().CcPath().string());

        if (!command) return nullptr;
        UEM_INFO("Using compile_commands.json entry for {}", command->Filename);

        std::unique_ptr<CompilationDatabase> selected_db =
            std::make_unique<SingleCommandCompilationDatabase>(std::move(*command));
        std::unique_ptr<CompilationDatabase> db = expandResponseFiles(std::move(selected_db), llvm::vfs::getRealFileSystem());
        if (!db) {
            UEM_ERROR("(llvm) Failed to expand response files from the selected compile command.");
            return nullptr;
        }

        const auto sources = std::vector{Config::GetConfig().CppPath().string()};
        auto tool = std::make_unique<ToolData>(ClangTool{*db, sources}, std::move(db));

        tool->clang_tool.appendArgumentsAdjuster([](const CommandLineArguments& args,...) {
            auto adjusted = args;
            if (adjusted.empty()) {
                UEM_WARN("Selected compile command has no arguments.");
                return adjusted;
            }
            adjusted.front() = Config::GetConfig().ClangPath().string();
            auto out = StripUnneededUnrealBuildArgs(adjusted);
            out.insert_range(out.end(), Config::GetConfig().AdditionalClangArgs());
            return out;
        });
        tool->clang_tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fparse-all-comments"));
        tool->clang_tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-w"));

        return tool;
    } catch (std::exception& ex) {
        UEM_ERROR("(llvm) Failed to make tools with error: {}", ex.what());
    } catch (...) {
        UEM_ERROR("(llvm) Failed to make tools with unknown error!");
    }

    return nullptr;
}
