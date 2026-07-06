// ReSharper disable CppHidingFunction
#pragma once
#include <clang/Frontend/FrontendAction.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/AST/DeclTemplate.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/DenseMap.h>
#include <atomic>
#include <filesystem>
#include <memory>
#include <google/protobuf/arena.h>
#include "UEMeta/HeartbeatLogger.hpp"

namespace clang::tooling {
    /**
     * @brief Forward declaration of Clang's command-line tool driver.
     */
    class ClangTool;
}

namespace UEMeta {
    class ASTData;

    /**
     * @brief Runs a configured Clang tool with UEMeta's AST frontend action.
     *
     * @param tool Clang tool to execute.
     * @return Clang's run result, or 1 when visitor/callback exception guards recorded a failure.
     */
    int RunClangTool(clang::tooling::ClangTool& tool) noexcept;

    /**
     * @brief Clang frontend action and AST visitor that converts declarations into UEMeta JSON models.
     */
    class ClangHandler : public clang::ASTFrontendAction, public clang::RecursiveASTVisitor<ClangHandler> {
    public:
        /**
         * @brief Requests traversal of template instantiations.
         *
         * @return True so explicit template instantiations are visited.
         */
        bool shouldVisitTemplateInstantiations() const;

        /**
         * @brief Requests that Clang skip implicit declarations during recursive traversal.
         *
         * @return False to avoid implicit compiler-generated code.
         */
        bool shouldVisitImplicitCode() const;

        /**
         * @brief Requests that lambda bodies be skipped during recursive traversal.
         *
         * @return False because lambda internals are not emitted as top-level metadata.
         */
        bool shouldVisitLambdaBody() const;

        /**
         * @brief Visits C and C++ record declarations and records top-level metadata.
         *
         * @param decl Record declaration supplied by Clang.
         * @return True to continue traversal.
         */
        bool VisitRecordDecl(clang::RecordDecl* decl);

        /**
         * @brief Visits enum declarations and records top-level metadata.
         *
         * @param clang_decl Enum declaration supplied by Clang.
         * @return True to continue traversal.
         */
        bool VisitEnumDecl(clang::EnumDecl* clang_decl);

        /**
         * @brief Visits free function declarations and records top-level metadata.
         *
         * Member functions are emitted while visiting their containing records.
         *
         * @param decl Function declaration supplied by Clang.
         * @return True to continue traversal.
         */
        bool VisitFunctionDecl(clang::FunctionDecl* decl);

        /**
         * @brief Visits top-level type alias declarations.
         *
         * @param decl Type alias declaration supplied by Clang.
         * @return True to continue traversal.
         */
        bool VisitTypeAliasDecl(clang::TypeAliasDecl* decl);

        /**
         * @brief Visits global variable declarations and records top-level metadata.
         *
         * Static data members are emitted while visiting their containing records.
         *
         * @param decl Variable declaration supplied by Clang.
         * @return True to continue traversal.
         */
        bool VisitVarDecl(clang::VarDecl* decl);

        ClangHandler();

    protected:
        /**
         * @brief Creates the AST consumer and preprocessor callbacks for one translation unit.
         *
         * @param compiler Active compiler instance.
         * @param file Translation unit file name reported by Clang.
         * @return Consumer that performs traversal and JSON emission when the translation unit is ready.
         */
        std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler, llvm::StringRef file) override;

    private:
        /**
         * @brief Logs that declaration traversal is starting.
         *
         * @param ctx AST context for the translation unit.
         */
        void BeginTranslationUnit(clang::ASTContext& ctx);

        /**
         * @brief Logs that declaration traversal has completed.
         *
         * @param ctx AST context for the translation unit.
         */
        void EndTranslationUnit(clang::ASTContext& ctx);

        std::unique_ptr<ASTData> data;
        CountingHeartbeatLogger logger;
    };
}
