#include "UEMeta/MetaTool.hpp"

#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <algorithm>
#include <atomic>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

#include "UEMeta/ClangHandler.hpp"
#include "UEMeta/Cli.hpp"
#include "UEMeta/Internal/ClangHelpers.hpp"

using namespace clang::tooling;

/// @brief Removes configured Unreal build arguments before Clang parses the translation unit.
CommandLineArguments UEMeta::MetaTool::StripUnneededUnrealBuildArgs(const CommandLineArguments& args) {
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

/// @brief Loads, validates, and expands the already-filtered compile_commands.json content.
std::unique_ptr<CompilationDatabase> UEMeta::MetaTool::LoadCompileDatabase(const std::string& cc_json) {
    std::string error{};
    std::unique_ptr<CompilationDatabase> db =
        JSONCompilationDatabase::loadFromBuffer(cc_json, error, JSONCommandLineSyntax::AutoDetect);
    if (!db) {
        throw std::runtime_error(fmtquill::format("(llvm) Failed to load compile commands from JSON buffer: {}", error));
    }

    const auto commands = db->getAllCompileCommands();
    if (commands.empty()) {
        throw std::runtime_error("(llvm) compile_commands JSON buffer does not contain any compile commands.");
    }
    for (const auto& command : commands) {
        if (command.CommandLine.empty()) {
            throw std::runtime_error(fmtquill::format(
                "(llvm) Found compile command for file \"{}\", but its command line is empty.", command.Filename));
        }
    }

    UEM_INFO("Using filtered compile_commands JSON buffer ({} bytes)", cc_json.size());

    db = expandResponseFiles(std::move(db), llvm::vfs::getRealFileSystem());
    if (!db) {
        throw std::runtime_error("(llvm) Failed to expand response files from the compile commands.");
    }
    if (db->getAllFiles().empty()) {
        throw std::runtime_error("(llvm) compile_commands JSON buffer does not contain any source files.");
    }

    return db;
}

/// @brief Creates the ClangTool and argument adjusters for the configured compile commands.
UEMeta::MetaTool::MetaTool()
    : compilation_database(LoadCompileDatabase(Config::GetConfig().CompileCommands())),
      clang_tool(*compilation_database, compilation_database->getAllFiles()) {
    clang_tool.appendArgumentsAdjuster([](const CommandLineArguments& args, llvm::StringRef) {
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
    clang_tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-fparse-all-comments"));
    clang_tool.appendArgumentsAdjuster(getInsertArgumentAdjuster("-w"));
}

/// @brief Runs Clang with ClangHandler and converts guarded exceptions into a nonzero result.
int UEMeta::MetaTool::RunClangTool() noexcept {
    GClangExceptionCaught.store(false, std::memory_order_relaxed);
    try {
        const auto result = clang_tool.run(newFrontendActionFactory<ClangHandler>().get());
        return GClangExceptionCaught.load(std::memory_order_relaxed) ? 1 : result;
    } catch (const std::exception& ex) {
        LogClangException("ClangTool::run", ex);
    } catch (...) {
        LogClangUnknownException("ClangTool::run");
    }

    return 1;
}
