// ReSharper disable CppMemberFunctionMayBeStatic
#include "UEMeta/ClangHandler.hpp"

#include <atomic>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/Tooling.h>
#include "parser.pb.h"

#include "UEMeta/Cli.hpp"
#include "UEMeta/Internal/ClangHelpers.hpp"

using google::protobuf::Arena;
using namespace ParseResult;

/// @brief Requests traversal of template instantiations.
bool UEMeta::ClangHandler::shouldVisitTemplateInstantiations() const { return true; }

/// @brief Skips implicit compiler-generated declarations.
bool UEMeta::ClangHandler::shouldVisitImplicitCode() const { return false; }

/// @brief Skips lambda body traversal.
bool UEMeta::ClangHandler::shouldVisitLambdaBody() const { return false; }

/// @brief Runs Clang with ClangHandler and converts guarded exceptions into a nonzero result.
int UEMeta::RunClangTool(clang::tooling::ClangTool& tool) noexcept {
    GClangExceptionCaught.store(false, std::memory_order_relaxed);
    try {
        const auto result = tool.run(clang::tooling::newFrontendActionFactory<ClangHandler>().get());
        return GClangExceptionCaught.load(std::memory_order_relaxed) ? 1 : result;
    } catch (const std::exception& ex) {
        LogClangException("ClangTool::run", ex);
    } catch (...) {
        LogClangUnknownException("ClangTool::run");
    }

    return 1;
}

/// @brief Logs the start of declaration traversal.
void UEMeta::ClangHandler::BeginTranslationUnit(clang::ASTContext& ctx) {
    GuardClangCallback("BeginTranslationUnit", [&] {
        UEM_INFO("Starting AST traversal...");
        transient_data.context = &ctx;
    });
}

/// @brief Logs the end of declaration traversal.
void UEMeta::ClangHandler::EndTranslationUnit(clang::ASTContext&) {
    GuardClangCallback("EndTranslationUnit", [] {
        UEM_INFO("Finished traversing AST");
    });
}

bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl*) { return true; }

bool UEMeta::ClangHandler::VisitClassTemplateDecl(clang::ClassTemplateDecl*) { return true; }

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* clang_decl) {
    // if this is a forward declaration...
    if (!clang_decl->isComplete()) {
        // and we don't know about it yet...
        if (!transient_data.visited_forward_decls.insert(clang_decl).second) {
            // generate a new forward declaration message and return
            auto* p_forward_decl = Arena::Create<TLForwardDeclaration>(&transient_data.arena);
            p_forward_decl->set_kind(FORWARD_DECLARATION_KIND_ENUM);
            PopulateEnumDetails(transient_data, p_forward_decl->mutable_enum_details(), clang_decl);
            PopulateDeclarationMetadata(transient_data, p_forward_decl->mutable_metadata(), clang_decl);
            p_forward_decl->set_as_string(GetDeclAsString(transient_data, clang_decl));
            FinalizeTL(transient_data, p_forward_decl);
            ++transient_data.occurrence_index;
            return true;
        }
        return true;
    }

    // if this is a complete definition that we've already seen (secondary translation unit), continue
    if (!transient_data.visited_decls.insert(clang_decl->getCanonicalDecl()).second) return true;

    // else, generate a new enum declaration
    auto* p_enum_decl = Arena::Create<TLEnumDeclaration>(&transient_data.arena);
    PopulateDeclarationMetadata(transient_data, p_enum_decl->mutable_metadata(), clang_decl);
    PopulateEnumDetails(transient_data, p_enum_decl->mutable_details(), clang_decl);
    for (auto* enumerator : clang_decl->enumerators()) {
        auto p_enumerator = p_enum_decl->add_enumerators();
        PopulateIdentifier(transient_data, p_enumerator->mutable_identifier(), enumerator);
        p_enumerator->set_value(llvm::toString(enumerator->getInitVal(), 10));
    }
    FinalizeTL(transient_data, p_enum_decl);
    ++transient_data.occurrence_index;
    return true;
}

bool UEMeta::ClangHandler::VisitFunctionDecl(clang::FunctionDecl*) { return true; }

bool UEMeta::ClangHandler::VisitFunctionTemplateDecl(clang::FunctionTemplateDecl*) { return true; }

bool UEMeta::ClangHandler::VisitTypeAliasDecl(clang::TypeAliasDecl*) { return true; }

bool UEMeta::ClangHandler::VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl*) { return true; }

bool UEMeta::ClangHandler::VisitVarDecl(clang::VarDecl*) { return true; }

bool UEMeta::ClangHandler::VisitVarTemplateDecl(clang::VarTemplateDecl*) { return true; }

UEMeta::ClangHandler::ClangHandler() {
    transient_data.results.reserve(10000);
}

/// @brief Creates the AST consumer for one translation unit.
std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                            llvm::StringRef file) {
    try {
        compiler.getLangOpts().CommentOpts.ParseAllComments = true;
        const auto input_name = file.str();
        const auto file_name = std::filesystem::path(input_name).filename().string();
        const auto tu_name = file_name.empty() ? input_name : file_name;
        UEM_INFO("Parsing TU '{}' (this may take a moment)", tu_name);

        /// @brief AST consumer that drives traversal once Clang finishes parsing a translation unit.
        class Consumer : public clang::ASTConsumer {
        public:
            /// @brief Creates a consumer tied to the owning ClangHandler.
            explicit Consumer(ClangHandler* owner, std::string tu_name)
                : owner(owner), tu_name(std::move(tu_name)) {}

            /// @brief Traverses declarations after Clang finishes parsing the translation unit.
            void HandleTranslationUnit(clang::ASTContext& ctx) override {
                GuardClangCallback("HandleTranslationUnit", [&] {
                    UEM_INFO("TU '{}' parsed!", tu_name);
                    owner->BeginTranslationUnit(ctx);
                    if (!owner->TraverseDecl(ctx.getTranslationUnitDecl())) {
                        UEM_ERROR("(clang) AST traversal aborted due to an earlier exception.");
                        return;
                    }
                    owner->EndTranslationUnit(ctx);
                });
            }

        private:
            ClangHandler* owner;
            std::string tu_name;
        };

        return std::make_unique<Consumer>(this, tu_name);
    } catch (const std::exception& ex) {
        LogClangException("CreateASTConsumer", ex);
        throw;
    } catch (...) {
        LogClangUnknownException("CreateASTConsumer");
        throw;
    }
}
