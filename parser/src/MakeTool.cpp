#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <algorithm>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "UEMeta/MakeTool.hpp"
#include "UEMeta/Cli.hpp"

using namespace clang::tooling;

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

/// @brief Loads the already-filtered compile_commands.json content.
static std::unique_ptr<CompilationDatabase> LoadCompileDatabase(const std::string& cc_json) {
    try {
        std::string error{};
        auto db = JSONCompilationDatabase::loadFromBuffer(cc_json, error, JSONCommandLineSyntax::AutoDetect);
        if (!db) {
            UEM_ERROR("(llvm) Failed to load compile commands from JSON buffer: {}", error);
            return nullptr;
        }

        const auto commands = db->getAllCompileCommands();
        if (commands.empty()) {
            UEM_ERROR("(llvm) compile_commands JSON buffer does not contain any compile commands.");
            return nullptr;
        }
        for (const auto& command : commands) {
            if (command.CommandLine.empty()) {
                UEM_ERROR("(llvm) Found compile command for file \"{}\", but its command line is empty.", command.Filename);
                return nullptr;
            }
        }

        return db;
    } catch (std::exception& ex) {
        UEM_ERROR("(llvm) Failed to load compile commands from JSON buffer with error: {}", ex.what());
    } catch (...) {
        UEM_ERROR("(llvm) Failed to load compile commands from JSON buffer with unknown error.");
    }
    return nullptr;
}

/// @brief Creates the ClangTool and argument adjusters for the configured compile commands.
std::unique_ptr<UEMeta::ToolData> UEMeta::MakeTool() {
    try {
        const auto& compile_commands_json = Config::GetConfig().CompileCommands();
        auto db = LoadCompileDatabase(compile_commands_json);

        if (!db) return nullptr;
        UEM_INFO("Using filtered compile_commands JSON buffer ({} bytes)", compile_commands_json.size());

        db = expandResponseFiles(std::move(db), llvm::vfs::getRealFileSystem());
        if (!db) {
            UEM_ERROR("(llvm) Failed to expand response files from the compile commands.");
            return nullptr;
        }

        const auto sources = db->getAllFiles();
        if (sources.empty()) {
            UEM_ERROR("(llvm) compile_commands JSON buffer does not contain any source files.");
            return nullptr;
        }

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
