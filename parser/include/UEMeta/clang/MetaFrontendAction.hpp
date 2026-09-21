// ReSharper disable CppHidingFunction
#pragma once
#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <memory>

namespace UEMeta {
    /**
     * @brief Clang frontend action that creates the AST consumer.
     */
    class MetaFrontendAction : public clang::ASTFrontendAction {
    protected:
        /// @brief Creates the consumer that traverses the AST and waits for serialization to finish.
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file) override;

        bool PrepareToExecuteAction(clang::CompilerInstance& CI) override;
        bool BeginSourceFileAction(clang::CompilerInstance& CI) override;
    };
} // namespace UEMeta
