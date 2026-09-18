// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst
#include "UEMeta/clang/MetaASTConsumer.hpp"

#include <utility>

#include <clang/AST/ASTContext.h>

#include "UEMeta/Cli.hpp"
#include "UEMeta/clang/DeclDb.hpp"

UEMeta::MetaASTConsumer::MetaASTConsumer(std::string tu_name) :
    tu_name(std::move(tu_name)), parse_logger(fmtquill::format("Parsing TU {}...", this->tu_name)) {}

bool UEMeta::MetaASTConsumer::shouldVisitTemplateInstantiations() const { return true; }

bool UEMeta::MetaASTConsumer::shouldVisitImplicitCode() const { return false; }

bool UEMeta::MetaASTConsumer::shouldVisitLambdaBody() const { return false; }

bool UEMeta::MetaASTConsumer::VisitRecordDecl(clang::RecordDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

bool UEMeta::MetaASTConsumer::VisitEnumDecl(clang::EnumDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

bool UEMeta::MetaASTConsumer::VisitFunctionDecl(clang::FunctionDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

bool UEMeta::MetaASTConsumer::VisitVarDecl(clang::VarDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

void UEMeta::MetaASTConsumer::Initialize(clang::ASTContext& context) {
    UEM_INFO("Starting TU '{}' parsing (this may take a moment)!", tu_name);
    auto policy               = context.getPrintingPolicy();
    policy.FullyQualifiedName = true;
    context.setPrintingPolicy(policy);
    parse_logger.start();
}

void UEMeta::MetaASTConsumer::HandleTranslationUnit(clang::ASTContext& context) {
    parse_logger.stop();
    UEM_INFO("TU '{}' parsed!", tu_name);
    UEM_INFO("Starting AST traversal...");
    HeartbeatLogger traverse_logger("Traversing AST...");
    traverse_logger.start();
    const bool traversed = TraverseDecl(context.getTranslationUnitDecl());
    traverse_logger.stop();
    if (traversed) {
        UEM_INFO("Finished traversing AST!");
    }
    else {
        UEM_ERROR("(clang) AST traversal aborted.");
    }

    DeclDb::serializeForwardDeclarations();
    DeclDb::awaitPendingSerializations();
}
