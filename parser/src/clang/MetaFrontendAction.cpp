// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppHidingFunction
#include "UEMeta/clang/MetaFrontendAction.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>

#include "UEMeta/Cli.hpp"
#include "UEMeta/utility/HeartbeatLogger.hpp"
#include "../../include/UEMeta/clang/DeclDb.hpp"

namespace {
    class MetaASTConsumer : public clang::ASTConsumer, public clang::RecursiveASTVisitor<MetaASTConsumer> {
    public:
        explicit MetaASTConsumer(std::string tu_name) :
            tu_name(std::move(tu_name)), parse_logger(fmtquill::format("Parsing TU {}...", this->tu_name)) {}

        [[nodiscard]] bool shouldVisitTemplateInstantiations() const { return true; } // NOLINT(*-convert-member-functions-to-static)

        [[nodiscard]] bool shouldVisitImplicitCode() const { return false; } // NOLINT(*-convert-member-functions-to-static)

        [[nodiscard]] bool shouldVisitLambdaBody() const { return false; } // NOLINT(*-convert-member-functions-to-static)

        bool VisitRecordDecl(clang::RecordDecl* decl) { // NOLINT(*-convert-member-functions-to-static)
            UEMeta::DeclDb::serializeIfNeeded(decl);
            return true;
        }

        bool VisitEnumDecl(clang::EnumDecl* decl) { // NOLINT(*-convert-member-functions-to-static)
            UEMeta::DeclDb::serializeIfNeeded(decl);
            return true;
        }

        bool VisitFunctionDecl(clang::FunctionDecl* decl) { // NOLINT(*-convert-member-functions-to-static)
            UEMeta::DeclDb::serializeIfNeeded(decl);
            return true;
        }

        bool VisitVarDecl(clang::VarDecl* decl) { // NOLINT(*-convert-member-functions-to-static)
            UEMeta::DeclDb::serializeIfNeeded(decl);
            return true;
        }

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
            const bool traversed = TraverseDecl(context.getTranslationUnitDecl());
            traverse_logger.stop();
            if (traversed) {
                UEM_INFO("Finished traversing AST!");
            }
            else {
                UEM_ERROR("(clang) AST traversal aborted.");
            }

            UEMeta::DeclDb::serializeForwardDeclarations();
            UEMeta::DeclDb::awaitPendingSerializations();
        }

    private:
        std::string             tu_name;
        UEMeta::HeartbeatLogger parse_logger;
    };
} // namespace

std::unique_ptr<clang::ASTConsumer> UEMeta::MetaFrontendAction::CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file) {
    compiler.getLangOpts().CommentOpts.ParseAllComments = true;
    const auto input_name                               = file.str();
    const auto file_name                                = std::filesystem::path(input_name).filename().string();
    return std::make_unique<MetaASTConsumer>(file_name.empty() ? input_name : file_name);
}
