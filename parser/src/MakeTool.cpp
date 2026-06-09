#include <clang/Tooling/JSONCompilationDatabase.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <glaze/glaze.hpp>
#include <compare>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "UEMeta/MakeTool.hpp"
#include "UEMeta/Cli.hpp"

using namespace clang::tooling;

/// @brief Minimal compile_commands.json entry shape needed to rebuild a one-file database.
struct CompileCommandEntry {
    std::string file;
    std::optional<std::string> command;
    std::optional<std::string> directory;
    std::optional<std::string> output;
};

/// @brief Glaze metadata for the compile command fields read from compile_commands.json.
template <>
struct glz::meta<CompileCommandEntry> {
    using T = CompileCommandEntry;

    static constexpr auto value = object(
        "file", &T::file,
        "command", &T::command,
        "directory", &T::directory,
        "output", &T::output
    );
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

/// @brief Loads compile_commands.json and rewrites the configured source entry to use the configured Clang path.
static std::string FixupCommand(const std::string& cc_path) {
    try {
        auto& cpp_path = UEMeta::Config::GetConfig().CppPath();
        auto cpp_path_string = UEMeta::Config::GetConfig().CppPath().string();

        std::ifstream in{cc_path, std::ios::binary};
        if (!in) {
            UEM_ERROR("(fs) Failed to open compile commands at \"{}\"", cc_path);
            return "";
        }

        const std::string json{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        std::vector<CompileCommandEntry> entries;
        constexpr auto read_options = glz::opts{.error_on_unknown_keys = false};
        if (const auto error = glz::read<read_options>(entries, json)) {
            UEM_ERROR("(glaze) Failed to parse compile commands at \"{}\": {}",
                      cc_path, glz::format_error(error, json));
            return "";
        }

        for (const auto& entry : entries) {
            const UEMeta::StablePath candidate_path{entry.file};
            if (!std::is_eq(candidate_path <=> cpp_path))
                continue;

            if (!entry.command) {
                UEM_ERROR("(glaze) Found command for file \"{}\", but it's missing a 'command' field!", cpp_path_string);
                return "";
            }

            const auto& command = *entry.command;
            const auto cmd_start = command.find_first_not_of(" \t");
            if (cmd_start == std::string::npos || cmd_start == command.size() - 1) {
                UEM_ERROR("(glaze) Found command for file \"{}\", but it's not long enough!", cpp_path_string);
                return "";
            }

            std::size_t cmd_end{};
            if (command[cmd_start] == '\"') {
                cmd_end = command.find_first_of('\"', cmd_start + 1);
                if (cmd_end == std::string::npos) {
                    UEM_ERROR("(glaze) Found command for file \"{}\", but it's not long enough!", cpp_path_string);
                    return "";
                }
                ++cmd_end;
            } else {
                cmd_end = command.find_first_of(" \t", cmd_start);
            }

            if (cmd_end == std::string::npos || cmd_end == command.size()) {
                UEM_ERROR("(glaze) Found command for file \"{}\", but it's not long enough!", cpp_path_string);
                return "";
            }
            auto keep = command.substr(cmd_end);
            std::string new_command = UEMeta::Config::GetConfig().ClangPath().string() + keep;

            if (!entry.directory) {
                UEM_ERROR("(glaze) Found command for file \"{}\", but it's missing a 'directory' field!", cpp_path_string);
                return "";
            }

            const auto transformed = std::vector{
                CompileCommandEntry{
                    .file = entry.file,
                    .command = std::move(new_command),
                    .directory = entry.directory,
                    .output = entry.output
                }
            };

            std::string out;
            if (const auto error = glz::write_json(transformed, out)) {
                UEM_ERROR("(glaze) Failed to build transformed compile_commands for file \"{}\": {}",
                          cpp_path_string, glz::format_error(error, out));
                return "";
            }
            return out;
        }

        UEM_ERROR("(glaze) Failed to find matching entry in compile_commands.json for file {}", cpp_path_string);
    } catch (std::exception& ex) {
        UEM_ERROR("(glaze) Failed to load compile commands at \"{}\" with error: {}", cc_path, ex.what());
    } catch (...) {
        UEM_ERROR("(glaze) Failed to load compile commands at \"{}\" with unknown error!", cc_path);
    }
    return "";
}

/// @brief Creates the ClangTool and argument adjusters for the configured translation unit.
std::unique_ptr<UEMeta::ToolData> UEMeta::MakeTool() {
    try {
        std::string error{};
        const auto cc_as_string = FixupCommand(Config::GetConfig().CcPath().string());

        if (cc_as_string.empty()) return nullptr;
        UEM_INFO("Using compile_commands.json entry as {}", cc_as_string);

        std::unique_ptr<JSONCompilationDatabase> json_db = JSONCompilationDatabase::loadFromBuffer(cc_as_string, error, JSONCommandLineSyntax::AutoDetect);
        if (!json_db) {
            UEM_ERROR("(llvm) Failed to load filtered compile_commands with error \"{}\" and JSON:\n{}", error, cc_as_string);
            return nullptr;
        }

        std::unique_ptr<CompilationDatabase> db = expandResponseFiles(std::move(json_db), llvm::vfs::getRealFileSystem());
        if (!db) {
            UEM_ERROR("(llvm) Failed to load filtered compile_commands with error \"{}\" and JSON:\n{}", error, cc_as_string);
            return nullptr;
        }

        const auto sources = std::vector{Config::GetConfig().CppPath().string()};
        auto tool = std::make_unique<ToolData>(ClangTool{*db, sources}, std::move(db));

        tool->clang_tool.appendArgumentsAdjuster([](const CommandLineArguments& args,...) {
            auto out = StripUnneededUnrealBuildArgs(args);
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
