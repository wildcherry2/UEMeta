// ReSharper disable CppHidingFunction
#pragma once

#include <string>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include "UEMeta/utility/HeartbeatLogger.hpp"

namespace UEMeta {
    /**
     * @brief Traverses a translation unit and forwards visited declarations to DeclDb.
     */
    class MetaASTConsumer : public clang::ASTConsumer, public clang::RecursiveASTVisitor<MetaASTConsumer> {
    public:
        explicit MetaASTConsumer(std::string tu_name);

        [[nodiscard]] bool shouldVisitTemplateInstantiations() const;

        [[nodiscard]] bool shouldVisitImplicitCode() const;

        [[nodiscard]] bool shouldVisitLambdaBody() const;

        bool VisitRecordDecl(clang::RecordDecl* decl);

        bool VisitEnumDecl(clang::EnumDecl* decl);

        bool VisitFunctionDecl(clang::FunctionDecl* decl);

        bool VisitVarDecl(clang::VarDecl* decl);

        bool VisitNamespaceDecl(clang::NamespaceDecl* decl);

        void Initialize(clang::ASTContext& context) override;

        void HandleTranslationUnit(clang::ASTContext& context) override;

    private:
        std::string     tu_name;
        HeartbeatLogger parse_logger;
    };
} // namespace UEMeta
