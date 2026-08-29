#pragma once
#include "UEMeta/Cli.hpp"
#include "UEMeta/Internal/ProtoHelpers.hpp"
#include <atomic>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Comment.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/NestedNameSpecifier.h>
#include <clang/AST/QualTypeNames.h>
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/xxhash.h>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace ParserTypes;

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

inline const clang::NamedDecl* GetTypeIdentityDecl(const clang::QualType type) {
    if (type.isNull()) return nullptr;

    if (const auto* typedef_type = type->getAs<clang::TypedefType>()) {
        return typedef_type->getDecl();
    }
    if (const auto* tag_type = type->getAs<clang::TagType>()) {
        const auto* tag = tag_type->getDecl();
        return tag->getDefinition() ? tag->getDefinition() : tag;
    }

    const auto* type_ptr = type.getTypePtr();
    if (const auto* pack = llvm::dyn_cast<clang::PackExpansionType>(type_ptr)) {
        return GetTypeIdentityDecl(pack->getPattern());
    }
    if (const auto* specialization = llvm::dyn_cast<clang::TemplateSpecializationType>(type_ptr)) {
        return specialization->getTemplateName().getAsTemplateDecl(/*IgnoreDeduced=*/true);
    }
    if (const auto* parameter = llvm::dyn_cast<clang::TemplateTypeParmType>(type_ptr)) {
        return parameter->getDecl();
    }
    if (const auto* substituted = llvm::dyn_cast<clang::SubstTemplateTypeParmType>(type_ptr)) {
        if (const auto* parameter = substituted->getReplacedParameter()) return parameter;
        return GetTypeIdentityDecl(substituted->getReplacementType());
    }
    if (const auto* unresolved = llvm::dyn_cast<clang::UnresolvedUsingType>(type_ptr)) {
        return unresolved->getDecl();
    }
    return nullptr;
}

inline const clang::NamedDecl* GetTypeDependencyDecl(clang::QualType type);

inline const clang::NamedDecl* GetQualifierDependencyDecl(const clang::NestedNameSpecifier qualifier) {
    switch (qualifier.getKind()) {
        case clang::NestedNameSpecifier::Kind::Type:
            return GetTypeDependencyDecl(clang::QualType{qualifier.getAsType(), 0});
        case clang::NestedNameSpecifier::Kind::Namespace:
            return qualifier.getAsNamespaceAndPrefix().Namespace;
        case clang::NestedNameSpecifier::Kind::MicrosoftSuper:
            return qualifier.getAsMicrosoftSuper();
        case clang::NestedNameSpecifier::Kind::Null:
        case clang::NestedNameSpecifier::Kind::Global:
            return nullptr;
    }
    return nullptr;
}

inline const clang::NamedDecl* GetTypeDependencyDecl(const clang::QualType type) {
    if (type.isNull()) return nullptr;
    if (const auto* identity = GetTypeIdentityDecl(type)) return identity;

    const auto* type_ptr = type.getTypePtr();
    if (const auto* pack = llvm::dyn_cast<clang::PackExpansionType>(type_ptr)) {
        return GetTypeDependencyDecl(pack->getPattern());
    }
    if (const auto* dependent = llvm::dyn_cast<clang::DependentNameType>(type_ptr)) {
        return GetQualifierDependencyDecl(dependent->getQualifier());
    }
    if (const auto* specialization = llvm::dyn_cast<clang::TemplateSpecializationType>(type_ptr)) {
        if (const auto* dependent = specialization->getTemplateName().getAsDependentTemplateName()) {
            return GetQualifierDependencyDecl(dependent->getQualifier());
        }
    }
    return nullptr;
}

inline std::string GetSyntheticTypeName(
    const clang::QualType type, const clang::PrintingPolicy& policy) {
    if (const auto* pack = llvm::dyn_cast<clang::PackExpansionType>(type.getTypePtr())) {
        return GetSyntheticTypeName(pack->getPattern(), policy);
    }

    if (const auto* dependent = llvm::dyn_cast<clang::DependentNameType>(type.getTypePtr())) {
        return dependent->getIdentifier()->getName().str();
    }

    if (const auto* specialization = llvm::dyn_cast<clang::TemplateSpecializationType>(type.getTypePtr())) {
        if (const auto* dependent = specialization->getTemplateName().getAsDependentTemplateName()) {
            if (const auto* identifier = dependent->getName().getIdentifier()) {
                return identifier->getName().str();
            }
        }
    }

    auto spelling = type.getAsString(policy);
    const auto* identifier = type.getBaseTypeIdentifier();
    return identifier ? identifier->getName().str() : std::move(spelling);
}

/// @brief Strips pointer, reference, array, and cv-qualifier tokens from a type.
inline clang::QualType GetUnderlyingType(clang::QualType in) {
    in = in.getNonReferenceType();
    while (in->isPointerType() || in->isArrayType()) {
        if (in->isPointerType()) {
            in = in->getPointeeType();
        } else if (in->isArrayType()) {
            in = in->getAsArrayTypeUnsafe()->getElementType();
        }
    }
    return in.getUnqualifiedType();
}

/// Returns a source-level name for entities that Clang leaves unnamed.
inline std::string GetPlainIdentifierName(const clang::NamedDecl* declaration) {
    if (!declaration) return {};
    if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(declaration)) {
        return "operator " + conversion->getConversionType().getAsString(
            declaration->getASTContext().getPrintingPolicy());
    }
    if (!declaration->getNameAsString().empty()) return declaration->getNameAsString();

    if (const auto* parameter = llvm::dyn_cast<clang::ParmVarDecl>(declaration)) {
        return "<parameter" + std::to_string(parameter->getFunctionScopeIndex()) + ">";
    }

    unsigned template_parameter_index = 0;
    if (const auto* type_parameter = llvm::dyn_cast<clang::TemplateTypeParmDecl>(declaration)) {
        template_parameter_index = type_parameter->getIndex();
    }
    else if (const auto* non_type_parameter = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(declaration)) {
        template_parameter_index = non_type_parameter->getIndex();
    }
    else if (const auto* template_parameter = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(declaration)) {
        template_parameter_index = template_parameter->getIndex();
    }
    else {
        std::string result;
        llvm::raw_string_ostream output(result);
        declaration->getNameForDiagnostic(
            output, declaration->getASTContext().getPrintingPolicy(), false);
        output.flush();
        return result.empty() ? "(anonymous)" : result;
    }
    return "<template-parameter" + std::to_string(template_parameter_index) + ">";
}

/// Hashes Clang's USR for a declaration. Zero means Clang did not provide one.
inline std::uint64_t GetDeclarationIdentityHash(const clang::NamedDecl* declaration) {
    if (!declaration) return 0;
    if (const auto* template_declaration = llvm::dyn_cast<clang::TemplateDecl>(declaration);
        template_declaration && !llvm::isa<clang::TemplateTemplateParmDecl>(template_declaration)) {
        if (const auto* templated_declaration = template_declaration->getTemplatedDecl()) {
            declaration = templated_declaration;
        }
    }

    llvm::SmallString<256> identity;
    if (clang::index::generateUSRForDecl(declaration, identity) || identity.empty()) {
        return 0;
    }

    if (identity.empty()) return 0;
    const auto hash = llvm::xxHash64(identity);
    return hash == 0 ? 1 : hash;
}

/// Hashes Clang's USR for a type. Type identities are best-effort.
inline std::uint64_t GetTypeIdentityHash(
    clang::ASTContext& context, const clang::QualType type) {
    llvm::SmallString<256> identity;
    if (type.isNull() || clang::index::generateUSRForType(type, context, identity)
        || identity.empty()) return 0;
    const auto hash = llvm::xxHash64(identity);
    return hash == 0 ? 1 : hash;
}

/// Produces the display name stored on a TL* message's DeclarationMetadata.
inline std::string GetFullyQualifiedName(
    clang::ASTContext& context, const clang::NamedDecl* declaration) {
    if (!declaration) return {};
    if (const auto* template_declaration = llvm::dyn_cast<clang::TemplateDecl>(declaration);
        template_declaration && !llvm::isa<clang::TemplateTemplateParmDecl>(template_declaration)) {
        if (const auto* templated_declaration = template_declaration->getTemplatedDecl()) {
            declaration = templated_declaration;
        }
    }

    auto policy = context.getPrintingPolicy();
    policy.SuppressTagKeyword = true;
    policy.SuppressInitializers = true;
    if (const auto* type_declaration = llvm::dyn_cast<clang::TypeDecl>(declaration)) {
        return clang::TypeName::getFullyQualifiedName(
            context.getTypeDeclType(type_declaration), context, policy, true);
    }

    std::string result;
    llvm::raw_string_ostream output(result);
    clang::TypeName::getFullyQualifiedDeclaredContext(context, declaration, true).print(output, policy);
    declaration->getNameForDiagnostic(output, policy, false);
    output.flush();
    return result;
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

inline bool IsDeclFromBuiltinFile(clang::ASTContext& context, const clang::Decl* decl) {
    auto path = GetDeclSourcePath(context, decl);
    return std::ranges::any_of(UEMeta::Config::GetConfig().BuiltinSubpaths(),
        [&](const auto& builtin) { return path.contains(std::string_view{builtin}); });
}

// todo may not be worth it to cache this
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
    UEMeta::Proto::MutableVersionItem(p_msg->mutable_underlying_type());
    auto underlying = decl->getIntegerType();
    if (!underlying.isNull()) {
        UEMeta::Proto::SetVersioned(
            p_msg->mutable_underlying_type(), underlying.getAsString(context.getPrintingPolicy()));
    }
    p_msg->set_scope(!decl->isScoped() ? ENUM_SCOPE_UNSCOPED : decl->isScopedUsingClassTag() ? ENUM_SCOPE_CLASS : ENUM_SCOPE_STRUCT);
}

inline void PopulateIdentifier(
    clang::ASTContext& context, Identifier* p_msg, std::string_view name,
    clang::QualType identity_type,
    clang::SourceLocation location);
inline void PopulateIdentifier(
    clang::ASTContext& context, Identifier* p_msg, const clang::Decl* decl);

inline void PopulateTypeInfo(clang::ASTContext& context, TypeInfo* p_msg, const clang::QualType& type) {
    const clang::QualType underlying_type = GetUnderlyingType(type);
    UEMeta::Proto::SetVersioned(p_msg->mutable_type(), type.getAsString());
    UEMeta::Proto::SetVersioned(p_msg->mutable_underlying_type(), underlying_type.getAsString());
    UEMeta::Proto::MutableVersionItem(p_msg->mutable_source_path_hash());
    const bool is_dependent_type = type->isDependentType();
    p_msg->set_is_templated_type(is_dependent_type);

    if (const auto* declaration = GetTypeIdentityDecl(underlying_type)) {
        PopulateIdentifier(context, p_msg->mutable_identifier(), declaration);
    }
    else {
        auto name = GetSyntheticTypeName(underlying_type, context.getPrintingPolicy());
        const auto* dependency = GetTypeDependencyDecl(underlying_type);
        PopulateIdentifier(
            context, p_msg->mutable_identifier(), name, underlying_type,
            dependency ? dependency->getLocation() : clang::SourceLocation{});
    }

    const auto SetSourcePathHash = [&](const clang::Decl* decl) {
        if (decl) {
            UEMeta::Proto::SetVersioned(
                p_msg->mutable_source_path_hash(), GetDeclSourcePathHash(GetDeclSourcePath(context, decl)));
        }
    };

    if (is_dependent_type || underlying_type->isBuiltinType()) return;

    if (const auto* as_typedef = underlying_type->getAs<clang::TypedefType>()) {
        SetSourcePathHash(as_typedef->getDecl());
    }
    else if (const auto* as_tag = underlying_type->getAs<clang::TagType>()) {
        const auto* declaration = as_tag->getDecl();
        SetSourcePathHash(declaration->getDefinition() ? declaration->getDefinition() : declaration);
    }
}

/// @brief Fills out an Identifier for a syntactically named entity that has no concrete Decl yet.
inline void PopulateIdentifier(clang::ASTContext& context, Identifier* p_msg, const std::string_view name,
                               const clang::QualType identity_type,
                               const clang::SourceLocation location) {
    if (!p_msg) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! p_msg=false");
        return;
    }
    UEMeta::Proto::MutableVersionItem(p_msg->mutable_documentation());

    p_msg->set_name(std::string{name});
    p_msg->set_qualified_name_hash(GetTypeIdentityHash(context, identity_type));

    const auto& source_man = context.getSourceManager();
    const auto source_loc = source_man.getExpansionLoc(location);
    const auto source_path = source_man.getFilename(source_loc);
    UEMeta::Proto::SetVersioned(p_msg->mutable_file_path(), source_path.str());
    UEMeta::Proto::SetVersioned(
        p_msg->mutable_file_path_hash(),
        source_path.empty() ? 0 : GetDeclSourcePathHash(source_path));
}

/// @brief Fills out an Identifier from a Decl
inline void PopulateIdentifier(clang::ASTContext& context, Identifier* p_msg, const clang::Decl* decl) {
    if (!(p_msg && decl)) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! p_msg={}, decl={}", !!p_msg, !!decl);
        return;
    }
    UEMeta::Proto::MutableVersionItem(p_msg->mutable_documentation());
    if (auto* as_named = llvm::dyn_cast_or_null<clang::NamedDecl>(decl)) {
        if ((as_named->getNameAsString().empty()
             && llvm::isa<clang::ParmVarDecl, clang::TemplateTypeParmDecl,
                          clang::NonTypeTemplateParmDecl, clang::TemplateTemplateParmDecl>(as_named))
            || llvm::isa<clang::CXXConversionDecl>(as_named)) {
            p_msg->set_name(GetPlainIdentifierName(as_named));
        }
        else {
            // Anonymous tags retain their source-level empty name; the
            // identity hash still carries Clang's anonymous-tag USR.
            p_msg->set_name(as_named->getNameAsString());
        }
        p_msg->set_qualified_name_hash(GetDeclarationIdentityHash(as_named));

        const auto src = GetDeclSourcePath(context, decl);
        UEMeta::Proto::SetVersioned(p_msg->mutable_file_path(), src.str());
        UEMeta::Proto::SetVersioned(p_msg->mutable_file_path_hash(), GetDeclSourcePathHash(src));
        if (const auto* comment = context.getRawCommentForDeclNoCache(decl)) {
            UEMeta::Proto::SetVersioned(
                p_msg->mutable_documentation(), comment->getRawText(context.getSourceManager()).str());
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
    if (const auto* named = llvm::dyn_cast<clang::NamedDecl>(decl)) {
        p_msg->mutable_identifier()->set_qualified_name(GetFullyQualifiedName(context, named));
    }
    if (auto* as_enum = llvm::dyn_cast_or_null<clang::EnumDecl>(decl)) {
        p_msg->set_is_anonymous(as_enum->getName().empty());
    }
    else if (auto* as_record = llvm::dyn_cast_or_null<clang::RecordDecl>(decl)) {
        p_msg->set_is_anonymous(as_record->isAnonymousStructOrUnion() || as_record->getName().empty()); // catches nested anon structs as well
    }
    else {
        p_msg->set_is_anonymous(false);
    }
    UEMeta::Proto::SetVersioned(
        p_msg->mutable_content_hash(), std::hash<std::string_view>::operator()(ClangToString(context, decl)));
}

inline void PopulateTemplateDetails(clang::ASTContext& context, TemplateDetails* p_msg, const clang::Decl* decl) {
    if (!(p_msg && decl)) {
        UEM_WARN("Failed to PopulateTemplateDetails due to nullptr! p_msg={}, decl={}", !!p_msg, !!decl);
        return;
    }
    const auto PopulateDefaultType = [&](TemplateParameter* parameter, const clang::TemplateArgument& argument) {
        if (argument.getKind() == clang::TemplateArgument::Type) {
            PopulateTypeInfo(context, parameter->mutable_default_type(), argument.getAsType());
            return;
        }

        auto* default_type = parameter->mutable_default_type();
        const auto as_string = ClangToString(context, argument);
        UEMeta::Proto::SetVersioned(default_type->mutable_type(), as_string);
        UEMeta::Proto::SetVersioned(default_type->mutable_underlying_type(), as_string);
        UEMeta::Proto::MutableVersionItem(default_type->mutable_source_path_hash());
        default_type->set_is_templated_type(argument.isDependent());

        const clang::NamedDecl* template_declaration = nullptr;
        if ((argument.getKind() == clang::TemplateArgument::Template
             || argument.getKind() == clang::TemplateArgument::TemplateExpansion)
            && (template_declaration =
                    argument.getAsTemplateOrTemplatePattern().getAsTemplateDecl())) {
            UEMeta::Proto::SetVersioned(
                default_type->mutable_source_path_hash(),
                GetDeclSourcePathHash(GetDeclSourcePath(context, template_declaration)));
            PopulateIdentifier(
                context, default_type->mutable_identifier(), template_declaration);
        }
        else {
            PopulateIdentifier(
                context, default_type->mutable_identifier(), as_string,
                clang::QualType{}, clang::SourceLocation{});
        }
    };
    const auto PopulateParameters = [&](this auto self, const clang::TemplateParameterList* params, auto* p_params) -> void {
        if (!params) return;
        for (const auto* param : *params) {
            TemplateParameter* p_param = p_params->add_parameters();
            UEMeta::Proto::SetVersioned(
                p_param->mutable_occurrence_index(),
                static_cast<std::uint64_t>(p_params->parameters_size() - 1));
            PopulateIdentifier(context, p_param->mutable_identifier(), param);
            if (const auto* type_param = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
                p_param->set_kind(type_param->wasDeclaredWithTypename()
                                      ? TEMPLATE_PARAMETER_KIND_TYPENAME
                                      : TEMPLATE_PARAMETER_KIND_CLASS);
                if (type_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (type_param->hasDefaultArgument()) {
                    PopulateDefaultType(p_param, type_param->getDefaultArgument().getArgument());
                }
            }
            else if (const auto* non_type_param = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param)) {
                p_param->set_kind(TEMPLATE_PARAMETER_KIND_NON_TYPE);
                PopulateTypeInfo(context, p_param->mutable_type(), non_type_param->getType());
                if (non_type_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (non_type_param->hasDefaultArgument()) {
                    p_param->set_default_value(
                        ClangToString(context, non_type_param->getDefaultArgument().getArgument()));
                }
            }
            else if (const auto* template_param = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
                p_param->set_kind(template_param->wasDeclaredWithTypename()
                                      ? TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE
                                      : TEMPLATE_PARAMETER_KIND_CLASS_TEMPLATE);
                if (template_param->isParameterPack()) p_param->set_is_parameter_pack(true);
                if (template_param->hasDefaultArgument()) {
                    PopulateDefaultType(p_param, template_param->getDefaultArgument().getArgument());
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
        const auto SetPrimaryTemplateHash = [&] {
            if (const auto* spec = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(as_cxx)) {
                if (const auto* primary = spec->getSpecializedTemplate()) {
                    p_msg->set_primary_template_hash(GetDeclarationIdentityHash(primary));
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
            p_msg->set_primary_template_hash(GetDeclarationIdentityHash(as_cxx));
        }
        else {
            SetPrimaryTemplateHash();
        }
        return;
    }

    if (const auto* as_func = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        SetSpecializationKind(as_func->getTemplateSpecializationKind());
        if (const auto* primary = as_func->getDescribedFunctionTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_primary_template_hash(GetDeclarationIdentityHash(primary));
            return;
        }

        if (const auto* primary = as_func->getPrimaryTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            if (const auto* args = as_func->getTemplateSpecializationArgs()) {
                AddTemplateArguments(args->asArray());
            }
            p_msg->set_primary_template_hash(GetDeclarationIdentityHash(primary));
        }

        return;
    }

    if (const auto* as_alias = llvm::dyn_cast<clang::TypeAliasDecl>(decl)) {
        if (const auto* primary = as_alias->getDescribedAliasTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_NONE);
            p_msg->set_primary_template_hash(GetDeclarationIdentityHash(primary));
        }

        return;
    }

    if (const auto* as_var = llvm::dyn_cast<clang::VarDecl>(decl)) {
        SetSpecializationKind(as_var->getTemplateSpecializationKind());
        if (const auto* primary = as_var->getDescribedVarTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_primary_template_hash(GetDeclarationIdentityHash(primary));
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
                p_msg->set_primary_template_hash(GetDeclarationIdentityHash(primary));
            }
            AddTemplateArguments(spec->getTemplateArgs().asArray());
        }
    }
}

inline void PopulateFunctionCommon(clang::ASTContext& context, FunctionCommon* p_msg, clang::FunctionDecl* decl) {
    if (decl->getDescribedFunctionTemplate() || decl->getPrimaryTemplate()) {
        PopulateTemplateDetails(context, p_msg->mutable_template_details(), decl);
    }
    PopulateIdentifier(context, p_msg->mutable_identifier(), decl);
    PopulateTypeInfo(context, p_msg->mutable_return_type(), decl->getReturnType());
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

    UEMeta::Proto::SetVersioned(
        p_msg->mutable_consteval_kind(),
        decl->isConsteval() ? CONSTANT_EVALUATION_CONSTEVAL
        : decl->isConstexpr() ? CONSTANT_EVALUATION_CONSTEXPR : CONSTANT_EVALUATION_NONE);

    UEMeta::Proto::MutableVersionItem(p_msg->mutable_inline_definition());
    if (decl->doesThisDeclarationHaveABody()) {
        UEMeta::Proto::SetVersioned(
            p_msg->mutable_inline_definition(), ClangToString(context, decl->getBody()));
    }

    UEMeta::Proto::SetVersioned(
        p_msg->mutable_storage_class(),
        decl->getStorageClass() == clang::SC_Extern && decl->isExternC() ? FUN_VAR_STORAGE_CLASS_EXTERN_C
        : decl->getStorageClass() == clang::SC_Extern ? FUN_VAR_STORAGE_CLASS_EXTERN
        : decl->isStatic() ? FUN_VAR_STORAGE_CLASS_STATIC : FUN_VAR_STORAGE_CLASS_UNSPECIFIED);

    UEMeta::Proto::SetVersioned(
        p_msg->mutable_content_hash(), std::hash<std::string>::operator()(ClangToString(context, decl)));

    for (const auto param : decl->parameters()) {
        const auto p_param = p_msg->add_parameters();
        UEMeta::Proto::SetVersioned(
            p_param->mutable_occurrence_index(),
            static_cast<std::uint64_t>(p_msg->parameters_size() - 1));
        const auto str = ClangToString(context, param);
        UEMeta::Proto::SetVersioned(
            p_param->mutable_content_hash(), std::hash<std::string>::operator()(str));
        PopulateIdentifier(context, p_param->mutable_identifier(), param);
        PopulateTypeInfo(context, p_param->mutable_type_info(), param->getType());
        UEMeta::Proto::SetVersioned(
            p_param->mutable_default_value(), ClangToString(context, param->getInit()));
    }
}

#define EMP(s) if(proto->s().empty()) return false
#define NEZ(i) if(proto->i() <= 0) return false
#define NEQ(i, v) if(proto->i() == v) return false

inline bool Validate(const google::protobuf::Message* msg) {
    if (!msg) return false;

    const auto ValidateIdentifier = [](const Identifier* proto) {
        EMP(name);
        if (UEMeta::Proto::GetVersioned(proto->file_path()).empty()) return false;
        if (UEMeta::Proto::GetVersioned(proto->file_path_hash()) <= 0) return false;
        return true;
    };

    const auto ValidateMetadata = [&](const DeclarationMetadata* proto) {
        if (!ValidateIdentifier(&proto->identifier())) return false;
        if (proto->identifier().qualified_name().empty()) return false;
        if (proto->identifier().qualified_name_hash() == 0) return false;
        if (UEMeta::Proto::GetVersioned(proto->content_hash()) <= 0) return false;
        // no need to validate occurrence index since it can be zero
        return true;
    };

    const auto ValidateTemplateDetails = [&] (const TemplateDetails* proto) {
        const auto ValidateTemplateParam = [&] (this auto self, const TemplateParameter* proto) {
            if (!ValidateIdentifier(&proto->identifier())) return false;
            NEQ(kind, TEMPLATE_PARAMETER_KIND_UNSPECIFIED);
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
