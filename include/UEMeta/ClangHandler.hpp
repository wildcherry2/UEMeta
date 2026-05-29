// ReSharper disable CppHidingFunction
#pragma once
#include <clang/Frontend/FrontendAction.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>

// use multiple jsons for multithreading purposes?

namespace UEMeta {
    class ClangHandler : public clang::ASTFrontendAction, public clang::RecursiveASTVisitor<ClangHandler> {
    public:
        bool shouldVisitTemplateInstantiations() const;
        bool shouldVisitImplicitCode() const;
        bool shouldVisitLambdaBody() const;

        // classes, unions, and structs (C and C++ style)
        //bool VisitRecordDecl(clang::RecordDecl* decl);

        // enums
        //bool VisitEnumDecl(clang::EnumDecl* decl);

        // functions (free, methods, ctors, etc), may need to filter out non-free functions since they'll be caught in RecordDecls
       // bool VisitFunctionDecl(clang::FunctionDecl* decl);

        // using statements (`using ty = int`)
        //bool VisitUsingDecl(clang::UsingDecl* decl);

        // using namespace statements
        //bool VisitUsingDirectiveDecl(clang::UsingDirectiveDecl* decl);

        // variable declarations (global, field, code, etc)
        //bool VisitVarDecl(clang::VarDecl* decl);
    protected:
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file) override;
    };
}