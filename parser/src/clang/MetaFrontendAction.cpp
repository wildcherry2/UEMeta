#include "UEMeta/clang/MetaFrontendAction.hpp"

#include <filesystem>
#include <memory>
#include <string>

#include <clang/Frontend/CompilerInstance.h>

#include "UEMeta/clang/MetaASTConsumer.hpp"
#include "UEMeta/clang/MetaPreprocessor.hpp"
#include "UEMeta/Cli.hpp"

std::unique_ptr<clang::ASTConsumer> UEMeta::MetaFrontendAction::CreateASTConsumer(clang::CompilerInstance&, llvm::StringRef file) {
    const auto input_name                               = file.str();
    const auto file_name                                = std::filesystem::path(input_name).filename().string();
    return std::make_unique<MetaASTConsumer>(file_name.empty() ? input_name : file_name);
}

bool UEMeta::MetaFrontendAction::PrepareToExecuteAction(clang::CompilerInstance& compiler) {
    compiler.getLangOpts().CommentOpts.ParseAllComments = true;
    return true;
}

bool UEMeta::MetaFrontendAction::BeginSourceFileAction(clang::CompilerInstance& compiler) {
    if (!clang::ASTFrontendAction::BeginSourceFileAction(compiler))
        return false;
    // The preprocessor is available only once Clang starts the source file.
    if (Config::getConfig().unrealExtensionsEnabled()) {
        compiler.getPreprocessor().addPPCallbacks(std::make_unique<MetaPreprocessor>(compiler.getSourceManager(), compiler.getLangOpts()));
    }
    return true;
}
