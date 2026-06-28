#pragma once
#include "UEMeta/Cli.hpp"
#include "parser.pb.h"
#include <clang/AST/ASTContext.h>
#include <clang/AST/Comment.h>
#include <ranges>
#include <string_view>
#include "UEMeta/ClangHandler.hpp"

using namespace ParseResult;
using Data = UEMeta::ClangHandler::TransientData;

/// @brief Tracks whether any guarded Clang callback caught an exception.
inline std::atomic_bool GClangExceptionCaught{false};

/// @brief Records a Clang processing exception and logs the failing step.
inline void LogClangException(const std::string_view step, const std::exception& ex) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with exception: {}", step, ex.what());
    } catch (...) {
    }
}

/// @brief Records an unknown Clang processing exception and logs the failing step.
inline void LogClangUnknownException(const std::string_view step) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with unknown exception", step);
    } catch (...) {
    }
}

/// @brief Runs a Clang callback with exception logging and failure recording.
template <typename Func>
void GuardClangCallback(const std::string_view step, Func&& func) noexcept {
    try {
        std::forward<Func>(func)();
    } catch (const std::exception& ex) {
        LogClangException(step, ex);
    } catch (...) {
        LogClangUnknownException(step);
    }
}

inline std::string ClangToString(const clang::Decl* decl, const clang::PrintingPolicy& policy) {
    if (!decl) throw std::runtime_error("Can't get a string from a null decl!");
    std::string s{};
    llvm::raw_string_ostream os(s);
    decl->print(os, policy, 0, true);
    os.flush();
    return s;
}

inline std::string ClangToString(const Data& data, const clang::Stmt* statement) {
    if (!statement) throw std::runtime_error("Can't get a string from a null decl!");
    std::string s{};
    llvm::raw_string_ostream os(s);
    statement->printPretty(os, nullptr, data.context->getPrintingPolicy());
    os.flush();
    return s;
}

/// @brief Gets the declaration as a macro-expanded string
inline std::string ClangToString(const Data& data, const clang::Decl* decl) {
    return ClangToString(decl, data.context->getPrintingPolicy());
}

/// @brief Gets the template argument as a macro-expanded string
inline std::string ClangToString(const Data& data, const clang::TemplateArgument& arg) {
    std::string s{};
    llvm::raw_string_ostream os(s);
    arg.print(data.context->getPrintingPolicy(), os, true);
    os.flush();
    return s;
}

/// @brief Gets the expr as a macro-expanded string
inline std::string ClangToString(const Data& data, const clang::Expr* expr) {
    std::string s{};
    llvm::raw_string_ostream os(s);
    expr->printPretty(os, nullptr, data.context->getPrintingPolicy());
    os.flush();
    return s;
}

inline llvm::StringRef GetDeclSourcePath(const Data& data, const clang::Decl* decl) {
    static llvm::DenseMap<const clang::Decl*, llvm::StringRef> decl_srcs;
    if (!decl) throw std::runtime_error("Can't get a source location from a null decl!");
    if (const auto cached = decl_srcs.find(decl); cached != decl_srcs.end()) {
        return cached->second;
    }
    const auto& source_man = data.context->getSourceManager();
    const auto source_loc = source_man.getExpansionLoc(decl->getLocation());
    decl_srcs[decl] = source_man.getFilename(source_loc);
    return decl_srcs[decl];
}

inline uint64_t GetDeclSourcePathHash(llvm::StringRef in) {
    static llvm::DenseMap<llvm::StringRef, uint64_t> hashes;
    if (const auto hash = hashes.find(in); hash != hashes.end()) {
        return hash->getSecond();
    }
    hashes[in] = std::hash<std::string>::operator()(in.str());
    return hashes[in];
}

/// @brief Fills out an EnumDetails* from an EnumDecl*
inline void PopulateEnumDetails(const Data& data, EnumDetails* p_msg, const clang::EnumDecl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateEnumDetails due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    auto underlying = decl->getIntegerType();
    if (!underlying.isNull()) {
        p_msg->set_underlying_type(underlying.getAsString(data.context->getPrintingPolicy()));
    }
    p_msg->set_scope(!decl->isScoped() ? ENUM_SCOPE_UNSCOPED : decl->isScopedUsingClassTag() ? ENUM_SCOPE_CLASS : ENUM_SCOPE_STRUCT);
}


/// @brief Fills out an Identifier from a Decl
inline void PopulateIdentifier(const Data& data, Identifier* p_msg, const clang::Decl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    if (auto* as_named = llvm::dyn_cast_or_null<clang::NamedDecl>(decl)) {
        p_msg->set_qualified_name(as_named->getQualifiedNameAsString());
        p_msg->set_qualified_name_hash(std::hash<std::string>::operator()(p_msg->qualified_name()));
        if (as_named->getDeclName().isIdentifier()) {
            p_msg->set_name(as_named->getName());
        }
        else {
            p_msg->set_name(as_named->getNameAsString());
        }
        p_msg->set_file_path_hash(GetDeclSourcePathHash(GetDeclSourcePath(data, decl)));
        if (const auto* comment = data.context->getRawCommentForDeclNoCache(decl)) {
            p_msg->set_documentation(comment->getRawText(data.context->getSourceManager()).str());
        }
        auto scopes = std::views::split(p_msg->qualified_name(), std::string_view{"::"})
                        | std::views::transform([](auto range) -> std::string_view {
                                auto sv = std::string_view{range};
                                auto start = sv.find_first_not_of(" \t\n\r");
                                if (start == std::string_view::npos) return ""; // All whitespace

                                // Find last non-whitespace character
                                auto end = sv.find_last_not_of(" \t\n\r");
                                return sv.substr(start, end - start + 1);
                            })
                        | std::views::filter([] (auto sv) { return !sv.empty(); });
        for (auto scope : scopes) {
            p_msg->add_scope(std::string{scope});
        }
    }
}

/// @brief Fills out a DeclarationMetadata from a Decl
inline void PopulateDeclarationMetadata(const Data& data, DeclarationMetadata* p_msg, const clang::Decl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateDeclarationMetadata due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    PopulateIdentifier(data, p_msg->mutable_identifier(), decl);
    p_msg->set_occurrence_index(data.occurrence_index++);
    if (auto* as_enum = llvm::dyn_cast_or_null<clang::EnumDecl>(decl)) {
        p_msg->set_is_anonymous(as_enum->getName().empty());
    }
    else if (auto* as_record = llvm::dyn_cast_or_null<clang::RecordDecl>(decl)) {
        p_msg->set_is_anonymous(as_record->isAnonymousStructOrUnion() || as_record->getName().empty()); // catches nested anon structs as well
    }
    p_msg->set_content_hash(std::hash<std::string_view>::operator()(ClangToString(data, decl)));
}

inline void PopulateTemplateDetails(const Data& data, TemplateDetails* p_msg, clang::Decl* decl) {
    if (!(data.context && p_msg && decl)) {
        UEM_WARN("Failed to PopulateTemplateDetails due to nullptr! ctx={}, p_msg={}, decl={}", !!data.context, !!p_msg, !!decl);
        return;
    }
    const auto& policy = data.context->getPrintingPolicy();
    const auto PopulateParameters = [&](this auto self, const clang::TemplateParameterList* params, auto* p_params) -> void {
        if (!params) return;
        for (const auto* param : *params) {
            auto* p_param = p_params->add_parameters();
            PopulateIdentifier(data, p_param->mutable_identifier(), param);
            p_param->set_as_string(ClangToString(data, param));
            if (const auto* type_param = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
                p_param->set_kind(type_param->wasDeclaredWithTypename()
                                      ? TEMPLATE_PARAMETER_KIND_TYPENAME
                                      : TEMPLATE_PARAMETER_KIND_CLASS);
                if (type_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (type_param->hasDefaultArgument()) {
                    p_param->set_default_value(ClangToString(data, type_param->getDefaultArgument().getArgument()));
                }
            }
            else if (const auto* non_type_param = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param)) {
                p_param->set_kind(TEMPLATE_PARAMETER_KIND_NON_TYPE);
                p_param->set_type(non_type_param->getType().getAsString(policy));
                if (non_type_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (non_type_param->hasDefaultArgument()) {
                    p_param->set_default_value(ClangToString(data, non_type_param->getDefaultArgument().getArgument()));
                }
            }
            else if (const auto* template_param = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
                p_param->set_kind(template_param->wasDeclaredWithTypename()
                                      ? TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE
                                      : TEMPLATE_PARAMETER_KIND_CLASS_TEMPLATE);
                if (template_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (template_param->hasDefaultArgument()) {
                    p_param->set_default_value(ClangToString(data, template_param->getDefaultArgument().getArgument()));
                }
                self(template_param->getTemplateParameters(), p_param);
            }
        }
    };
    const auto AddTemplateArguments = [&](const auto& args) {
        for (const auto& arg : args) {
            p_msg->add_arguments(ClangToString(data, arg));
        }
    };
    const auto SetSpecializationKind = [&](const clang::TemplateSpecializationKind kind) {
        switch (kind) {
            case clang::TSK_Undeclared:
                p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_NONE);
                break;
            case clang::TSK_ImplicitInstantiation:
                p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_IMPLICIT);
                break;
            case clang::TSK_ExplicitSpecialization:
                p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_EXPLICIT);
                break;
            case clang::TSK_ExplicitInstantiationDeclaration:
                p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION);
                break;
            case clang::TSK_ExplicitInstantiationDefinition:
                p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION);
                break;
        }
    };

    if (const auto* as_cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
        const auto SetPrimaryTemplateQName = [&] {
            if (const auto* spec = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(as_cxx)) {
                if (const auto* primary = spec->getSpecializedTemplate()) {
                    p_msg->set_primary_template_qualified_name(primary->getQualifiedNameAsString());
                    return;
                }
            }
            UEM_WARN("Failed to get primary template for declaration {}", ClangToString(data, decl));
        };
        const auto GetTemplateParameters = [&]() -> const clang::TemplateParameterList* {
            if (const auto* primary = as_cxx->getDescribedClassTemplate()) return primary->getTemplateParameters();
            if (const auto* partial = llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(as_cxx)) {
                return partial->getTemplateParameters();
            }
            if (const auto* spec = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(as_cxx)) {
                if (const auto* primary = spec->getSpecializedTemplate()) return primary->getTemplateParameters();
            }
            return nullptr;
        };

        PopulateParameters(GetTemplateParameters(), p_msg);
        if (const auto* spec = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(as_cxx)) {
            AddTemplateArguments(spec->getTemplateArgs().asArray());
        }

        SetSpecializationKind(as_cxx->getTemplateSpecializationKind());
        if (as_cxx->getTemplateSpecializationKind() == clang::TSK_Undeclared) {
            p_msg->set_primary_template_qualified_name(as_cxx->getQualifiedNameAsString());
        }
        else {
            SetPrimaryTemplateQName();
        }
        return;
    }

    if (const auto* as_func = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        if (const auto* primary = as_func->getDescribedFunctionTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_NONE);
            p_msg->set_primary_template_qualified_name(primary->getQualifiedNameAsString());
            return;
        }

        if (const auto* primary = as_func->getPrimaryTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            if (const auto* args = as_func->getTemplateSpecializationArgs()) {
                AddTemplateArguments(args->asArray());
            }
            SetSpecializationKind(as_func->getTemplateSpecializationKind());
            p_msg->set_primary_template_qualified_name(primary->getQualifiedNameAsString());
        }

        return;
    }

    UEM_WARN("Failed to PopulateTemplateDetails because decl was not FunctionDecl or CXXRecordDecl!");
}

inline void PopulateFunctionCommon(const Data& data, FunctionCommon* p_msg, clang::FunctionDecl* decl) {
    PopulateTemplateDetails(data, p_msg->mutable_template_details(), decl);
    PopulateIdentifier(data, p_msg->mutable_identifier(), decl);
    p_msg->set_return_type(decl->getReturnType().getAsString());
    if (!decl->isCXXClassMember()) {
        p_msg->set_kind(llvm::dyn_cast_or_null<clang::CXXConversionDecl>(decl) ? FUNCTION_KIND_CONVERSION : FUNCTION_KIND_FREE);
    }
    else if (const auto as_cxx = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(decl)) {
        if (as_cxx->isStatic()) {
            p_msg->set_kind(FUNCTION_KIND_STATIC_MEMBER);
        }
        else if (const auto as_ctor = llvm::dyn_cast_or_null<clang::CXXConstructorDecl>(decl)) {
            p_msg->set_kind(FUNCTION_KIND_CONSTRUCTOR);
            p_msg->set_is_explicit(as_ctor->isExplicit());
        }
        else if (auto as_dtor = llvm::dyn_cast_or_null<clang::CXXDestructorDecl>(decl)) {
            p_msg->set_kind(FUNCTION_KIND_DESTRUCTOR);
        }
        else if (auto as_conv = llvm::dyn_cast_or_null<clang::CXXConversionDecl>(decl)) {
            p_msg->set_kind(FUNCTION_KIND_MEMBER_CONVERSION);
            p_msg->set_is_explicit(as_conv->isExplicit());
        }
        else {
            p_msg->set_kind(FUNCTION_KIND_MEMBER);
        }

        p_msg->set_storage_class(as_cxx->isExternCXXContext() ? FUN_VAR_STORAGE_CLASS_EXTERN
            : as_cxx->isExternC() ? FUN_VAR_STORAGE_CLASS_EXTERN_C
            : as_cxx->isStatic() ? FUN_VAR_STORAGE_CLASS_STATIC : FUN_VAR_STORAGE_CLASS_UNSPECIFIED);

        p_msg->set_definition_kind(as_cxx->isDefaulted() ? FUNCTION_DEFINITION_DEFAULTED
                : as_cxx->isDeleted() ? FUNCTION_DEFINITION_DELETED : FUNCTION_DEFINITION_NORMAL);
    }
    else {
        p_msg->set_kind(FUNCTION_KIND_FREE);
        p_msg->set_storage_class(decl->isExternC() ? FUN_VAR_STORAGE_CLASS_EXTERN_C
            : decl->isStatic() ? FUN_VAR_STORAGE_CLASS_STATIC : FUN_VAR_STORAGE_CLASS_UNSPECIFIED);
    }
    p_msg->set_consteval_kind(decl->isConsteval() ? CONSTANT_EVALUATION_CONSTEVAL
        : decl->isConstexpr() ? CONSTANT_EVALUATION_CONSTEXPR : CONSTANT_EVALUATION_NONE);

    if (decl->doesThisDeclarationHaveABody()) {
        p_msg->set_inline_definition(ClangToString(data, decl->getBody()));
    }

    static clang::PrintingPolicy fn_sig_pp = [&] {
        clang::PrintingPolicy pp(data.context->getPrintingPolicy());
        pp.TerseOutput = true;
        return pp;
    }();
    p_msg->set_as_string(ClangToString(decl, fn_sig_pp));
    p_msg->set_content_hash(std::hash<std::string>::operator()(ClangToString(data, decl)));
}

/// @pre decl is a forward declaration
/// @brief Adds a new forward declaration message to results from decl
inline auto AddForwardDeclaration(const Data& data, clang::TagDecl* decl) {
    if (decl->isThisDeclarationADefinition())
        throw std::runtime_error("AddForwardDeclaration can't be called on a non forward declaration!");

    auto* p_decl = google::protobuf::Arena::Create<Declaration>(&data.arena);
    auto* p_forward_decl = p_decl->mutable_forward_declaration();

    // assign the kind and template info
    switch (decl->getTagKind()) {
        case clang::TagTypeKind::Struct:
            p_forward_decl->set_kind(FORWARD_DECLARATION_KIND_STRUCT);
            PopulateTemplateDetails(data, p_forward_decl->mutable_template_details(), decl);
            break;
        case clang::TagTypeKind::Union:
            p_forward_decl->set_kind(FORWARD_DECLARATION_KIND_UNION);
            PopulateTemplateDetails(data, p_forward_decl->mutable_template_details(), decl);
            break;
        case clang::TagTypeKind::Class:
            p_forward_decl->set_kind(FORWARD_DECLARATION_KIND_CLASS);
            PopulateTemplateDetails(data, p_forward_decl->mutable_template_details(), decl);
            break;
        case clang::TagTypeKind::Enum:
            p_forward_decl->set_kind(FORWARD_DECLARATION_KIND_ENUM);
            PopulateEnumDetails(data, p_forward_decl->mutable_enum_details(), static_cast<clang::EnumDecl*>(decl));
            break;
        default:
            throw std::runtime_error("AddForwardDeclaration can't be called on a nonstandard declaration!");
    }

    // populate declaration metadata
    PopulateDeclarationMetadata(data, p_forward_decl->mutable_metadata(), decl);

    // populate as string
    p_forward_decl->set_as_string(ClangToString(data, decl));

    return p_decl;
}
