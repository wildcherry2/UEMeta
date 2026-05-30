// ReSharper disable CppHidingFunction
#pragma once
#include <clang/Frontend/FrontendAction.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/DeclTemplate.h>
#include <llvm/ADT/DenseSet.h>

#include "UEMeta/JsonBuilders.hpp"

// use multiple jsons for multithreading purposes?
//todo add include order to json
namespace UEMeta {
    class ClangHandler : public clang::ASTFrontendAction, public clang::RecursiveASTVisitor<ClangHandler> {
    public:
        bool shouldVisitTemplateInstantiations() const;
        bool shouldVisitImplicitCode() const;
        bool shouldVisitLambdaBody() const;

        // classes, unions, and structs (C and C++ style)
        bool VisitRecordDecl(clang::RecordDecl* decl);
        bool VisitClassTemplateDecl(clang::ClassTemplateDecl* decl);

        // enums
        bool VisitEnumDecl(clang::EnumDecl* decl);

        // functions (free, methods, ctors, etc), may need to filter out non-free functions since they'll be caught in RecordDecls
        bool VisitFunctionDecl(clang::FunctionDecl* decl);
        bool VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* decl);

        // using statements (`using ty = int`)
        bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl);
        bool VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl* decl);

        // using namespace statements
        //bool VisitUsingDirectiveDecl(clang::UsingDirectiveDecl* decl);

        // variable declarations (global, field, code, etc)
        bool VisitVarDecl(clang::VarDecl* decl);
        bool VisitVarTemplateDecl(clang::VarTemplateDecl* decl);
    protected:
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file) override;

    private:
        void BeginTranslationUnit(clang::ASTContext& ctx);
        void EndTranslationUnit(clang::ASTContext& ctx);

        clang::ASTContext* context{};
        std::vector<JsonDeclaration> declarations{};
        llvm::DenseSet<const clang::Decl*> visited_decls{};
    };
}
