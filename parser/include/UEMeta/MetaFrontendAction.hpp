// ReSharper disable CppHidingFunction
#pragma once
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/FrontendAction.h>
#include <memory>

namespace UEMeta {
    /**
     * @brief Clang frontend action that forwards visited declarations to DeclDb.
     */
    class MetaFrontendAction : public clang::ASTFrontendAction, public clang::RecursiveASTVisitor<MetaFrontendAction> {
    public:
        /// @brief Requests traversal of template instantiations; DeclDb decides which to serialize.
        bool shouldVisitTemplateInstantiations() const;

        /// @brief Skips implicit compiler-generated declarations.
        bool shouldVisitImplicitCode() const;

        /// @brief Skips lambda body traversal.
        bool shouldVisitLambdaBody() const;

        /// @brief Forwards a record declaration to DeclDb and continues traversal.
        bool VisitRecordDecl(clang::RecordDecl* decl);

        /// @brief Forwards an enum declaration to DeclDb and continues traversal.
        bool VisitEnumDecl(clang::EnumDecl* decl);

        /// @brief Forwards a function declaration to DeclDb and continues traversal.
        bool VisitFunctionDecl(clang::FunctionDecl* decl);

        /// @brief Forwards a variable declaration to DeclDb and continues traversal.
        bool VisitVarDecl(clang::VarDecl* decl);

    protected:
        /// @brief Creates the consumer that traverses the AST and waits for serialization to finish.
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file) override;
    };
} // namespace UEMeta
