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
#include <clang/AST/VTableBuilder.h>
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

bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl* clang_decl) {
    if (OnVisit(clang_decl)) return true;

    auto* p_decl = Arena::Create<Declaration>(&transient_data.arena);
    auto* p_record_decl = p_decl->mutable_record();
    PopulateDeclarationMetadata(transient_data, p_record_decl->mutable_metadata(), clang_decl);
    PopulateTemplateDetails(transient_data, p_record_decl->mutable_template_details(), clang_decl);
    p_record_decl->set_is_complete_definition(clang_decl->isCompleteDefinition());
    auto& layout = transient_data.context->getASTRecordLayout(clang_decl);
    p_record_decl->set_align_bytes(layout.getAlignment().getQuantity());
    p_record_decl->set_size_bytes(layout.getSize().getQuantity());
    p_record_decl->set_kind(clang_decl->isClass() ? RECORD_KIND_CLASS : clang_decl->isStruct() ? RECORD_KIND_STRUCT : RECORD_KIND_UNION);

    const auto ClangToProtoAccess = [&](const clang::AccessSpecifier access) {
        return access == clang::AS_public ? ACCESS_SPECIFIER_PUBLIC
            : access == clang::AS_protected ? ACCESS_SPECIFIER_PROTECTED : access == clang::AS_private ? ACCESS_SPECIFIER_PRIVATE
            : clang_decl->isClass() ? ACCESS_SPECIFIER_PRIVATE : ACCESS_SPECIFIER_PUBLIC;
    };

    // populate instance fields
    for (const auto* field : clang_decl->fields()) {
        auto* p_field = p_record_decl->add_fields();
        const auto type = field->getType();
        p_field->set_access(ClangToProtoAccess(field->getAccess()));
        PopulateIdentifier(transient_data, p_field->mutable_identifier(), field);
        p_field->set_offset_bits(layout.getFieldOffset(field->getFieldIndex()));
        if (field->isBitField()) {
            p_field->set_is_bitfield(true);
            p_field->set_bit_width(field->getBitWidthValue());
        }
        else {
            p_field->set_bit_width(transient_data.context->getTypeSize(type));
        }
        p_field->set_is_mutable(field->isMutable());
        if (const auto* def_val = field->getInClassInitializer()) {
            p_field->set_default_value(ClangToString(transient_data, def_val));
        }

        static clang::PrintingPolicy field_printing_policy = [&] {
            auto pol = clang::PrintingPolicy(transient_data.context->getPrintingPolicy());
            pol.SuppressInitializers = true;
            pol.SuppressSpecifiers = false;
            return pol;
        }();

        p_field->set_as_string(ClangToString(field, field_printing_policy));
        p_field->set_content_hash(std::hash<std::string>::operator()(ClangToString(transient_data, field)));
    }

    // if it's not a c-style POD
    if (const auto* cxx = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(clang_decl)) {
        // populate methods (covers ctors, dtors, conversion ops, static methods, and normal ones)
        for (auto* method : cxx->methods()) {
            if (method->isImplicit()) continue; // we don't care about compiler-generated functions
            auto* p_method = p_record_decl->add_methods();
            PopulateFunctionCommon(transient_data, p_method->mutable_common(), method);

            if (method->isVirtual()) {
                p_method->set_virtuality(method->isPureVirtual() ? FUNCTION_VIRTUALITY_PURE : FUNCTION_VIRTUALITY_VIRTUAL);
                auto* p_vt = p_method->mutable_vtable_index();
                // NOTE: it may be possible to support Linux vtable parsing with Itanium VTableContext when the flags
                // indicate linux compilation, but the math is a bit more complicated
                if (!transient_data.context->getTargetInfo().getCXXABI().isMicrosoft())
                    throw std::runtime_error("Itanium (Linux) ABI not supported yet!");
                const auto vtable = llvm::cast<clang::MicrosoftVTableContext>(transient_data.context->getVTableContext());
                const auto method_decl = [&]() -> clang::GlobalDecl {
                    if (const auto* dtor = llvm::dyn_cast<clang::CXXDestructorDecl>(method)) {
                        const auto dtor_type = transient_data.context->getTargetInfo().emitVectorDeletingDtors(
                                                   transient_data.context->getLangOpts())
                            ? clang::Dtor_VectorDeleting
                            : clang::Dtor_Deleting;
                        return {dtor, dtor_type};
                    }

                    return {method};
                }();
                const auto method_loc = vtable->getMethodVFTableLocation(method_decl);
                p_vt->set_offset(method_loc.VFPtrOffset.getQuantity() / 8);
                p_vt->set_index(method_loc.Index);
            }

            p_method->set_access(ClangToProtoAccess(method->getAccess()));
            p_method->set_is_volatile(method->isVolatile());
            p_method->set_is_const(method->isConst());
            p_method->set_is_deleted(method->isDeleted());
        }

        // populate base classes
        for (const auto& base : cxx->bases()) {
            auto* p_base = p_record_decl->add_bases();
            const auto* base_decl = base.getType()->getAsCXXRecordDecl();
            if (!base_decl) {
                UEM_WARN("Failed to get definition for base type!");
                continue;
            }
            PopulateIdentifier(transient_data, p_base->mutable_identifier(), base_decl->getDefinitionOrSelf());
            p_base->set_access(ClangToProtoAccess(base.getAccessSpecifier()));
            p_base->set_is_virtual(base.isVirtual());
            p_base->set_offset(layout.getBaseClassOffset(base_decl).getQuantity());
            p_base->set_as_string(base.getType().getAsString());
        }
    }

    transient_data.visited_decls.insert(std::pair<const clang::Decl*, Declaration*>(clang_decl, p_decl));
    return OnAfterVisit(clang_decl, p_record_decl->metadata().identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* clang_decl) {
    if (OnVisit(clang_decl)) return true;

    auto* p_decl = Arena::Create<Declaration>(&transient_data.arena);
    auto* p_enum_decl = p_decl->mutable_enum_declaration();
    PopulateDeclarationMetadata(transient_data, p_enum_decl->mutable_metadata(), clang_decl);
    PopulateEnumDetails(transient_data, p_enum_decl->mutable_details(), clang_decl);
    for (auto* enumerator : clang_decl->enumerators()) {
        auto p_enumerator = p_enum_decl->add_enumerators();
        PopulateIdentifier(transient_data, p_enumerator->mutable_identifier(), enumerator);
        p_enumerator->set_value(llvm::toString(enumerator->getInitVal(), 10));
    }

    transient_data.visited_decls.insert(std::pair<const clang::Decl*, Declaration*>(clang_decl, p_decl));
    return OnAfterVisit(clang_decl, p_enum_decl->mutable_metadata()->identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitFunctionDecl(clang::FunctionDecl* clang_decl) {
    if (clang_decl->isCXXClassMember()) return true; // handled by VisitRecordDecl

}

bool UEMeta::ClangHandler::VisitTypeAliasDecl(clang::TypeAliasDecl*) { return true; }

bool UEMeta::ClangHandler::VisitVarDecl(clang::VarDecl* clang_decl) {
    if (clang_decl->isCXXClassMember()) return true; // handled by VisitRecordDecl
    return true;
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

void UEMeta::ClangHandler::Serialize() const {
    // only going to worry about stdout for now

    auto& cfg = Config::GetConfig();
    if (cfg.DumpToStdout()) {
        for (const auto& [decl, msg] : transient_data.visited_forward_decls) {
            auto str = msg->DebugString();
            UEM_INFO(str);
        }
        for (const auto& [decl, msg] : transient_data.visited_decls) {
            auto str = msg->DebugString();
            UEM_INFO(str);
        }
    }
}

bool UEMeta::ClangHandler::OnVisit(clang::TagDecl* decl) const {
    // if this is a forward declaration...
    if (!decl->isThisDeclarationADefinition()) {
        // and we don't know about it yet...
        if (!transient_data.visited_forward_decls.contains(decl)) {
            // generate a new forward declaration message and return
            auto p_decl = AddForwardDeclaration(transient_data, decl);
            transient_data.visited_forward_decls.insert(std::pair<const clang::Decl*, Declaration*>(decl, p_decl));
            return OnAfterVisit(decl, p_decl->mutable_forward_declaration()->metadata().identifier().qualified_name_hash());
        }

        return true;
    }

    // if this is a complete definition that we've already seen (secondary translation unit), continue
    if (transient_data.visited_decls.contains(decl)) return true;

    return false;
}

bool UEMeta::ClangHandler::OnAfterVisit(const clang::TagDecl* clang_decl, const uint64_t clang_decl_fqn_hash) const {
    // populate parent nested hashes
    if (const auto decl_context = clang_decl->getDeclContext(); decl_context->isRecord()) {
        auto* as_cls = llvm::cast<clang::RecordDecl>(decl_context);
        if (const auto parent = transient_data.visited_decls.find(as_cls); parent != transient_data.visited_decls.end()) {
            parent->getSecond()->mutable_record()->add_nested_hashes(clang_decl_fqn_hash);
        }
    }
    return true;
}
