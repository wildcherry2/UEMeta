#include "UEMeta/clang/MetaTool.hpp"

#include <algorithm>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/ArgumentsAdjusters.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/JSONCompilationDatabase.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <stdexcept>
#include <string>
#include <utility>

#include "UEMeta/Cli.hpp"
#include "UEMeta/clang/MetaASTConsumer.hpp"
#include "UEMeta/clang/MetaFrontendAction.hpp"

using namespace clang::tooling;

/// @brief Removes configured Unreal build arguments before Clang parses the translation unit.
CommandLineArguments UEMeta::MetaTool::stripUnneededUnrealBuildArgs(const CommandLineArguments& args) {
    CommandLineArguments out{};
    const auto&          ignored_options = UEMeta::Config::getConfig().getStripArgs();

    int skip_count = 0;
    for (const std::string& arg : args) {
        if (skip_count) {
            --skip_count;
            continue;
        }

        if (const auto opt = std::ranges::find_if(ignored_options, [&arg](auto& opt) { return opt.starts_with(arg); });
            opt != ignored_options.end()) {
            if (opt->ends_with('}') && opt->length() >= 3) {
                const auto begin_skip_index = opt->find_last_of('{');
                if (begin_skip_index == std::string::npos)
                    continue;
                const auto skip_count_str = opt->substr(begin_skip_index + 1, opt->length() - 1);
                try {
                    skip_count = std::stoi(std::string{skip_count_str});
                }
                catch (...) {
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
std::unique_ptr<CompilationDatabase> UEMeta::MetaTool::loadCompileDatabase(const std::string& cc_json) {
    std::string                          error{};
    std::unique_ptr<CompilationDatabase> db = JSONCompilationDatabase::loadFromBuffer(cc_json, error, JSONCommandLineSyntax::AutoDetect);
    if (!db) {
        throw std::runtime_error(fmtquill::format("(llvm) Failed to load compile commands from JSON buffer: {}", error));
    }

    const auto commands = db->getAllCompileCommands();
    if (commands.empty()) {
        throw std::runtime_error("(llvm) compile_commands JSON buffer does not contain any compile commands.");
    }
    for (const auto& command : commands) {
        if (command.CommandLine.empty()) {
            throw std::runtime_error(
                fmtquill::format("(llvm) Found compile command for file \"{}\", but its command line is empty.", command.Filename));
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

/// @brief Creates the ClangTool only when source files are selected instead of an AST cache.
UEMeta::MetaTool::MetaTool() {
    if (!Config::getConfig().getInputASTFile().isEmptyPath())
        return;

    compilation_database = loadCompileDatabase(Config::getConfig().getCompileCommands());
    clang_tool = std::make_unique<ClangTool>(*compilation_database, compilation_database->getAllFiles());
    clang_tool->appendArgumentsAdjuster([](const CommandLineArguments& args, llvm::StringRef) {
        auto adjusted = args;
        if (adjusted.empty()) {
            UEM_WARN("Selected compile command has no arguments.");
            return adjusted;
        }
        const auto& cfg = Config::getConfig();
        // The driver name selects argument syntax inside LibTooling; it is not executed.
        adjusted.front() = cfg.prefersClang() ? "clang" : "clang-cl";
        auto out         = stripUnneededUnrealBuildArgs(adjusted);
        out.insert_range(out.end(), cfg.getAdditionalClangArgs());
        return out;
    });
    clang_tool->appendArgumentsAdjuster(getInsertArgumentAdjuster("-fparse-all-comments"));
    clang_tool->appendArgumentsAdjuster(getInsertArgumentAdjuster("-w"));
}

/// @brief Runs the metadata consumer against the cached AST or a freshly parsed translation unit.
int UEMeta::MetaTool::runClangTool() {
    const auto& ast_file = Config::getConfig().getInputASTFile();
    if (ast_file.isEmptyPath())
        return clang_tool->run(newFrontendActionFactory<MetaFrontendAction>().get());

    const auto ast_path = ast_file.getUnderlyingPath().string();
    UEM_INFO("Loading AST cache '{}'", ast_path);

    // The diagnostics and container reader must outlive the loaded AST.
    clang::CompilerInstance compiler;
    compiler.createVirtualFileSystem();
    compiler.createDiagnostics();
    auto ast = clang::ASTUnit::LoadFromASTFile(
        ast_path, compiler.getPCHContainerReader(), clang::ASTUnit::LoadEverything,
        compiler.getVirtualFileSystemPtr(), nullptr, compiler.getDiagnosticsPtr(),
        compiler.getFileSystemOpts(), compiler.getHeaderSearchOpts());
    if (!ast || ast->getDiagnostics().hasErrorOccurred()) {
        UEM_ERROR("Failed to load AST cache '{}'", ast_path);
        return 1;
    }

    MetaASTConsumer consumer(ast->getOriginalSourceFileName().str());
    consumer.Initialize(ast->getASTContext());
    consumer.HandleTranslationUnit(ast->getASTContext());
    return ast->getDiagnostics().hasErrorOccurred() ? 1 : 0;
}
