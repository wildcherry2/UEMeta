// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst
#include "UEMeta/MetaFrontendAction.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>

#include "UEMeta/Cli.hpp"
#include "UEMeta/HeartbeatLogger.hpp"
#include "UEMeta/wrappers/DeclDb.hpp"

namespace {
    class Consumer : public clang::ASTConsumer {
    public:
        Consumer(UEMeta::MetaFrontendAction& owner, std::string tu_name) :
            owner(owner), tu_name(std::move(tu_name)), parse_logger(fmtquill::format("Parsing TU {}...", this->tu_name)) {}

        void Initialize(clang::ASTContext& context) override {
            UEM_INFO("Starting TU '{}' parsing (this may take a moment)!", tu_name);
            auto policy               = context.getPrintingPolicy();
            policy.FullyQualifiedName = true;
            context.setPrintingPolicy(policy);
            parse_logger.start();
        }

        void HandleTranslationUnit(clang::ASTContext& context) override {
            parse_logger.stop();
            UEM_INFO("TU '{}' parsed!", tu_name);
            UEM_INFO("Starting AST traversal...");
            UEMeta::HeartbeatLogger traverse_logger("Traversing AST...");
            traverse_logger.start();
            const bool traversed = owner.TraverseDecl(context.getTranslationUnitDecl());
            traverse_logger.stop();
            if (traversed) {
                UEM_INFO("Finished traversing AST!");
            }
            else {
                UEM_ERROR("(clang) AST traversal aborted.");
            }

            UEMeta::DeclDb::awaitPendingSerializations();
        }

    private:
        UEMeta::MetaFrontendAction& owner;
        std::string                 tu_name;
        UEMeta::HeartbeatLogger     parse_logger;
    };
} // namespace

bool UEMeta::MetaFrontendAction::shouldVisitTemplateInstantiations() const { return true; } // NOLINT(*-convert-member-functions-to-static)

bool UEMeta::MetaFrontendAction::shouldVisitImplicitCode() const { return false; } // NOLINT(*-convert-member-functions-to-static)

bool UEMeta::MetaFrontendAction::shouldVisitLambdaBody() const { return false; } // NOLINT(*-convert-member-functions-to-static)

bool UEMeta::MetaFrontendAction::VisitRecordDecl(clang::RecordDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

bool UEMeta::MetaFrontendAction::VisitEnumDecl(clang::EnumDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

bool UEMeta::MetaFrontendAction::VisitFunctionDecl(clang::FunctionDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

bool UEMeta::MetaFrontendAction::VisitVarDecl(clang::VarDecl* decl) {
    DeclDb::serializeIfNeeded(decl);
    return true;
}

std::unique_ptr<clang::ASTConsumer> UEMeta::MetaFrontendAction::CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file) {
    compiler.getLangOpts().CommentOpts.ParseAllComments = true;
    const auto input_name                               = file.str();
    const auto file_name                                = std::filesystem::path(input_name).filename().string();
    return std::make_unique<Consumer>(*this, file_name.empty() ? input_name : file_name);
}
