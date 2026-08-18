#pragma once
#include "UEMeta/Cli.hpp"
#include "parser.pb.h"
#include <atomic>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Comment.h>
#include <exception>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace ParseResult;

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
    if (!decl) return "";
    std::string s{};
    llvm::raw_string_ostream os(s);
    decl->print(os, policy, 0, true);
    os.flush();
    return s;
}

inline std::string ClangToString(clang::ASTContext& context, const clang::Stmt* statement) {
    if (!statement) return "";
    std::string s{};
    llvm::raw_string_ostream os(s);
    statement->printPretty(os, nullptr, context.getPrintingPolicy());
    os.flush();
    return s;
}

/// @brief Gets the declaration as a macro-expanded string
inline std::string ClangToString(clang::ASTContext& context, const clang::Decl* decl) {
    return ClangToString(decl, context.getPrintingPolicy());
}

/// @brief Gets the template argument as a macro-expanded string
inline std::string ClangToString(clang::ASTContext& context, const clang::TemplateArgument& arg) {
    std::string s{};
    llvm::raw_string_ostream os(s);
    arg.print(context.getPrintingPolicy(), os, true);
    os.flush();
    return s;
}

/// @brief Gets the expr as a macro-expanded string
inline std::string ClangToString(clang::ASTContext& context, const clang::Expr* expr) {
    if (!expr) return "";
    std::string s{};
    llvm::raw_string_ostream os(s);
    expr->printPretty(os, nullptr, context.getPrintingPolicy());
    os.flush();
    return s;
}

/// @brief Strips pointer/refs/array tokens from type
inline clang::QualType GetUnderlyingType(clang::QualType in) {
    in = in.getNonReferenceType();
    while (in->isPointerType() || in->isArrayType()) {
        if (in->isPointerType()) {
            in = in->getPointeeType();
        } else if (in->isArrayType()) {
            in = llvm::cast<clang::ArrayType>(in.getTypePtr())->getElementType();
        }
    }
    return in.getUnqualifiedType();
}

inline llvm::StringRef GetDeclSourcePath(clang::ASTContext& context, const clang::Decl* decl) {
    static llvm::DenseMap<const clang::Decl*, llvm::StringRef> decl_srcs;
    if (!decl) throw std::runtime_error("Can't get a source location from a null decl!");
    if (const auto cached = decl_srcs.find(decl); cached != decl_srcs.end()) {
        return cached->second;
    }
    const auto& source_man = context.getSourceManager();
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
inline void PopulateEnumDetails(clang::ASTContext& context, EnumDetails* p_msg, const clang::EnumDecl* decl) {
    if (!(p_msg && decl)) {
        UEM_WARN("Failed to PopulateEnumDetails due to nullptr! p_msg={}, decl={}", !!p_msg, !!decl);
        return;
    }
    auto underlying = decl->getIntegerType();
    if (!underlying.isNull()) {
        p_msg->set_underlying_type(underlying.getAsString(context.getPrintingPolicy()));
    }
    p_msg->set_scope(!decl->isScoped() ? ENUM_SCOPE_UNSCOPED : decl->isScopedUsingClassTag() ? ENUM_SCOPE_CLASS : ENUM_SCOPE_STRUCT);
}


/// @brief Fills out an Identifier from a Decl
inline void PopulateIdentifier(clang::ASTContext& context, Identifier* p_msg, const clang::Decl* decl) {
    if (!(p_msg && decl)) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! p_msg={}, decl={}", !!p_msg, !!decl);
        return;
    }
    if (auto* as_named = llvm::dyn_cast_or_null<clang::NamedDecl>(decl)) {
        if (auto* as_parmdecl = llvm::dyn_cast_or_null<clang::ParmVarDecl>(decl)) { // params need special handling
            auto qual_name = llvm::dyn_cast_or_null<clang::NamedDecl>(as_parmdecl->getDeclContext())->getQualifiedNameAsString() + "::" + as_parmdecl->getQualifiedNameAsString();
            p_msg->set_qualified_name(qual_name);
            p_msg->set_qualified_name_hash(std::hash<std::string>::operator()(qual_name));
        }
        else {
            p_msg->set_qualified_name(as_named->getQualifiedNameAsString());
            p_msg->set_qualified_name_hash(std::hash<std::string>::operator()(p_msg->qualified_name()));
        }

        if (as_named->getDeclName().isIdentifier()) {
            p_msg->set_name(as_named->getName());
        }
        else {
            p_msg->set_name(as_named->getNameAsString());
        }

        const auto src = GetDeclSourcePath(context, decl);
        p_msg->set_file_path(src.str());
        p_msg->set_file_path_hash(GetDeclSourcePathHash(src));
        if (const auto* comment = context.getRawCommentForDeclNoCache(decl)) {
            p_msg->set_documentation(comment->getRawText(context.getSourceManager()).str());
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
    else {
        UEM_WARN("Failed to cast decl {} to clang::NamedDecl!", ClangToString(context, decl));
    }
}

/// @brief Fills out a DeclarationMetadata from a Decl
inline void PopulateDeclarationMetadata(clang::ASTContext& context, DeclarationMetadata* p_msg, const clang::Decl* decl) {
    if (!(p_msg && decl)) {
        UEM_WARN("Failed to PopulateDeclarationMetadata due to nullptr! p_msg={}, decl={}", !!p_msg, !!decl);
        return;
    }
    PopulateIdentifier(context, p_msg->mutable_identifier(), decl);
    if (auto* as_enum = llvm::dyn_cast_or_null<clang::EnumDecl>(decl)) {
        p_msg->set_is_anonymous(as_enum->getName().empty());
    }
    else if (auto* as_record = llvm::dyn_cast_or_null<clang::RecordDecl>(decl)) {
        p_msg->set_is_anonymous(as_record->isAnonymousStructOrUnion() || as_record->getName().empty()); // catches nested anon structs as well
    }
    else {
        p_msg->set_is_anonymous(false);
    }
    p_msg->set_content_hash(std::hash<std::string_view>::operator()(ClangToString(context, decl)));
}

inline void PopulateTemplateDetails(clang::ASTContext& context, TemplateDetails* p_msg, const clang::Decl* decl) {
    if (!(p_msg && decl)) {
        UEM_WARN("Failed to PopulateTemplateDetails due to nullptr! p_msg={}, decl={}", !!p_msg, !!decl);
        return;
    }
    const auto& policy = context.getPrintingPolicy();
    const auto PopulateParameters = [&](this auto self, const clang::TemplateParameterList* params, auto* p_params) -> void {
        if (!params) return;
        for (const auto* param : *params) {
            auto* p_param = p_params->add_parameters();
            PopulateIdentifier(context, p_param->mutable_identifier(), param);
            p_param->set_as_string(ClangToString(context, param));
            if (const auto* type_param = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
                p_param->set_kind(type_param->wasDeclaredWithTypename()
                                      ? TEMPLATE_PARAMETER_KIND_TYPENAME
                                      : TEMPLATE_PARAMETER_KIND_CLASS);
                if (type_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (type_param->hasDefaultArgument()) {
                    p_param->set_default_value(ClangToString(context, type_param->getDefaultArgument().getArgument()));
                }
            }
            else if (const auto* non_type_param = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param)) {
                p_param->set_kind(TEMPLATE_PARAMETER_KIND_NON_TYPE);
                p_param->set_type(non_type_param->getType().getAsString(policy));
                if (non_type_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (non_type_param->hasDefaultArgument()) {
                    p_param->set_default_value(ClangToString(context, non_type_param->getDefaultArgument().getArgument()));
                }
            }
            else if (const auto* template_param = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
                p_param->set_kind(template_param->wasDeclaredWithTypename()
                                      ? TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE
                                      : TEMPLATE_PARAMETER_KIND_CLASS_TEMPLATE);
                if (template_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (template_param->hasDefaultArgument()) {
                    p_param->set_default_value(ClangToString(context, template_param->getDefaultArgument().getArgument()));
                }
                self(template_param->getTemplateParameters(), p_param);
            }
        }
    };
    const auto AddTemplateArguments = [&](const auto& args) {
        for (const auto& arg : args) {
            p_msg->add_arguments(ClangToString(context, arg));
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
            UEM_WARN("Failed to get primary template for declaration {}", ClangToString(context, decl));
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

    if (const auto* as_alias = llvm::dyn_cast<clang::TypeAliasDecl>(decl)) {
        if (const auto* primary = as_alias->getDescribedAliasTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_NONE);
            p_msg->set_primary_template_qualified_name(primary->getQualifiedNameAsString());
        }

        return;
    }

    if (const auto* as_var = llvm::dyn_cast<clang::VarDecl>(decl)) {
        if (const auto* primary = as_var->getDescribedVarTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_NONE);
            p_msg->set_primary_template_qualified_name(primary->getQualifiedNameAsString());
            return;
        }

        if (const auto* spec = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(as_var)) {
            if (const auto* partial = llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(spec)) {
                PopulateParameters(partial->getTemplateParameters(), p_msg);
            }
            else if (const auto* primary = spec->getSpecializedTemplate()) {
                PopulateParameters(primary->getTemplateParameters(), p_msg);
            }

            if (const auto* primary = spec->getSpecializedTemplate()) {
                p_msg->set_primary_template_qualified_name(primary->getQualifiedNameAsString());
            }
            AddTemplateArguments(spec->getTemplateArgs().asArray());
            SetSpecializationKind(spec->getSpecializationKind());
        }
    }
}

inline void PopulateFunctionCommon(clang::ASTContext& context, FunctionCommon* p_msg, clang::FunctionDecl* decl) {
    PopulateTemplateDetails(context, p_msg->mutable_template_details(), decl);
    PopulateIdentifier(context, p_msg->mutable_identifier(), decl);
    p_msg->set_return_type(decl->getReturnType().getAsString());
    if (const auto as_cxx = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(decl)) {
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

        p_msg->set_definition_kind(as_cxx->isDefaulted() ? FUNCTION_DEFINITION_DEFAULTED
                : as_cxx->isDeleted() ? FUNCTION_DEFINITION_DELETED : FUNCTION_DEFINITION_NORMAL);
    }
    else {
        p_msg->set_kind(llvm::dyn_cast_or_null<clang::CXXConversionDecl>(decl) ? FUNCTION_KIND_CONVERSION : FUNCTION_KIND_FREE);
        p_msg->set_kind(FUNCTION_KIND_FREE);
    }

    p_msg->set_consteval_kind(decl->isConsteval() ? CONSTANT_EVALUATION_CONSTEVAL
        : decl->isConstexpr() ? CONSTANT_EVALUATION_CONSTEXPR : CONSTANT_EVALUATION_NONE);

    if (decl->doesThisDeclarationHaveABody()) {
        p_msg->set_inline_definition(ClangToString(context, decl->getBody()));
    }

    p_msg->set_storage_class(decl->getStorageClass() == clang::SC_Extern && decl->isExternCXXContext() ? FUN_VAR_STORAGE_CLASS_EXTERN
            : decl->getStorageClass() == clang::SC_Extern && decl->isExternC() ? FUN_VAR_STORAGE_CLASS_EXTERN_C
            : decl->isStatic() ? FUN_VAR_STORAGE_CLASS_STATIC : FUN_VAR_STORAGE_CLASS_UNSPECIFIED);

    static clang::PrintingPolicy fn_sig_pp = [&] {
        clang::PrintingPolicy pp(context.getPrintingPolicy());
        pp.TerseOutput = true;
        return pp;
    }();
    p_msg->set_as_string(ClangToString(decl, fn_sig_pp));
    p_msg->set_content_hash(std::hash<std::string>::operator()(ClangToString(context, decl)));

    for (const auto param : decl->parameters()) {
        const auto p_param = p_msg->add_parameters();
        const auto str = ClangToString(context, param);
        p_param->set_as_string(str);
        p_param->set_content_hash(std::hash<std::string>::operator()(str));
        PopulateIdentifier(context, p_param->mutable_identifier(), param);
        p_param->set_type(param->getType().getAsString());
        p_param->set_default_value(ClangToString(context, param->getInit()));
    }
}

#define EMP(s) if(proto->s().empty()) return false
#define NEZ(i) if(proto->i() <= 0) return false
#define NEQ(i, v) if(proto->i() == v) return false

inline bool Validate(const google::protobuf::Message* msg) {
    if (!msg) return false;

    const auto ValidateIdentifier = [](const Identifier* proto) {
        EMP(name);
        EMP(qualified_name);
        EMP(file_path);
        EMP(scope);
        NEZ(file_path_hash);
        NEZ(qualified_name_hash);
        return true;
    };

    const auto ValidateMetadata = [&](const DeclarationMetadata* proto) {
        if (!ValidateIdentifier(&proto->identifier())) return false;
        NEZ(content_hash);
        // no need to validate occurrence index since it can be zero
        return true;
    };

    const auto ValidateTemplateDetails = [&] (const TemplateDetails* proto) {
        const auto ValidateTemplateParam = [&] (this auto self, const TemplateParameter* proto) {
            if (!ValidateIdentifier(&proto->identifier())) return false;
            NEQ(kind, TEMPLATE_PARAMETER_KIND_UNSPECIFIED);
            EMP(as_string);
            for (auto& parameter : proto->parameters()) {
                if (!self(&parameter)) return false;
            }
            return true;
        };

        for (auto& parameter : proto->parameters()) {
            if (!ValidateTemplateParam(&parameter)) return false;
        }

        return true;
    };

    if (const auto* proto = dynamic_cast<const TLRecordDeclaration*>(msg)) {
        NEQ(kind, RECORD_KIND_UNSPECIFIED);
        if (!ValidateMetadata(&proto->metadata())) return false;
        if (proto->has_template_details() && !ValidateTemplateDetails(&proto->template_details())) return false;

    }

    return true;
}

#undef EMP
#undef NEZ
#undef NEQ
