// ReSharper disable CppMemberFunctionMayBeStatic
// ReSharper disable CppMemberFunctionMayBeConst
#include "UEMeta/ClangHandler.hpp"

#include <atomic>
#include <exception>
#include <execution>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/AST/RecordLayout.h>
#include <clang/Tooling/Tooling.h>
#include <google/protobuf/util/json_util.h>
#include "parser.pb.h"

#include "UEMeta/Cli.hpp"
#include "UEMeta/Internal/ASTData.hpp"
#include "UEMeta/Internal/ClangHelpers.hpp"

using namespace ParseResult;

/// @brief Requests traversal of template instantiations.
bool UEMeta::ClangHandler::shouldVisitTemplateInstantiations() const { return true; } // NOLINT(*-convert-member-functions-to-static)

/// @brief Skips implicit compiler-generated declarations.
bool UEMeta::ClangHandler::shouldVisitImplicitCode() const { return false; } // NOLINT(*-convert-member-functions-to-static)

/// @brief Skips lambda body traversal.
bool UEMeta::ClangHandler::shouldVisitLambdaBody() const { return false; } // NOLINT(*-convert-member-functions-to-static)

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
        data->GetTraverseLogger().Start();
        data->SetContext(&ctx);
    });
}

/// @brief Logs the end of declaration traversal, starts post-processing
void UEMeta::ClangHandler::EndTranslationUnit(clang::ASTContext&) { // NOLINT(*-convert-member-functions-to-static)
    GuardClangCallback("EndTranslationUnit", [&] {
        data->GetTraverseLogger().Stop();
        UEM_INFO("Finished traversing AST, extracted data from {} nodes!", data->GetTraverseLogger().GetValue());
        data->GenerateFileData();
        data->Serialize();
    });
}

bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl* clang_decl) {
    if (data->OnVisit(clang_decl)) return true;

    auto& context = data->GetContext();
    auto* p_record_decl = data->Allocate<TLRecordDeclaration>();
    PopulateDeclarationMetadata(context, p_record_decl->mutable_metadata(), clang_decl);
    if (const auto* cxx = llvm::dyn_cast<clang::CXXRecordDecl>(clang_decl);
        cxx && (cxx->getDescribedClassTemplate() || llvm::isa<clang::ClassTemplateSpecializationDecl>(cxx))) {
        PopulateTemplateDetails(context, p_record_decl->mutable_template_details(), clang_decl);
    }
    bool has_complete_definition = true;

    bool has_known_layout = clang_decl->isCompleteDefinition();
    for (const auto* field : clang_decl->fields()) {
        if (field->getType()->isDependentType()
            || (field->isBitField() && field->getBitWidth()->isValueDependent())) {
            has_known_layout = false;
            break;
        }
    }
    if (const auto* cxx = llvm::dyn_cast<clang::CXXRecordDecl>(clang_decl);
        cxx && cxx->hasAnyDependentBases()) {
        has_known_layout = false;
    }

    const clang::ASTRecordLayout* layout = nullptr;
    if (has_known_layout) {
        layout = &context.getASTRecordLayout(clang_decl);
        p_record_decl->set_align_bytes(layout->getAlignment().getQuantity());
        p_record_decl->set_size_bytes(layout->getSize().getQuantity());
    }
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
        PopulateIdentifier(context, p_field->mutable_identifier(), field);
        if (layout) {
            p_field->set_offset_bits(layout->getFieldOffset(field->getFieldIndex()));
        }
        if (field->isBitField()) {
            p_field->set_is_bitfield(true);
            if (!field->getBitWidth()->isValueDependent()) {
                p_field->set_bit_width(field->getBitWidthValue());
            }
        }
        else if (layout) {
            p_field->set_bit_width(context.getTypeSize(type));
        }
        p_field->set_is_mutable(field->isMutable());
        if (const auto* def_val = field->getInClassInitializer()) {
            p_field->set_default_value(ClangToString(context, def_val));
        }

        static clang::PrintingPolicy field_printing_policy = [&] {
            auto pol = clang::PrintingPolicy(context.getPrintingPolicy());
            pol.SuppressInitializers = true;
            pol.SuppressSpecifiers = false;
            return pol;
        }();

        p_field->set_as_string(ClangToString(field, field_printing_policy));
        p_field->set_content_hash(std::hash<std::string>::operator()(ClangToString(context, field)));
        p_field->set_underlying_type(GetUnderlyingType(field->getType()).getAsString());
        p_field->set_type(field->getType().getAsString());
    }

    // if it's not a c-style POD
    if (const auto* cxx = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(clang_decl)) {
        // populate methods (covers ctors, dtors, conversion ops, static methods, and normal ones)
        for (auto* method : cxx->methods()) {
            if (method->isImplicit()) continue; // we don't care about compiler-generated functions
            auto* p_method = p_record_decl->add_methods();
            PopulateFunctionCommon(context, p_method->mutable_common(), method);
            if (!p_method->common().has_inline_definition()) {
                has_complete_definition = false;
            }

            if (method->isVirtual()) {
                p_method->set_virtuality(method->isPureVirtual() ? FUNCTION_VIRTUALITY_PURE : FUNCTION_VIRTUALITY_VIRTUAL);
                auto* p_vt = p_method->mutable_vtable_index();
                // NOTE: it may be possible to support Linux vtable parsing with Itanium VTableContext when the flags
                // indicate linux compilation, but the math is a bit more complicated
                if (!context.getTargetInfo().getCXXABI().isMicrosoft())
                    throw std::runtime_error("Itanium (Linux) ABI not supported yet!");
                const auto vtable = llvm::cast<clang::MicrosoftVTableContext>(context.getVTableContext());
                const auto method_decl = [&]() -> clang::GlobalDecl {
                    if (const auto* dtor = llvm::dyn_cast<clang::CXXDestructorDecl>(method)) {
                        const auto dtor_type = context.getTargetInfo().emitVectorDeletingDtors(
                                                   context.getLangOpts())
                            ? clang::Dtor_VectorDeleting
                            : clang::Dtor_Deleting;
                        return {dtor, dtor_type};
                    }

                    return {method};
                }();
                const auto method_loc = vtable->getMethodVFTableLocation(method_decl);
                p_vt->set_offset(method_loc.VFPtrOffset.getQuantity());
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
            PopulateIdentifier(context, p_base->mutable_identifier(), base_decl->getDefinitionOrSelf());
            p_base->set_access(ClangToProtoAccess(base.getAccessSpecifier()));
            p_base->set_is_virtual(base.isVirtual());
            if (layout) {
                const auto offset = base.isVirtual()
                    ? layout->getVBaseClassOffset(base_decl)
                    : layout->getBaseClassOffset(base_decl);
                p_base->set_offset(offset.getQuantity());
            }
            p_base->set_as_string(base.getType().getAsString());
        }
    }

    // Inherited and compiler-generated methods are not serialized into this declaration, so their definitions come
    // from the base record or the compiler respectively.
    p_record_decl->set_is_complete_definition(has_complete_definition);

    data->AddVisitedDecl(clang_decl, p_record_decl);
    return data->OnAfterVisit(clang_decl, p_record_decl->metadata().identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* clang_decl) {
    if (data->OnVisit(clang_decl)) return true;

    auto& context = data->GetContext();
    auto* p_enum_decl = data->Allocate<TLEnumDeclaration>();
    PopulateDeclarationMetadata(context, p_enum_decl->mutable_metadata(), clang_decl);
    PopulateEnumDetails(context, p_enum_decl->mutable_details(), clang_decl);
    for (auto* enumerator : clang_decl->enumerators()) {
        auto p_enumerator = p_enum_decl->add_enumerators();
        // note that this retains C++ enumerator scoping rules; for an unscoped (non class/struct) enum,
        // enumerator constants exist in the enclosing scope, rather than within the scope of the enum
        // declaration
        PopulateIdentifier(context, p_enumerator->mutable_identifier(), enumerator);
        p_enumerator->set_value(llvm::toString(enumerator->getInitVal(), 10));
    }

    data->AddVisitedDecl(clang_decl, p_enum_decl);
    return data->OnAfterVisit(clang_decl, p_enum_decl->mutable_metadata()->identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitFunctionDecl(clang::FunctionDecl* clang_decl) {
    if (clang_decl->isCXXClassMember() || data->OnVisit(clang_decl)) return true; // handled by VisitRecordDecl

    auto& context = data->GetContext();
    auto* p_fun = data->Allocate<TLFreeFunctionDeclaration>();
    PopulateDeclarationMetadata(context, p_fun->mutable_metadata(), clang_decl);
    PopulateFunctionCommon(context, p_fun->mutable_common(), clang_decl);
    data->AddVisitedDecl(clang_decl, p_fun);
    return data->OnAfterVisit(clang_decl, p_fun->metadata().identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitTypedefNameDecl(clang::TypedefNameDecl* clang_decl) {
    if (data->OnVisit(clang_decl)) return true;
    auto& context = data->GetContext();
    auto* p_alias = data->Allocate<TLAliasDeclaration>();
    PopulateDeclarationMetadata(context, p_alias->mutable_metadata(), clang_decl);
    const auto* type_alias = llvm::dyn_cast<clang::TypeAliasDecl>(clang_decl);
    if (type_alias && type_alias->getDescribedAliasTemplate()) {
        PopulateTemplateDetails(context, p_alias->mutable_template_details(), clang_decl);
    }
    p_alias->set_alias(clang_decl->getNameAsString());
    p_alias->set_aliased_type(clang_decl->getUnderlyingType().getAsString());
    const auto str = ClangToString(
        context,
        type_alias && type_alias->getDescribedAliasTemplate()
            ? static_cast<const clang::Decl*>(type_alias->getDescribedAliasTemplate())
            : clang_decl);
    p_alias->set_as_string(str);
    data->AddVisitedDecl(clang_decl, p_alias);
    return data->OnAfterVisit(clang_decl, p_alias->metadata().identifier().qualified_name_hash());
}

bool UEMeta::ClangHandler::VisitVarDecl(clang::VarDecl* clang_decl) {
    if (clang_decl->isCXXClassMember() || clang::dyn_cast_or_null<clang::ParmVarDecl>(clang_decl) // handled by VisitRecordDecl/VisitFunctionDecl
        || data->OnVisit(clang_decl))
        return true;
    auto& context = data->GetContext();
    auto* p_var = data->Allocate<TLGlobalVariableDeclaration>();
    PopulateDeclarationMetadata(context, p_var->mutable_metadata(), clang_decl);
    p_var->set_underlying_type(GetUnderlyingType(clang_decl->getType()).getAsString());
    p_var->set_type(clang_decl->getType().getAsString());
    const auto str = ClangToString(context, clang_decl);
    p_var->set_as_string(str);
    p_var->set_content_hash(std::hash<std::string>::operator()(str));
    p_var->set_storage_class(clang_decl->getTLSKind() != clang::VarDecl::TLS_None ? VAR_STORAGE_CLASS_THREAD_LOCAL
        : clang_decl->getStorageClass() == clang::SC_Static ? VAR_STORAGE_CLASS_STATIC
        : clang_decl->getStorageClass() == clang::SC_Extern && clang_decl->isExternC() ? VAR_STORAGE_CLASS_EXTERN_C
        : clang_decl->getStorageClass() == clang::SC_Extern ? VAR_STORAGE_CLASS_EXTERN
        : VAR_STORAGE_CLASS_UNSPECIFIED);
    p_var->set_constant_evaluation_kind(clang_decl->isConstexpr() ? CONSTANT_EVALUATION_CONSTEXPR : CONSTANT_EVALUATION_NONE); // vars can't be consteval
    if (const auto* initializer = clang_decl->getInit()) {
        p_var->set_default_value(ClangToString(context, initializer));
    }
    data->AddVisitedDecl(clang_decl, p_var);
    return data->OnAfterVisit(clang_decl, p_var->metadata().identifier().qualified_name_hash());
}

UEMeta::ClangHandler::ClangHandler() : data(new ASTData()), logger("Visited {} total nodes...") {}

/// @brief Creates the AST consumer for one translation unit.
std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                            llvm::StringRef file) {
    try {
        compiler.getLangOpts().CommentOpts.ParseAllComments = true;
        const auto input_name = file.str();
        const auto file_name = std::filesystem::path(input_name).filename().string();
        const auto tu_name = file_name.empty() ? input_name : file_name;

        /// @brief AST consumer that drives traversal once Clang finishes parsing a translation unit.
        class Consumer : public clang::ASTConsumer {
        public:
            /// @brief Creates a consumer tied to the owning ClangHandler.
            explicit Consumer(ClangHandler* owner, std::string tu_name)
                : owner(owner), tu_name(std::move(tu_name)), parse_logger(fmtquill::format("Parsing TU {}...", this->tu_name)) {}

            void Initialize(clang::ASTContext &Context) override {
                UEM_INFO("Starting TU '{}' parsing (this may take a moment)!", tu_name);
                parse_logger.Start();
            }

            /// @brief Traverses declarations after Clang finishes parsing the translation unit.
            void HandleTranslationUnit(clang::ASTContext& ctx) override {
                GuardClangCallback("HandleTranslationUnit", [&] {
                    parse_logger.Stop();
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
            HeartbeatLogger parse_logger;
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
