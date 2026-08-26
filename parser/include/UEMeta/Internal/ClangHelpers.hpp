#pragma once
#include "UEMeta/Cli.hpp"
#include "UEMeta/Internal/ProtoHelpers.hpp"
#include <atomic>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Comment.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/NestedNameSpecifier.h>
#include <llvm/Support/xxhash.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

inline const clang::NamedDecl* GetBaseTypeIdentityDecl(const clang::QualType type) {
    if (type.isNull()) return nullptr;

    if (const auto* record = type->getAsCXXRecordDecl()) {
        return record->getDefinitionOrSelf();
    }

    const auto* type_ptr = type.getTypePtr();
    if (const auto* pack = llvm::dyn_cast<clang::PackExpansionType>(type_ptr)) {
        return GetBaseTypeIdentityDecl(pack->getPattern());
    }
    if (const auto* specialization = llvm::dyn_cast<clang::TemplateSpecializationType>(type_ptr)) {
        return specialization->getTemplateName().getAsTemplateDecl(/*IgnoreDeduced=*/true);
    }
    if (const auto* parameter = llvm::dyn_cast<clang::TemplateTypeParmType>(type_ptr)) {
        return parameter->getDecl();
    }
    if (const auto* substituted = llvm::dyn_cast<clang::SubstTemplateTypeParmType>(type_ptr)) {
        if (const auto* parameter = substituted->getReplacedParameter()) return parameter;
        return GetBaseTypeIdentityDecl(substituted->getReplacementType());
    }
    if (const auto* unresolved = llvm::dyn_cast<clang::UnresolvedUsingType>(type_ptr)) {
        return unresolved->getDecl();
    }
    return nullptr;
}

inline const clang::NamedDecl* GetBaseTypeDependencyDecl(clang::QualType type);

inline const clang::NamedDecl* GetQualifierDependencyDecl(const clang::NestedNameSpecifier qualifier) {
    switch (qualifier.getKind()) {
        case clang::NestedNameSpecifier::Kind::Type:
            return GetBaseTypeDependencyDecl(clang::QualType{qualifier.getAsType(), 0});
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

inline const clang::NamedDecl* GetBaseTypeDependencyDecl(const clang::QualType type) {
    if (type.isNull()) return nullptr;
    if (const auto* identity = GetBaseTypeIdentityDecl(type)) return identity;

    const auto* type_ptr = type.getTypePtr();
    if (const auto* pack = llvm::dyn_cast<clang::PackExpansionType>(type_ptr)) {
        return GetBaseTypeDependencyDecl(pack->getPattern());
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

inline std::pair<std::string, std::string> GetSyntheticBaseTypeIdentity(
    const clang::QualType type, const clang::PrintingPolicy& policy) {
    if (const auto* pack = llvm::dyn_cast<clang::PackExpansionType>(type.getTypePtr())) {
        return GetSyntheticBaseTypeIdentity(pack->getPattern(), policy);
    }

    if (const auto* dependent = llvm::dyn_cast<clang::DependentNameType>(type.getTypePtr())) {
        std::string qualified_name;
        llvm::raw_string_ostream out(qualified_name);
        dependent->getQualifier().print(out, policy);
        out << dependent->getIdentifier()->getName();
        out.flush();
        return {dependent->getIdentifier()->getName().str(), std::move(qualified_name)};
    }

    if (const auto* specialization = llvm::dyn_cast<clang::TemplateSpecializationType>(type.getTypePtr())) {
        if (const auto* dependent = specialization->getTemplateName().getAsDependentTemplateName()) {
            if (const auto* identifier = dependent->getName().getIdentifier()) {
                std::string qualified_name;
                llvm::raw_string_ostream out(qualified_name);
                dependent->getQualifier().print(out, policy);
                out << identifier->getName();
                out.flush();
                return {identifier->getName().str(), std::move(qualified_name)};
            }
        }
    }

    auto qualified_name = type.getAsString(policy);
    const auto* identifier = type.getBaseTypeIdentifier();
    auto name = identifier ? identifier->getName().str() : qualified_name;
    return {std::move(name), std::move(qualified_name)};
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

/// The template information visible at a declaration.  The shapes are used in
/// every descendant segment, while replacements make type/argument spelling
/// independent of template parameter identifiers.
struct CanonicalTemplateEnvironment {
    std::vector<std::string> shapes;
    std::vector<std::pair<std::string, std::string>> replacements;
};

/// A fixed policy is important here: ASTContext's policy reflects command-line
/// choices and therefore is not suitable for a persistent type identifier.
inline clang::PrintingPolicy GetCanonicalPrintingPolicy(const clang::ASTContext& context) {
    clang::PrintingPolicy policy(context.getLangOpts());
    policy.SuppressTagKeyword = true;
    policy.SuppressTagKeywordInAnonNames = true;
    policy.IncludeTagDefinition = false;
    policy.SuppressScope = false;
    policy.SuppressUnwrittenScope = false;
    policy.SuppressInlineNamespace = static_cast<unsigned>(
        clang::PrintingPolicy::SuppressInlineNamespaceMode::None);
    policy.SuppressInitializers = true;
    policy.ConstantArraySizeAsWritten = false;
    policy.AnonymousTagLocations = false;
    policy.SuppressDefaultTemplateArgs = false;
    policy.Bool = true;
    policy.Nullptr = true;
    policy.NullptrTypeInNamespace = true;
    policy.Restrict = true;
    policy.Alignof = true;
    policy.UnderscoreAlignof = false;
    policy.UseVoidForZeroParams = false;
    policy.SplitTemplateClosers = false;
    policy.TerseOutput = true;
    policy.MSWChar = false;
    policy.IncludeNewlines = false;
    policy.MSVCFormatting = false;
    policy.ConstantsAsWritten = false;
    policy.FullyQualifiedName = false;
    policy.PrintAsCanonical = false;
    policy.PrintInjectedClassNameWithArguments = false;
    policy.UsePreferredNames = false;
    policy.AlwaysIncludeTypeForTemplateArgument = false;
    policy.CleanUglifiedParameters = false;
    return policy;
}

inline bool IsCanonicalIdentifierStart(const char c) {
    return c == '_' || c == '$' || std::isalpha(static_cast<unsigned char>(c));
}

inline bool IsCanonicalIdentifierContinue(const char c) {
    return IsCanonicalIdentifierStart(c) || std::isdigit(static_cast<unsigned char>(c));
}

inline bool IsCanonicalWordBoundaryChar(const char c) {
    return IsCanonicalIdentifierContinue(c) || c == '\'';
}

/// Normalizes whitespace and replaces visible template parameter identifiers.
/// Replacements are token-based so a parameter named T does not alter Thing.
inline std::string NormalizeCanonicalSpelling(
    const std::string_view input, const CanonicalTemplateEnvironment& environment) {
    std::string replaced;
    replaced.reserve(input.size());
    for (std::size_t i = 0; i < input.size();) {
        // Clang uses depth/index placeholders for dependent template arguments
        // that no longer retain their source-level parameter spelling.
        constexpr std::string_view type_parameter_prefix{"type-parameter-"};
        if (input.substr(i).starts_with(type_parameter_prefix)) {
            auto end = i + type_parameter_prefix.size();
            const auto first_digit = end;
            while (end < input.size() && std::isdigit(static_cast<unsigned char>(input[end]))) ++end;
            if (end > first_digit && end < input.size() && input[end] == '-') {
                const auto second_digit = ++end;
                while (end < input.size() && std::isdigit(static_cast<unsigned char>(input[end]))) ++end;
                if (end > second_digit
                    && (end == input.size() || !IsCanonicalIdentifierContinue(input[end]))) {
                    replaced += "typename";
                    i = end;
                    continue;
                }
            }
        }

        if (!IsCanonicalIdentifierStart(input[i])) {
            replaced.push_back(input[i++]);
            continue;
        }

        const auto begin = i++;
        while (i < input.size() && IsCanonicalIdentifierContinue(input[i])) ++i;
        const auto token = input.substr(begin, i - begin);
        const auto previous_non_space = [&]() -> std::size_t {
            auto position = begin;
            while (position > 0 && std::isspace(static_cast<unsigned char>(input[position - 1]))) --position;
            return position;
        }();
        const bool is_qualified_member = previous_non_space >= 2
                                      && input[previous_non_space - 1] == ':'
                                      && input[previous_non_space - 2] == ':';

        bool did_replace = false;
        if (!is_qualified_member) {
            for (auto replacement = environment.replacements.rbegin();
                 replacement != environment.replacements.rend(); ++replacement) {
                if (token == replacement->first) {
                    replaced.append(replacement->second);
                    did_replace = true;
                    break;
                }
            }
        }
        if (!did_replace) replaced.append(token);
    }

    std::string result;
    result.reserve(replaced.size());
    bool saw_space = false;
    for (const char c : replaced) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            saw_space = !result.empty();
            continue;
        }

        if (saw_space && !result.empty()) {
            const char previous = result.back();
            // Whitespace is significant only between word-like tokens.  It is
            // deliberately removed next to C++ declarator punctuation.
            if (IsCanonicalWordBoundaryChar(previous) && IsCanonicalWordBoundaryChar(c)) {
                result.push_back(' ');
            }
        }
        result.push_back(c);
        saw_space = false;
    }
    return result;
}

inline std::string CanonicalTypeSpelling(
    clang::ASTContext& context, clang::QualType type,
    const CanonicalTemplateEnvironment& environment, const bool signature_parameter = false) {
    if (type.isNull()) return {};
    if (signature_parameter) type = context.getSignatureParameterType(type);
    return NormalizeCanonicalSpelling(type.getAsString(GetCanonicalPrintingPolicy(context)), environment);
}

inline std::string CanonicalTemplateArgumentSpelling(
    clang::ASTContext& context, const clang::TemplateArgument& argument,
    const CanonicalTemplateEnvironment& environment) {
    if (argument.getKind() == clang::TemplateArgument::Pack) {
        std::string result;
        for (const auto& element : argument.pack_elements()) {
            if (!result.empty()) result.push_back(',');
            result += CanonicalTemplateArgumentSpelling(context, element, environment);
        }
        return result;
    }

    std::string spelling;
    llvm::raw_string_ostream out(spelling);
    argument.print(GetCanonicalPrintingPolicy(context), out, false);
    out.flush();
    return NormalizeCanonicalSpelling(spelling, environment);
}

inline const clang::TemplateParameterList* GetDirectTemplateParameters(const clang::NamedDecl* declaration) {
    if (!declaration
        || llvm::isa<clang::TemplateTypeParmDecl, clang::NonTypeTemplateParmDecl,
                     clang::TemplateTemplateParmDecl>(declaration)) {
        return nullptr;
    }
    if (const auto* template_decl = llvm::dyn_cast<clang::TemplateDecl>(declaration);
        template_decl && !llvm::isa<clang::TemplateTemplateParmDecl>(template_decl)) {
        return template_decl->getTemplateParameters();
    }
    if (const auto* record = llvm::dyn_cast<clang::CXXRecordDecl>(declaration)) {
        if (const auto* partial = llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(record)) {
            return partial->getTemplateParameters();
        }
        if (const auto* described = record->getDescribedClassTemplate()) {
            return described->getTemplateParameters();
        }
        if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
            if (const auto* primary = specialization->getSpecializedTemplate()) {
                return primary->getTemplateParameters();
            }
        }
    }
    if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration)) {
        if (const auto* described = function->getDescribedFunctionTemplate()) {
            return described->getTemplateParameters();
        }
        if (const auto* primary = function->getPrimaryTemplate()) {
            return primary->getTemplateParameters();
        }
    }
    if (const auto* alias = llvm::dyn_cast<clang::TypeAliasDecl>(declaration)) {
        if (const auto* described = alias->getDescribedAliasTemplate()) {
            return described->getTemplateParameters();
        }
    }
    if (const auto* variable = llvm::dyn_cast<clang::VarDecl>(declaration)) {
        if (const auto* partial = llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(variable)) {
            return partial->getTemplateParameters();
        }
        if (const auto* described = variable->getDescribedVarTemplate()) {
            return described->getTemplateParameters();
        }
        if (const auto* specialization = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(variable)) {
            if (const auto* primary = specialization->getSpecializedTemplate()) {
                return primary->getTemplateParameters();
            }
        }
    }
    return nullptr;
}

inline bool IsTemplateParameterPack(const clang::NamedDecl* parameter) {
    if (const auto* type = llvm::dyn_cast<clang::TemplateTypeParmDecl>(parameter)) {
        return type->isParameterPack();
    }
    if (const auto* non_type = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(parameter)) {
        return non_type->isParameterPack();
    }
    if (const auto* nested = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(parameter)) {
        return nested->isParameterPack();
    }
    return false;
}

inline std::string CanonicalTemplateParameterShape(
    clang::ASTContext& context, const clang::NamedDecl* parameter,
    const CanonicalTemplateEnvironment& environment) {
    if (const auto* type = llvm::dyn_cast<clang::TemplateTypeParmDecl>(parameter)) {
        return std::string{"typename"} + (type->isParameterPack() ? "..." : "");
    }
    if (const auto* non_type = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(parameter)) {
        return CanonicalTypeSpelling(context, non_type->getType(), environment)
             + (non_type->isParameterPack() ? "..." : "");
    }
    if (const auto* nested = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(parameter)) {
        auto nested_environment = environment;
        std::string shape{"template<"};
        bool first = true;
        for (const auto* nested_parameter : *nested->getTemplateParameters()) {
            if (!first) shape.push_back(',');
            const auto nested_shape = CanonicalTemplateParameterShape(context, nested_parameter, nested_environment);
            shape += nested_shape;
            if (!nested_parameter->getName().empty()) {
                auto nested_reference = nested_shape;
                if (IsTemplateParameterPack(nested_parameter) && nested_reference.ends_with("...")) {
                    nested_reference.resize(nested_reference.size() - 3);
                }
                nested_environment.replacements.emplace_back(
                    nested_parameter->getName().str(), std::move(nested_reference));
            }
            first = false;
        }
        shape += ">typename";
        if (nested->isParameterPack()) shape += "...";
        return shape;
    }
    return "typename";
}

inline std::string CanonicalTemplateParameterReference(
    const clang::NamedDecl* parameter, std::string shape) {
    // The ellipsis belongs in the template declaration shape, but a use of a
    // pack already carries its own expansion ellipsis (Ts...). Keeping both
    // would produce `typename......` in function parameter types.
    if (IsTemplateParameterPack(parameter) && shape.ends_with("...")) {
        shape.resize(shape.size() - 3);
    }
    return shape;
}

inline void AddTemplateParametersToEnvironment(
    clang::ASTContext& context, const clang::TemplateParameterList* parameters,
    CanonicalTemplateEnvironment& environment) {
    if (!parameters) return;
    for (const auto* parameter : *parameters) {
        auto shape = CanonicalTemplateParameterShape(context, parameter, environment);
        environment.shapes.emplace_back(shape);
        if (!parameter->getName().empty()) {
            environment.replacements.emplace_back(
                parameter->getName().str(), CanonicalTemplateParameterReference(parameter, std::move(shape)));
        }
    }
}

inline std::string CanonicalTemplatePrefix(const CanonicalTemplateEnvironment& environment) {
    if (environment.shapes.empty()) return {};
    std::string result{"template<"};
    for (std::size_t index = 0; index < environment.shapes.size(); ++index) {
        if (index) result.push_back(',');
        result += environment.shapes[index];
    }
    result.push_back('>');
    return result;
}

inline std::string PlainDeclName(const clang::NamedDecl* declaration, const clang::PrintingPolicy& policy) {
    if (!declaration) return {};
    if (const auto* parameter = llvm::dyn_cast<clang::ParmVarDecl>(declaration);
        parameter && parameter->getName().empty()) {
        return "<parameter" + std::to_string(parameter->getFunctionScopeIndex()) + ">";
    }
    if (!declaration->getNameAsString().empty()) return declaration->getNameAsString();

    std::string result;
    llvm::raw_string_ostream out(result);
    declaration->getNameForDiagnostic(out, policy, true);
    out.flush();
    return result.empty() ? "(anonymous)" : result;
}

inline void CollectNamedDeclPath(const clang::DeclContext* declaration_context,
                                 std::vector<const clang::NamedDecl*>& result) {
    if (!declaration_context || declaration_context->isTranslationUnit()) return;
    CollectNamedDeclPath(declaration_context->getParent(), result);

    const auto* declaration = clang::Decl::castFromDeclContext(declaration_context);
    const auto* named = llvm::dyn_cast<clang::NamedDecl>(declaration);
    if (!named) return;
    if (llvm::isa<clang::NamespaceDecl, clang::RecordDecl, clang::FunctionDecl>(named)) {
        result.push_back(named);
        return;
    }
    if (const auto* enumeration = llvm::dyn_cast<clang::EnumDecl>(named);
        enumeration && enumeration->isScoped()) {
        result.push_back(named);
    }
}

inline std::vector<const clang::NamedDecl*> GetNamedDeclPath(const clang::NamedDecl* declaration) {
    std::vector<const clang::NamedDecl*> result;
    if (!declaration) return result;
    CollectNamedDeclPath(declaration->getDeclContext(), result);
    result.push_back(declaration);
    return result;
}

inline const clang::FunctionDecl* GetCanonicalFunctionPattern(const clang::FunctionDecl* declaration) {
    if (!declaration) return nullptr;
    if (const auto* described = declaration->getDescribedFunctionTemplate()) {
        return described->getTemplatedDecl();
    }
    if (const auto* primary = declaration->getPrimaryTemplate()) {
        return primary->getTemplatedDecl();
    }
    if (const auto* instantiated = declaration->getInstantiatedFromMemberFunction()) {
        return instantiated;
    }
    if (const auto* pattern = declaration->getTemplateInstantiationPattern(false);
        pattern && pattern != declaration) {
        return pattern;
    }
    return declaration;
}

inline void AppendCanonicalTemplateArguments(
    clang::ASTContext& context, const clang::NamedDecl* declaration,
    const CanonicalTemplateEnvironment& environment, std::string& result) {
    const auto Append = [&](const auto& arguments) {
        result.push_back('<');
        bool first = true;
        for (const auto& argument : arguments) {
            if (!first) result.push_back(',');
            result += CanonicalTemplateArgumentSpelling(context, argument, environment);
            first = false;
        }
        result.push_back('>');
    };

    if (const auto* record = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(declaration)) {
        Append(record->getTemplateArgs().asArray());
    }
    else if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration)) {
        if (const auto* arguments = function->getTemplateSpecializationArgs()) {
            Append(arguments->asArray());
        }
    }
    else if (const auto* variable = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(declaration)) {
        Append(variable->getTemplateArgs().asArray());
    }
}

inline std::string CanonicalDeclSegment(
    clang::ASTContext& context, const clang::NamedDecl* declaration,
    CanonicalTemplateEnvironment& environment) {
    const auto* direct_parameters = GetDirectTemplateParameters(declaration);
    AddTemplateParametersToEnvironment(context, direct_parameters, environment);

    const bool template_bearing_record = llvm::isa<clang::RecordDecl>(declaration)
                                      && !environment.shapes.empty();
    const bool template_bearing_function = llvm::isa<clang::FunctionDecl>(declaration)
                                        && !environment.shapes.empty();
    const bool directly_templated_alias = llvm::isa<clang::TypeAliasDecl>(declaration)
                                       && direct_parameters;
    const bool directly_templated_variable = llvm::isa<clang::VarDecl>(declaration)
                                          && direct_parameters;
    std::string result;
    if (template_bearing_record || template_bearing_function
        || directly_templated_alias || directly_templated_variable) {
        result = CanonicalTemplatePrefix(environment);
    }
    // Entity names remain spelled.  Only types and template arguments lose
    // template-parameter identifiers.
    result += PlainDeclName(declaration, GetCanonicalPrintingPolicy(context));
    AppendCanonicalTemplateArguments(context, declaration, environment, result);

    if (const auto* function = llvm::dyn_cast<clang::FunctionDecl>(declaration)) {
        const auto* pattern = GetCanonicalFunctionPattern(function);
        result.push_back('(');
        bool first = true;
        for (const auto* parameter : pattern->parameters()) {
            if (!first) result.push_back(',');
            result += CanonicalTypeSpelling(context, parameter->getType(), environment, true);
            first = false;
        }
        if (pattern->isVariadic()) {
            if (!first) result.push_back(',');
            result += "...";
        }
        result.push_back(')');

        if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(pattern)) {
            if (method->isConst()) result += "const";
            if (method->isVolatile()) result += "volatile";
            if (method->getRefQualifier() == clang::RQ_LValue) result.push_back('&');
            else if (method->getRefQualifier() == clang::RQ_RValue) result += "&&";
        }
    }
    return result;
}

/// Builds the persistent, compiler-independent declaration identity.
inline std::string GetCanonicalQualifiedName(clang::ASTContext& context, const clang::NamedDecl* declaration) {
    if (!declaration) return {};
    if (const auto* template_decl = llvm::dyn_cast<clang::TemplateDecl>(declaration);
        template_decl && !llvm::isa<clang::TemplateTemplateParmDecl>(template_decl)) {
        // TemplateTemplateParmDecl is a TemplateDecl whose templated
        // declaration is null; it remains the named entity being identified.
        if (const auto* templated_decl = template_decl->getTemplatedDecl()) {
            declaration = templated_decl;
        }
    }

    const auto legacy_qualified_name = declaration->getQualifiedNameAsString();
    if (declaration->getNameAsString().empty()) {
        auto result = NormalizeCanonicalSpelling(legacy_qualified_name, {});
        if (!result.starts_with("::")) result.insert(0, "::");
        return result;
    }

    // Clang intentionally leaves some template parameters unqualified (most
    // notably those belonging to free-function templates). Preserve that
    // existing identity instead of inventing a lexical owner.
    if (llvm::isa<clang::TemplateTypeParmDecl, clang::NonTypeTemplateParmDecl,
                  clang::TemplateTemplateParmDecl>(declaration)
        && legacy_qualified_name.find("::") == std::string::npos) {
        return "::" + NormalizeCanonicalSpelling(legacy_qualified_name, {});
    }
    CanonicalTemplateEnvironment environment;
    std::string result;
    for (const auto* segment : GetNamedDeclPath(declaration)) {
        result += "::";
        result += CanonicalDeclSegment(context, segment, environment);
    }
    return result.empty() ? "::" : result;
}

inline std::string GetLegacyQualifiedName(const clang::NamedDecl* declaration) {
    if (!declaration) return {};
    if (const auto* parameter = llvm::dyn_cast<clang::ParmVarDecl>(declaration)) {
        if (const auto* owner = llvm::dyn_cast<clang::NamedDecl>(
                clang::Decl::castFromDeclContext(parameter->getDeclContext()))) {
            auto result = owner->getQualifiedNameAsString();
            result += "::";
            result += parameter->getQualifiedNameAsString();
            return result;
        }
    }
    return declaration->getQualifiedNameAsString();
}

inline std::vector<std::string> GetPlainDeclScopes(
    clang::ASTContext&, const clang::NamedDecl* declaration) {
    std::vector<std::string> result;
    const auto qualified_name = GetLegacyQualifiedName(declaration);
    std::size_t begin = 0;
    while (begin <= qualified_name.size()) {
        const auto end = qualified_name.find("::", begin);
        auto segment = qualified_name.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        if (!segment.starts_with("operator")) {
            if (const auto template_arguments = segment.find('<');
                template_arguments != std::string::npos) {
                segment.erase(template_arguments);
            }
        }
        if (!segment.empty()) result.emplace_back(segment);
        if (end == std::string::npos) break;
        begin = end + 2;
    }
    return result;
}

inline std::uint64_t GetQualifiedNameHash(const std::string_view qualified_name) {
    return llvm::xxHash64(llvm::StringRef{qualified_name.data(), qualified_name.size()});
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

inline void PopulateTypeInfo(clang::ASTContext& context, TypeInfo* p_msg, const clang::QualType& type) {
    const clang::QualType underlying_type = GetUnderlyingType(type);
    UEMeta::Proto::SetVersioned(p_msg->mutable_type(), type.getAsString());
    UEMeta::Proto::SetVersioned(p_msg->mutable_underlying_type(), underlying_type.getAsString());
    UEMeta::Proto::MutableVersionItem(p_msg->mutable_source_path_hash());
    const bool is_dependent_type = type->isDependentType();
    p_msg->set_is_templated_type(is_dependent_type);

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

inline void PopulateIdentifierScopes(Identifier* p_msg, const std::vector<std::string>& scopes) {
    p_msg->clear_scope();
    for (const auto& scope : scopes) {
        if (!scope.empty()) p_msg->add_scope(scope);
    }
}

/// Extracts plain access-path scopes from a raw C++ qualified name.  This is
/// used only for synthetic dependent base identities, for which Clang has no
/// concrete NamedDecl.  Canonical signatures are never parsed to make scopes.
inline std::vector<std::string> GetPlainScopesFromRawQualifiedName(const std::string_view qualified_name) {
    std::vector<std::string> result;
    std::string current;
    unsigned angle_depth = 0;
    unsigned parenthesis_depth = 0;
    unsigned bracket_depth = 0;
    const auto Commit = [&] {
        const auto first = current.find_first_not_of(" \t\n\r:");
        const auto last = current.find_last_not_of(" \t\n\r:");
        if (first != std::string::npos) result.emplace_back(current.substr(first, last - first + 1));
        current.clear();
    };

    for (std::size_t index = 0; index < qualified_name.size(); ++index) {
        const char c = qualified_name[index];
        if (c == '<') {
            if (angle_depth++ == 0) continue;
        }
        else if (c == '>' && angle_depth) {
            if (--angle_depth == 0) continue;
        }
        else if (c == '(') ++parenthesis_depth;
        else if (c == ')' && parenthesis_depth) --parenthesis_depth;
        else if (c == '[') ++bracket_depth;
        else if (c == ']' && bracket_depth) --bracket_depth;

        if (angle_depth == 0 && parenthesis_depth == 0 && bracket_depth == 0
            && c == ':' && index + 1 < qualified_name.size() && qualified_name[index + 1] == ':') {
            Commit();
            ++index;
            continue;
        }
        if (angle_depth == 0) current.push_back(c);
    }
    Commit();
    return result;
}

/// @brief Fills out an Identifier for a syntactically named entity that has no concrete Decl yet.
inline void PopulateIdentifier(clang::ASTContext& context, Identifier* p_msg, const std::string_view name,
                               const std::string_view qualified_name, const clang::SourceLocation location) {
    if (!p_msg) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! p_msg=false");
        return;
    }
    UEMeta::Proto::MutableVersionItem(p_msg->mutable_documentation());

    p_msg->set_name(std::string{name});
    auto canonical_name = NormalizeCanonicalSpelling(qualified_name, {});
    if (!canonical_name.starts_with("::")) canonical_name.insert(0, "::");
    p_msg->set_qualified_name(std::move(canonical_name));
    p_msg->set_qualified_name_hash(GetQualifiedNameHash(p_msg->qualified_name()));

    const auto& source_man = context.getSourceManager();
    const auto source_loc = source_man.getExpansionLoc(location);
    const auto source_path = source_man.getFilename(source_loc);
    UEMeta::Proto::SetVersioned(p_msg->mutable_file_path(), source_path.str());
    UEMeta::Proto::SetVersioned(p_msg->mutable_file_path_hash(), GetDeclSourcePathHash(source_path));
    PopulateIdentifierScopes(p_msg, GetPlainScopesFromRawQualifiedName(qualified_name));
}

/// @brief Fills out an Identifier from a Decl
inline void PopulateIdentifier(clang::ASTContext& context, Identifier* p_msg, const clang::Decl* decl) {
    if (!(p_msg && decl)) {
        UEM_WARN("Failed to PopulateIdentifier due to nullptr! p_msg={}, decl={}", !!p_msg, !!decl);
        return;
    }
    UEMeta::Proto::MutableVersionItem(p_msg->mutable_documentation());
    if (auto* as_named = llvm::dyn_cast_or_null<clang::NamedDecl>(decl)) {
        if (const auto* parameter = llvm::dyn_cast<clang::ParmVarDecl>(as_named);
            parameter && parameter->getName().empty()) {
            p_msg->set_name(PlainDeclName(parameter, GetCanonicalPrintingPolicy(context)));
        }
        else {
            // Anonymous tags retain their source-level empty name; the
            // qualified identifier still carries Clang's unique fallback.
            p_msg->set_name(as_named->getNameAsString());
        }
        p_msg->set_qualified_name(GetCanonicalQualifiedName(context, as_named));
        p_msg->set_qualified_name_hash(GetQualifiedNameHash(p_msg->qualified_name()));

        const auto src = GetDeclSourcePath(context, decl);
        UEMeta::Proto::SetVersioned(p_msg->mutable_file_path(), src.str());
        UEMeta::Proto::SetVersioned(p_msg->mutable_file_path_hash(), GetDeclSourcePathHash(src));
        if (const auto* comment = context.getRawCommentForDeclNoCache(decl)) {
            UEMeta::Proto::SetVersioned(
                p_msg->mutable_documentation(), comment->getRawText(context.getSourceManager()).str());
        }
        PopulateIdentifierScopes(p_msg, GetPlainDeclScopes(context, as_named));
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

        if ((argument.getKind() == clang::TemplateArgument::Template
             || argument.getKind() == clang::TemplateArgument::TemplateExpansion)
            && argument.getAsTemplateOrTemplatePattern().getAsTemplateDecl()) {
            UEMeta::Proto::SetVersioned(
                default_type->mutable_source_path_hash(),
                GetDeclSourcePathHash(GetDeclSourcePath(
                    context, argument.getAsTemplateOrTemplatePattern().getAsTemplateDecl())));
        }
    };
    const auto PopulateParameters = [&](this auto self, const clang::TemplateParameterList* params, auto* p_params) -> void {
        if (!params) return;
        for (const auto* param : *params) {
            TemplateParameter* p_param = p_params->add_parameters();
            p_param->set_occurrence_index(
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
        const auto SetPrimaryTemplateQName = [&] {
            if (const auto* spec = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(as_cxx)) {
                if (const auto* primary = spec->getSpecializedTemplate()) {
                    p_msg->set_primary_template_qualified_name(GetCanonicalQualifiedName(context, primary));
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
            p_msg->set_primary_template_qualified_name(GetCanonicalQualifiedName(context, as_cxx));
        }
        else {
            SetPrimaryTemplateQName();
        }
        return;
    }

    if (const auto* as_func = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
        SetSpecializationKind(as_func->getTemplateSpecializationKind());
        if (const auto* primary = as_func->getDescribedFunctionTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_primary_template_qualified_name(GetCanonicalQualifiedName(context, primary));
            return;
        }

        if (const auto* primary = as_func->getPrimaryTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            if (const auto* args = as_func->getTemplateSpecializationArgs()) {
                AddTemplateArguments(args->asArray());
            }
            p_msg->set_primary_template_qualified_name(GetCanonicalQualifiedName(context, primary));
        }

        return;
    }

    if (const auto* as_alias = llvm::dyn_cast<clang::TypeAliasDecl>(decl)) {
        if (const auto* primary = as_alias->getDescribedAliasTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_specialization_kind(TEMPLATE_SPECIALIZATION_NONE);
            p_msg->set_primary_template_qualified_name(GetCanonicalQualifiedName(context, primary));
        }

        return;
    }

    if (const auto* as_var = llvm::dyn_cast<clang::VarDecl>(decl)) {
        SetSpecializationKind(as_var->getTemplateSpecializationKind());
        if (const auto* primary = as_var->getDescribedVarTemplate()) {
            PopulateParameters(primary->getTemplateParameters(), p_msg);
            p_msg->set_primary_template_qualified_name(GetCanonicalQualifiedName(context, primary));
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
                p_msg->set_primary_template_qualified_name(GetCanonicalQualifiedName(context, primary));
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
        p_param->set_occurrence_index(
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
        EMP(qualified_name);
        if (UEMeta::Proto::GetVersioned(proto->file_path()).empty()) return false;
        EMP(scope);
        if (UEMeta::Proto::GetVersioned(proto->file_path_hash()) <= 0) return false;
        NEZ(qualified_name_hash);
        return true;
    };

    const auto ValidateMetadata = [&](const DeclarationMetadata* proto) {
        if (!ValidateIdentifier(&proto->identifier())) return false;
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
