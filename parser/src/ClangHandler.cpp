// ReSharper disable CppMemberFunctionMayBeStatic
#include "UEMeta/ClangHandler.hpp"

#include <atomic>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/AST/RecordLayout.h>
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

// CXXRecordDecl->getDescribedClassTemplate() can tell you if you have a templated c++ class,
// RecordDecl->getDescribedTemplate() can tell you without casting to c++ type
bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl* clang_decl) {
    if (OnVisit(clang_decl)) return true;

    auto* p_record_decl = Arena::Create<TLRecordDeclaration>(&transient_data.arena);
    PopulateDeclarationMetadata(transient_data, p_record_decl->mutable_metadata(), clang_decl);
    PopulateTemplateDetails(transient_data, p_record_decl->mutable_template_details(), clang_decl);
    p_record_decl->set_is_complete_definition(clang_decl->isCompleteDefinition());
    auto& layout = transient_data.context->getASTRecordLayout(clang_decl);
    p_record_decl->set_align_bytes(layout.getAlignment().getQuantity());
    p_record_decl->set_size_bytes(layout.getSize().getQuantity());
    p_record_decl->set_kind(clang_decl->isClass() ? RECORD_KIND_CLASS : clang_decl->isStruct() ? RECORD_KIND_STRUCT : RECORD_KIND_UNION);
    for (const auto* field : clang_decl->fields()) {
        auto* p_field = p_record_decl->add_fields();
        p_field->set_access(field->getAccess() == clang::AS_public ? ACCESS_SPECIFIER_PUBLIC
            : field->getAccess() == clang::AS_private ? ACCESS_SPECIFIER_PRIVATE : ACCESS_SPECIFIER_PROTECTED);
        PopulateIdentifier(transient_data, p_field->mutable_identifier(), field);
    }
    return true;
}

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* clang_decl) {
    if (OnVisit(clang_decl)) return true;

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
                    owner->Serialize();
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

void UEMeta::ClangHandler::Serialize() {
    // only going to worry about stdout for now

    auto& cfg = Config::GetConfig();
    if (cfg.DumpToStdout()) {
        for (auto* msg : transient_data.results) {
            auto str = msg->DebugString();
            UEM_INFO(str);
        }
    }
}

bool UEMeta::ClangHandler::OnVisit(clang::TagDecl* clang_decl) {
    // if this is a forward declaration...
    if (!clang_decl->isThisDeclarationADefinition()) {
        // and we don't know about it yet...
        if (transient_data.visited_forward_decls.insert(clang_decl).second) {
            // generate a new forward declaration message and return
            AddForwardDeclaration(transient_data, clang_decl);
        }

        return true;
    }

    // if this is a complete definition that we've already seen (secondary translation unit), continue
    if (!transient_data.visited_decls.insert(clang_decl->getCanonicalDecl()).second) return true;

    return false;
}