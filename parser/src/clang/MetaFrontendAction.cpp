#include "UEMeta/clang/MetaFrontendAction.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <clang/Frontend/CompilerInstance.h>

#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "UEMeta/clang/MetaASTConsumer.hpp"
#include "UEMeta/clang/MetaPreprocessor.hpp"
#include "UEMeta/Cli.hpp"

std::unique_ptr<clang::ASTConsumer> UEMeta::MetaFrontendAction::CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef file) {
    const auto input_name = file.str();
    const auto file_name  = std::filesystem::path(input_name).filename().string();

    if (Config::getConfig().getMode() == Config::Mode::Repl || !Config::getConfig().getInputASTFile().isEmptyPath()) {
        return std::make_unique<MetaASTConsumer>(file_name.empty() ? input_name : file_name);
    }

    std::vector<std::unique_ptr<clang::ASTConsumer>> consumers;
    consumers.emplace_back(std::make_unique<MetaASTConsumer>(file_name.empty() ? input_name : file_name));

    std::string sysroot; // windows/linux sdk paths
    if (!clang::GeneratePCHAction::ComputeASTConsumerArguments(ci, sysroot)) {
        throw std::runtime_error("Failed to compute sysroot path for output ast file!");
    }

    const clang::FrontendOptions& frontend_opts = ci.getFrontendOpts();

    if (!frontend_opts.RelocatablePCH) {
        // just following clang's example with emit-ast
        sysroot.clear();
    }

    static std::string ast_out_file = (Config::getConfig().getOutputDirectory().getUnderlyingPath() / "cache.ast").string();
    auto               ast_buffer   = std::make_shared<clang::PCHBuffer>();

    consumers.push_back(std::make_unique<clang::PCHGenerator>(
        ci.getPreprocessor(),
        ci.getModuleCache(),
        ast_out_file,
        sysroot,
        ast_buffer,
        ci.getCodeGenOpts(),
        frontend_opts.ModuleFileExtensions,
        false,
        frontend_opts.IncludeTimestamps,
        frontend_opts.BuildingImplicitModule
    ));

    consumers.push_back(ci.getPCHContainerWriter().CreatePCHContainerGenerator(
        ci,
        std::string(file),
        ast_out_file,
        ci.createOutputFile(ast_out_file, true, false, false),
        ast_buffer
    ));

    return std::make_unique<clang::MultiplexConsumer>(std::move(consumers));
}

bool UEMeta::MetaFrontendAction::PrepareToExecuteAction(clang::CompilerInstance& compiler) {
    compiler.getLangOpts().CommentOpts.ParseAllComments = true;
    compiler.getLangOpts().CompilingPCH                 = true;
    return true;
}

bool UEMeta::MetaFrontendAction::BeginSourceFileAction(clang::CompilerInstance& compiler) {
    if (!ASTFrontendAction::BeginSourceFileAction(compiler))
        return false;
    // The preprocessor is available only once Clang starts the source file.
    if (Config::getConfig().unrealExtensionsEnabled()) {
        compiler.getPreprocessor().addPPCallbacks(std::make_unique<MetaPreprocessor>(compiler.getSourceManager(), compiler.getLangOpts()));
    }
    return true;
}
