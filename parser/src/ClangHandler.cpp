// ReSharper disable CppMemberFunctionMayBeStatic
#include "UEMeta/ClangHandler.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <exception>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/GlobalDecl.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RawCommentList.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/TemplateBase.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/ExceptionSpecificationType.h>
#include <clang/Basic/FileEntry.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/MD5.h>
#include <llvm/Support/raw_ostream.h>

#include "UEMeta/Cli.hpp"
#include "UEMeta/StablePath.hpp"

/// @brief Tracks whether any guarded Clang visitor or callback caught an exception.
static std::atomic_bool GClangExceptionCaught{false};

/// @brief Records a Clang processing exception and logs the failing step.
static void LogClangException(const std::string_view step, const std::exception& ex) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with exception: {}", step, ex.what());
    } catch (...) {
    }
}

/// @brief Records an unknown Clang processing exception and logs the failing step.
static void LogClangUnknownException(const std::string_view step) noexcept {
    GClangExceptionCaught.store(true, std::memory_order_relaxed);
    try {
        UEM_ERROR("(clang) {} failed with unknown exception", step);
    } catch (...) {
    }
}

/// @brief Runs a visitor callback with exception translation into Clang traversal failure.
template <typename Func>
static bool GuardClangVisitor(const std::string_view step, Func&& func) noexcept {
    try {
        return std::forward<Func>(func)();
    } catch (const std::exception& ex) {
        LogClangException(step, ex);
    } catch (...) {
        LogClangUnknownException(step);
    }

    return false;
}

/// @brief Runs a non-visitor Clang callback with exception logging and failure recording.
template <typename Func>
static void GuardClangCallback(const std::string_view step, Func&& func) noexcept {
    try {
        std::forward<Func>(func)();
    } catch (const std::exception& ex) {
        LogClangException(step, ex);
    } catch (...) {
        LogClangUnknownException(step);
    }
}

/// @brief Builds the type/declaration printing policy used for emitted C++ type strings.
static clang::PrintingPolicy MakePrintingPolicy(const clang::ASTContext& ctx) {
    clang::PrintingPolicy policy{ctx.getLangOpts()};
    policy.adjustForCPlusPlus();
    policy.SuppressScope = false;
    policy.SuppressUnwrittenScope = true;
    policy.SuppressTagKeyword = true;
    policy.ConstantsAsWritten = false;
    return policy;
}

/// @brief Pretty-prints a Clang type, optionally with a placeholder declarator name.
static std::string PrintType(const clang::ASTContext& ctx, const clang::QualType type, const std::string& placeholder = {}) {
    if (type.isNull()) {
        return "";
    }

    std::string out;
    llvm::raw_string_ostream stream{out};
    type.print(stream, MakePrintingPolicy(ctx), placeholder);
    return stream.str();
}

/// @brief Walks through transparent declaration contexts to the first semantic parent context.
static const clang::DeclContext* NonTransparentContext(const clang::DeclContext* context) {
    while (context && context->isTransparentContext()) {
        context = context->getParent();
    }
    return context;
}

/// @brief Returns a fully qualified declaration name with a leading global scope qualifier.
static inline std::string QualifiedName(const clang::NamedDecl* decl) {
    if (!decl || !decl->getDeclName()) {
        return "";
    }

    auto value = decl->getQualifiedNameAsString();
    if (value.empty() || value.starts_with("::")) {
        return value;
    }
    return "::" + value;
}

/// @brief Converts a Clang access specifier to the JSON access string.
static std::string AccessToString(const clang::AccessSpecifier access) {
    switch (access) {
        case clang::AS_public:
            return "public";
        case clang::AS_protected:
            return "protected";
        case clang::AS_private:
            return "private";
        case clang::AS_none:
            return "";
    }

    return "";
}

/// @brief Converts a Clang storage class to the JSON storage class string.
static std::string StorageClassToString(const clang::StorageClass storage_class) {
    switch (storage_class) {
        case clang::SC_None:
            return "";
        case clang::SC_Extern:
            return "extern";
        case clang::SC_Static:
            return "static";
        case clang::SC_PrivateExtern:
            return "private_extern";
        case clang::SC_Auto:
            return "auto";
        case clang::SC_Register:
            return "register";
    }

    return "";
}

/// @brief Computes a lowercase hexadecimal MD5 digest for a string.
static std::string Md5Hex(const std::string_view value) {
    llvm::MD5 hash;
    hash.update(llvm::StringRef{value.data(), value.size()});
    auto result = hash.final();
    const auto digest = result.digest();
    return digest.str().str();
}

/// @brief Converts a member function reference qualifier to its source spelling.
static std::string RefQualifierToString(const clang::RefQualifierKind qualifier) {
    switch (qualifier) {
        case clang::RQ_None:
            return "";
        case clang::RQ_LValue:
            return "&";
        case clang::RQ_RValue:
            return "&&";
    }

    return "";
}

/// @brief Converts a Clang exception specification kind to the JSON exceptionSpec spelling.
static std::string ExceptionSpecToString(const clang::ExceptionSpecificationType spec) {
    switch (spec) {
        case clang::EST_None:
            return "";
        case clang::EST_DynamicNone:
            return "throw()";
        case clang::EST_Dynamic:
            return "throw(...)";
        case clang::EST_MSAny:
            return "__declspec(nothrow)";
        case clang::EST_NoThrow:
            return "__declspec(nothrow)";
        case clang::EST_BasicNoexcept:
            return "noexcept";
        case clang::EST_DependentNoexcept:
            return "noexcept(dependent)";
        case clang::EST_NoexceptFalse:
            return "noexcept(false)";
        case clang::EST_NoexceptTrue:
            return "noexcept(true)";
        case clang::EST_Unevaluated:
            return "unevaluated";
        case clang::EST_Uninstantiated:
            return "uninstantiated";
        case clang::EST_Unparsed:
            return "unparsed";
    }

    return "";
}

/// @brief Converts a Clang template specialization kind to the JSON spelling.
static std::string TemplateSpecializationKindToString(const clang::TemplateSpecializationKind kind) {
    switch (kind) {
        case clang::TSK_Undeclared:
            return "";
        case clang::TSK_ImplicitInstantiation:
            return "implicitInstantiation";
        case clang::TSK_ExplicitSpecialization:
            return "explicitSpecialization";
        case clang::TSK_ExplicitInstantiationDeclaration:
            return "explicitInstantiationDeclaration";
        case clang::TSK_ExplicitInstantiationDefinition:
            return "explicitInstantiationDefinition";
    }

    return "";
}

/// @brief Returns true when a template specialization was produced implicitly by Clang.
static inline bool IsImplicitInstantiation(const clang::TemplateSpecializationKind kind) {
    return kind == clang::TSK_ImplicitInstantiation;
}

/// @brief Determines whether a record declaration should be ignored during metadata extraction.
static bool ShouldSkipRecord(const clang::RecordDecl* decl) {
    if (!decl) {
        return true;
    }

    if (const auto* cxx_record = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
        if (cxx_record->isInjectedClassName()) {
            return true;
        }

        if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(cxx_record);
            specialization && IsImplicitInstantiation(specialization->getSpecializationKind())) {
            return true;
        }
    }

    return decl->isImplicit() || decl->isInvalidDecl();
}

/// @brief Determines whether a function declaration should be ignored during metadata extraction.
static bool ShouldSkipFunction(const clang::FunctionDecl* decl) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl()) {
        return true;
    }

    if (decl->isFunctionTemplateSpecialization() && IsImplicitInstantiation(decl->getTemplateSpecializationKind())) {
        return true;
    }

    return false;
}

/// @brief Returns true for implicit virtual destructors that should still be emitted as class methods.
static bool ShouldEmitImplicitVirtualDestructor(const clang::CXXMethodDecl* method) {
    const auto* destructor = llvm::dyn_cast_or_null<clang::CXXDestructorDecl>(method);
    return destructor && destructor->isImplicit() && destructor->isVirtual() && !destructor->isInvalidDecl();
}

/// @brief Returns true for record methods that should appear in JSON output.
static bool ShouldEmitRecordMethod(const clang::CXXMethodDecl* method) {
    return method && (ShouldEmitImplicitVirtualDestructor(method) || !ShouldSkipFunction(method));
}

/// @brief Determines whether a variable declaration should be ignored during metadata extraction.
static bool ShouldSkipVariable(const clang::VarDecl* decl) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl()) {
        return true;
    }

    if (IsImplicitInstantiation(decl->getTemplateSpecializationKind())) {
        return true;
    }

    return false;
}

/// @brief Determines whether an enum declaration should be ignored during metadata extraction.
static bool ShouldSkipEnum(const clang::EnumDecl* decl) {
    return !decl || decl->isImplicit() || decl->isInvalidDecl() ||
           IsImplicitInstantiation(decl->getTemplateSpecializationKind());
}

/// @brief Returns true for non-local declarations in the translation unit or namespace scopes.
static bool IsTopLevelNamedDecl(const clang::NamedDecl* decl) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl() || decl->getParentFunctionOrMethod()) {
        return false;
    }

    const auto* context = NonTransparentContext(decl->getDeclContext());
    return llvm::isa_and_nonnull<clang::TranslationUnitDecl>(context) ||
           llvm::isa_and_nonnull<clang::NamespaceDecl>(context);
}

/// @brief Returns true when a declaration location is invalid or comes from a system header.
static bool IsInSystemHeader(const clang::Decl* decl, const clang::ASTContext& ctx) {
    if (!decl) {
        return true;
    }

    const auto& source_manager = ctx.getSourceManager();
    const auto location = source_manager.getExpansionLoc(decl->getLocation());
    return location.isInvalid() || source_manager.isInSystemHeader(location);
}

/// @brief Resolves a source location to a stable source file path string.
static std::string FilePathForLocation(const clang::SourceManager& source_manager,
                                       const clang::SourceLocation location) {
    const auto expansion_location = source_manager.getExpansionLoc(location);
    if (expansion_location.isInvalid()) {
        return "";
    }

    if (const auto presumed = source_manager.getPresumedLoc(expansion_location); presumed.isValid()) {
        return UEMeta::JsonDetail::StablePathString(presumed.getFilename());
    }

    if (auto file_entry = source_manager.getFileEntryRefForID(source_manager.getFileID(expansion_location))) {
        return UEMeta::JsonDetail::StablePathString(file_entry->getName().str());
    }

    return "";
}

/// @brief Resolves a function source file, falling back to the containing record for implicit methods.
static std::string FilePathForFunction(const clang::FunctionDecl* decl, const clang::ASTContext& ctx) {
    auto file = FilePathForLocation(ctx.getSourceManager(), decl ? decl->getLocation() : clang::SourceLocation{});
    if (!file.empty()) {
        return file;
    }

    if (const auto* method = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(decl)) {
        if (const auto* parent = method->getParent()) {
            return FilePathForLocation(ctx.getSourceManager(), parent->getLocation());
        }
    }

    return "";
}

/// @brief Trims leading and trailing whitespace from formatted documentation text.
static std::string TrimDocumentation(std::string value) {
    const auto begin = std::ranges::find_if_not(value, [](const unsigned char character) {
        return std::isspace(character);
    });
    const auto end = std::ranges::find_if_not(value.rbegin(), value.rend(), [](const unsigned char character) {
        return std::isspace(character);
    }).base();

    if (begin >= end) {
        return "";
    }

    return {begin, end};
}

/// @brief Extracts formatted Doxygen text attached directly to one declaration.
static std::string DocumentationForSingleDecl(const clang::Decl* decl, const clang::ASTContext& ctx) {
    if (!decl) {
        return "";
    }

    const auto* comment = ctx.getRawCommentForAnyRedecl(decl);
    if (!comment || !comment->isDocumentation()) {
        return "";
    }

    return TrimDocumentation(comment->getFormattedText(ctx.getSourceManager(), ctx.getDiagnostics()));
}

/// @brief Extracts formatted Doxygen text from a declaration or its described template declaration.
static std::string DocumentationForDecl(const clang::Decl* decl, const clang::ASTContext& ctx) {
    if (auto documentation = DocumentationForSingleDecl(decl, ctx); !documentation.empty()) {
        return documentation;
    }

    if (const auto* cxx_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl)) {
        if (auto documentation = DocumentationForSingleDecl(cxx_record->getDescribedClassTemplate(), ctx);
            !documentation.empty()) {
            return documentation;
        }
    } else if (const auto* function = llvm::dyn_cast_or_null<clang::FunctionDecl>(decl)) {
        if (auto documentation = DocumentationForSingleDecl(function->getDescribedFunctionTemplate(), ctx);
            !documentation.empty()) {
            return documentation;
        }
    } else if (const auto* alias = llvm::dyn_cast_or_null<clang::TypeAliasDecl>(decl)) {
        if (auto documentation = DocumentationForSingleDecl(alias->getDescribedAliasTemplate(), ctx);
            !documentation.empty()) {
            return documentation;
        }
    } else if (const auto* variable = llvm::dyn_cast_or_null<clang::VarDecl>(decl)) {
        if (auto documentation = DocumentationForSingleDecl(variable->getDescribedVarTemplate(), ctx);
            !documentation.empty()) {
            return documentation;
        }
    }

    return "";
}

/// @brief Selects the declaration whose source range should be used for declaration hashing.
static const clang::Decl* SourceDeclForHash(const clang::NamedDecl* decl) {
    if (const auto* cxx_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl)) {
        if (const auto* templ = cxx_record->getDescribedClassTemplate()) {
            return templ;
        }
    } else if (const auto* function = llvm::dyn_cast_or_null<clang::FunctionDecl>(decl)) {
        if (const auto* templ = function->getDescribedFunctionTemplate()) {
            return templ;
        }
    } else if (const auto* alias = llvm::dyn_cast_or_null<clang::TypeAliasDecl>(decl)) {
        if (const auto* templ = alias->getDescribedAliasTemplate()) {
            return templ;
        }
    } else if (const auto* variable = llvm::dyn_cast_or_null<clang::VarDecl>(decl)) {
        if (const auto* templ = variable->getDescribedVarTemplate()) {
            return templ;
        }
    }

    return decl;
}

/// @brief Reads the single-file source text range used as the stable declaration hash input.
static std::optional<std::string> RawSourceTextForHash(const clang::NamedDecl* decl, const clang::ASTContext& ctx) {
    const auto* source_decl = SourceDeclForHash(decl);
    if (!source_decl) {
        return std::nullopt;
    }

    const auto range = source_decl->getSourceRange();
    if (range.isInvalid()) {
        return std::nullopt;
    }

    const auto& source_manager = ctx.getSourceManager();
    const auto begin = source_manager.getExpansionLoc(range.getBegin());
    const auto end = source_manager.getExpansionLoc(range.getEnd());
    if (begin.isInvalid() || end.isInvalid() ||
        source_manager.getFileID(begin) != source_manager.getFileID(end)) {
        return std::nullopt;
    }

    bool invalid = false;
    const auto text = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(begin, end),
        source_manager,
        ctx.getLangOpts(),
        &invalid);
    if (invalid) {
        return std::nullopt;
    }

    return text.str();
}

/// @brief Reads source text for a single-file source range.
static std::optional<std::string> SourceTextForRange(const clang::SourceRange range, const clang::ASTContext& ctx) {
    if (range.isInvalid()) {
        return std::nullopt;
    }

    const auto& source_manager = ctx.getSourceManager();
    const auto begin = source_manager.getExpansionLoc(range.getBegin());
    const auto end = source_manager.getExpansionLoc(range.getEnd());
    if (begin.isInvalid() || end.isInvalid() ||
        source_manager.getFileID(begin) != source_manager.getFileID(end)) {
        return std::nullopt;
    }

    bool invalid = false;
    const auto text = clang::Lexer::getSourceText(
        clang::CharSourceRange::getTokenRange(begin, end),
        source_manager,
        ctx.getLangOpts(),
        &invalid);
    if (invalid) {
        return std::nullopt;
    }

    return text.str();
}

/// @brief Computes the MD5 hash of a declaration's source text range when it is available.
static std::string DeclarationSourceHash(const clang::NamedDecl* decl, const clang::ASTContext& ctx) {
    const auto text = RawSourceTextForHash(decl, ctx);
    if (!text) {
        return "";
    }

    return Md5Hex(*text);
}

/// @brief Builds the lexical namespace and record scope for a named declaration.
static std::vector<std::string> BuildScope(const clang::NamedDecl* decl) {
    std::vector<std::string> scope;
    const auto* context = NonTransparentContext(decl ? decl->getDeclContext() : nullptr);

    while (context && !llvm::isa<clang::TranslationUnitDecl>(context)) {
        if (const auto* namespace_decl = llvm::dyn_cast<clang::NamespaceDecl>(context)) {
            if (!namespace_decl->isAnonymousNamespace() && namespace_decl->getDeclName()) {
                scope.push_back(namespace_decl->getNameAsString());
            }
        } else if (const auto* record_decl = llvm::dyn_cast<clang::RecordDecl>(context)) {
            if (record_decl->getDeclName()) {
                scope.push_back(record_decl->getNameAsString());
            }
        }

        context = NonTransparentContext(context->getParent());
    }

    std::ranges::reverse(scope);
    return scope;
}

/// @brief Populates fields shared by every top-level JSON declaration.
template <typename Declaration>
static void FillCommonDeclaration(Declaration& out, const clang::NamedDecl* decl, const clang::ASTContext& ctx) {
    out.common.name = decl->getNameAsString();
    out.common.qualified_name = QualifiedName(decl);
    out.common.file = FilePathForLocation(ctx.getSourceManager(), decl->getLocation());
    out.common.scope = BuildScope(decl);
    out.common.is_anonymous = out.common.name.empty();
    out.common.documentation = DocumentationForDecl(decl, ctx);
}

/// @brief Builds JSON metadata for one template parameter declaration.
static UEMeta::JsonTemplateParameter BuildTemplateParameter(const clang::NamedDecl* param, const clang::ASTContext& ctx);

/// @brief Builds JSON metadata for all parameters in a Clang template parameter list.
static std::vector<UEMeta::JsonTemplateParameter> BuildTemplateParameters(const clang::TemplateParameterList* parameters,
                                                                          const clang::ASTContext& ctx) {
    std::vector<UEMeta::JsonTemplateParameter> out;
    if (!parameters) {
        return out;
    }

    out.reserve(parameters->size());
    for (const auto* param : parameters->asArray()) {
        out.push_back(BuildTemplateParameter(param, ctx));
    }

    return out;
}

/// @brief Retrieves template parameters from a function template or its primary template when specializing one.
static std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForFunction(const clang::FunctionDecl* decl,
                                                                                const clang::ASTContext& ctx) {
    if (!decl) {
        return {};
    }

    if (const auto* templ = decl->getDescribedFunctionTemplate()) {
        return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
    }

    if (const auto* templ = decl->getPrimaryTemplate()) {
        return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
    }

    if (const auto* info = decl->getTemplateSpecializationInfo()) {
        if (const auto* templ = info->getTemplate()) {
            return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
        }
    }

    return {};
}

/// @brief Prints template arguments with the parser's normal C++ type policy.
static std::vector<std::string> PrintTemplateArguments(llvm::ArrayRef<clang::TemplateArgument> args,
                                                       const clang::ASTContext& ctx) {
    std::vector<std::string> out;
    const auto policy = MakePrintingPolicy(ctx);
    out.reserve(args.size());
    for (const auto& arg : args) {
        std::string value;
        llvm::raw_string_ostream stream{value};
        arg.print(policy, stream, true);
        out.push_back(stream.str());
    }

    return out;
}

/// @brief Prints source-spelled template arguments when the AST retained their written locations.
static std::vector<std::string> PrintTemplateArgumentsAsWritten(const clang::ASTTemplateArgumentListInfo* args,
                                                                const clang::ASTContext& ctx) {
    std::vector<std::string> out;
    if (!args) {
        return out;
    }

    out.reserve(args->getNumTemplateArgs());
    for (const auto& arg : args->arguments()) {
        if (auto text = SourceTextForRange(arg.getSourceRange(), ctx); text && !text->empty()) {
            out.push_back(*text);
            continue;
        }

        auto printed = PrintTemplateArguments(llvm::ArrayRef{&arg.getArgument(), 1}, ctx);
        if (!printed.empty()) {
            out.push_back(std::move(printed.front()));
        }
    }

    return out;
}

/// @brief Prints the deduced or explicitly written template arguments for a function specialization.
static std::vector<std::string> TemplateArgumentsForFunction(const clang::FunctionDecl* decl,
                                                             const clang::ASTContext& ctx) {
    if (decl) {
        auto written = PrintTemplateArgumentsAsWritten(decl->getTemplateSpecializationArgsAsWritten(), ctx);
        if (!written.empty()) {
            return written;
        }
    }

    const auto* args = decl ? decl->getTemplateSpecializationArgs() : nullptr;
    return args ? PrintTemplateArguments(args->asArray(), ctx) : std::vector<std::string>{};
}

/// @brief Builds JSON metadata for a type, non-type, or template-template parameter.
static UEMeta::JsonTemplateParameter BuildTemplateParameter(const clang::NamedDecl* param, const clang::ASTContext& ctx) {
    UEMeta::JsonTemplateParameter out;
    if (!param) {
        return out;
    }

    out.name = param->getNameAsString();
    out.is_parameter_pack = param->isParameterPack();
    out.documentation = DocumentationForDecl(param, ctx);

    if (const auto* type_param = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
        out.kind = type_param->wasDeclaredWithTypename() ? "typename" : "class";
    } else if (const auto* non_type_param = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param)) {
        out.kind = "nonType";
        out.type = PrintType(ctx, non_type_param->getType(), out.name);
    } else if (const auto* template_template_param = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
        out.kind = template_template_param->wasDeclaredWithTypename() ? "typenameTemplate" : "classTemplate";
        out.parameters = BuildTemplateParameters(template_template_param->getTemplateParameters(), ctx);
    } else {
        out.kind = param->getDeclKindName();
    }

    return out;
}

/// @brief Retrieves template parameters declared by a record template declaration or specialization.
static std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForRecord(const clang::RecordDecl* decl,
                                                                              const clang::ASTContext& ctx) {
    if (const auto* cxx_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl)) {
        if (const auto* partial = llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(cxx_record)) {
            return BuildTemplateParameters(partial->getTemplateParameters(), ctx);
        }

        if (const auto* templ = cxx_record->getDescribedClassTemplate()) {
            return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
        }

        if (const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(cxx_record)) {
            if (const auto* templ = specialization->getSpecializedTemplate()) {
                return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
            }
        }
    }

    return {};
}

/// @brief Copies record template parameters and specialization metadata into a record declaration payload.
template <typename RecordDeclaration>
static void FillRecordTemplateDetails(RecordDeclaration& out, const clang::RecordDecl* decl, const clang::ASTContext& ctx) {
    out.template_parameters = TemplateParametersForRecord(decl, ctx);

    const auto* cxx_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl);
    if (!cxx_record) {
        return;
    }

    if (const auto* partial = llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(cxx_record)) {
        out.is_template_specialization = true;
        out.template_specialization_kind = "partialSpecialization";
        out.template_arguments = PrintTemplateArgumentsAsWritten(partial->getTemplateArgsAsWritten(), ctx);
        if (out.template_arguments.empty()) {
            out.template_arguments = PrintTemplateArguments(partial->getTemplateArgs().asArray(), ctx);
        }
        if (const auto* templ = partial->getSpecializedTemplate()) {
            out.primary_template_qualified_name = QualifiedName(templ->getTemplatedDecl());
        }
        return;
    }

    const auto* specialization = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(cxx_record);
    if (!specialization) {
        return;
    }

    const auto specialization_kind = specialization->getSpecializationKind();
    if (specialization_kind == clang::TSK_Undeclared || specialization_kind == clang::TSK_ImplicitInstantiation) {
        return;
    }

    out.is_template_specialization = true;
    out.template_specialization_kind = TemplateSpecializationKindToString(specialization_kind);
    out.template_arguments = PrintTemplateArgumentsAsWritten(specialization->getTemplateArgsAsWritten(), ctx);
    if (out.template_arguments.empty()) {
        out.template_arguments = PrintTemplateArguments(specialization->getTemplateArgs().asArray(), ctx);
    }
    if (const auto* templ = specialization->getSpecializedTemplate()) {
        out.primary_template_qualified_name = QualifiedName(templ->getTemplatedDecl());
    }
}

/// @brief Retrieves template parameters declared by a variable template declaration or specialization.
static std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForVariable(const clang::VarDecl* decl,
                                                                                const clang::ASTContext& ctx) {
    if (!decl) {
        return {};
    }

    if (const auto* partial = llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(decl)) {
        return BuildTemplateParameters(partial->getTemplateParameters(), ctx);
    }

    if (const auto* templ = decl->getDescribedVarTemplate()) {
        return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
    }

    if (const auto* specialization = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl)) {
        if (const auto* templ = specialization->getSpecializedTemplate()) {
            return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
        }
    }

    return {};
}

/// @brief Copies variable template parameters and specialization metadata into a variable payload.
template <typename VariableDeclaration>
static void FillVariableTemplateDetails(VariableDeclaration& out, const clang::VarDecl* decl, const clang::ASTContext& ctx) {
    out.template_parameters = TemplateParametersForVariable(decl, ctx);

    if (const auto* partial = llvm::dyn_cast_or_null<clang::VarTemplatePartialSpecializationDecl>(decl)) {
        out.is_template_specialization = true;
        out.template_specialization_kind = "partialSpecialization";
        out.template_arguments = PrintTemplateArgumentsAsWritten(partial->getTemplateArgsAsWritten(), ctx);
        if (out.template_arguments.empty()) {
            out.template_arguments = PrintTemplateArguments(partial->getTemplateArgs().asArray(), ctx);
        }
        if (const auto* templ = partial->getSpecializedTemplate()) {
            out.primary_template_qualified_name = QualifiedName(templ->getTemplatedDecl());
        }
        return;
    }

    const auto* specialization = llvm::dyn_cast_or_null<clang::VarTemplateSpecializationDecl>(decl);
    if (!specialization) {
        return;
    }

    const auto specialization_kind = specialization->getSpecializationKind();
    if (specialization_kind == clang::TSK_Undeclared || specialization_kind == clang::TSK_ImplicitInstantiation) {
        return;
    }

    out.is_template_specialization = true;
    out.template_specialization_kind = TemplateSpecializationKindToString(specialization_kind);
    out.template_arguments = PrintTemplateArgumentsAsWritten(specialization->getTemplateArgsAsWritten(), ctx);
    if (out.template_arguments.empty()) {
        out.template_arguments = PrintTemplateArguments(specialization->getTemplateArgs().asArray(), ctx);
    }
    if (const auto* templ = specialization->getSpecializedTemplate()) {
        out.primary_template_qualified_name = QualifiedName(templ->getTemplatedDecl());
    }
}

/// @brief Converts an enum constant's evaluated value to a decimal string.
static inline std::string EnumValueToString(const clang::EnumConstantDecl* enumerator) {
    llvm::SmallString<32> value;
    enumerator->getInitVal().toString(value, 10);
    return value.str().str();
}

/// @brief Determines the emitted record kind string for class, struct, and union declarations.
static std::string RecordKind(const clang::RecordDecl* decl) {
    if (decl->isUnion()) {
        return "union";
    }

    if (decl->isClass()) {
        return "class";
    }

    return "struct";
}

/// @brief Builds ABI vtable slot metadata for a virtual method when Clang exposes layout information.
static std::optional<UEMeta::JsonVTableIndex> BuildVTableIndex(const clang::CXXMethodDecl* method, clang::ASTContext& ctx) {
    if (!method || !method->isVirtual() || !clang::VTableContextBase::hasVtableSlot(method)) {
        return std::nullopt;
    }

    const auto* parent = method->getParent();
    if (!parent || !parent->hasDefinition() || parent->isDependentContext() || !parent->isCompleteDefinition()) {
        return std::nullopt;
    }

    const auto* canonical = method->getCanonicalDecl();
    clang::GlobalDecl global_decl;
    if (const auto* destructor = llvm::dyn_cast<clang::CXXDestructorDecl>(canonical)) {
        global_decl = clang::GlobalDecl{destructor, clang::Dtor_Deleting};
    } else {
        global_decl = clang::GlobalDecl{canonical};
    }

    auto* vtable_context = ctx.getVTableContext();
    if (!vtable_context) {
        return std::nullopt;
    }

    if (auto* microsoft_context = llvm::dyn_cast<clang::MicrosoftVTableContext>(vtable_context)) {
        const auto location = microsoft_context->getMethodVFTableLocation(global_decl);
        return UEMeta::JsonVTableIndex{
            .index = location.Index,
            .offset = location.VFPtrOffset.getQuantity()
        };
    }

    if (auto* itanium_context = llvm::dyn_cast<clang::ItaniumVTableContext>(vtable_context)) {
        return UEMeta::JsonVTableIndex{
            .index = itanium_context->getMethodVTableIndex(global_decl),
            .offset = 0
        };
    }

    return std::nullopt;
}

/// @brief Builds JSON metadata for a free function, method, constructor, destructor, or conversion function.
static UEMeta::JsonFunction BuildFunction(const clang::FunctionDecl* decl, clang::ASTContext& ctx) {
    UEMeta::JsonFunction out;
    out.function_kind = "function";
    out.name = decl->getNameAsString();
    out.qualified_name = QualifiedName(decl);
    out.file = FilePathForFunction(decl, ctx);
    out.scope = BuildScope(decl);
    out.documentation = DocumentationForDecl(decl, ctx);
    out.type = PrintType(ctx, decl->getType());
    out.return_type = PrintType(ctx, decl->getReturnType());
    out.storage_class = StorageClassToString(decl->getStorageClass());
    out.is_constexpr = decl->isConstexpr();
    out.is_consteval = decl->isConsteval();
    out.is_inline = decl->isInlined() || decl->isInlineSpecified();
    out.is_deleted = decl->isDeleted();
    out.is_defaulted = decl->isDefaulted();
    out.exception_spec = ExceptionSpecToString(decl->getExceptionSpecType());

    out.template_parameters = TemplateParametersForFunction(decl, ctx);
    if (const auto specialization_kind = decl->getTemplateSpecializationKind();
        specialization_kind != clang::TSK_Undeclared && specialization_kind != clang::TSK_ImplicitInstantiation) {
        out.is_template_specialization = true;
        out.template_specialization_kind = TemplateSpecializationKindToString(specialization_kind);
        out.template_arguments = TemplateArgumentsForFunction(decl, ctx);
        if (const auto* primary_template = decl->getPrimaryTemplate()) {
            out.primary_template_qualified_name = QualifiedName(primary_template->getTemplatedDecl());
        } else if (const auto* info = decl->getTemplateSpecializationInfo(); info && info->getTemplate()) {
            out.primary_template_qualified_name = QualifiedName(info->getTemplate()->getTemplatedDecl());
        }
    }

    if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
        out.function_kind = "method";
        out.access = AccessToString(method->getAccess());
        out.is_const = method->isConst();
        out.is_volatile = method->isVolatile();
        out.is_static = method->isStatic();
        out.is_virtual = method->isVirtual();
        out.is_pure = method->isPureVirtual();
        out.ref_qualifier = RefQualifierToString(method->getRefQualifier());
        out.vtable_index = BuildVTableIndex(method, ctx);

        if (const auto* constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(method)) {
            out.function_kind = "constructor";
            out.return_type.clear();
            out.is_explicit = constructor->isExplicit();
        } else if (llvm::isa<clang::CXXDestructorDecl>(method)) {
            out.function_kind = "destructor";
            out.return_type.clear();
        } else if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(method)) {
            out.function_kind = "conversion";
            out.return_type = PrintType(ctx, conversion->getConversionType());
            out.is_explicit = conversion->isExplicit();
        }
    }

    out.parameters.reserve(decl->getNumParams());
    for (const auto* param : decl->parameters()) {
        UEMeta::JsonParameter json_param;
        json_param.name = param->getNameAsString();
        json_param.type = PrintType(ctx, param->getType());
        json_param.declaration = PrintType(ctx, param->getType(), json_param.name);
        json_param.documentation = DocumentationForDecl(param, ctx);
        out.parameters.push_back(std::move(json_param));
    }

    return out;
}

/// @brief Builds JSON metadata for a global variable or static data member.
static UEMeta::JsonVariable BuildVariable(const clang::VarDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonVariable out;
    out.name = decl->getNameAsString();
    out.qualified_name = QualifiedName(decl);
    out.file = FilePathForLocation(ctx.getSourceManager(), decl->getLocation());
    out.scope = BuildScope(decl);
    out.documentation = DocumentationForDecl(decl, ctx);
    out.type = PrintType(ctx, decl->getType());
    out.declaration = PrintType(ctx, decl->getType(), out.name);
    out.access = AccessToString(decl->getAccess());
    out.storage_class = StorageClassToString(decl->getStorageClass());
    FillVariableTemplateDetails(out, decl, ctx);
    out.is_constexpr = decl->isConstexpr();
    out.is_inline = decl->isInline();
    out.is_static_data_member = decl->isStaticDataMember();
    out.is_thread_local = decl->getTLSKind() != clang::VarDecl::TLS_None;
    return out;
}

/// @brief Copies function-specific fields into a flattened function declaration payload.
template <typename FunctionDetails>
static void FillFunctionDetails(FunctionDetails& out, UEMeta::JsonFunction function) {
    out.function_kind = std::move(function.function_kind);
    out.type = std::move(function.type);
    out.return_type = std::move(function.return_type);
    out.access = std::move(function.access);
    out.storage_class = std::move(function.storage_class);
    out.ref_qualifier = std::move(function.ref_qualifier);
    out.exception_spec = std::move(function.exception_spec);
    out.is_const = function.is_const;
    out.is_volatile = function.is_volatile;
    out.is_static = function.is_static;
    out.is_virtual = function.is_virtual;
    out.is_pure = function.is_pure;
    out.is_constexpr = function.is_constexpr;
    out.is_consteval = function.is_consteval;
    out.is_inline = function.is_inline;
    out.is_deleted = function.is_deleted;
    out.is_defaulted = function.is_defaulted;
    out.is_explicit = function.is_explicit;
    out.vtable_index = std::move(function.vtable_index);
    out.template_parameters = std::move(function.template_parameters);
    out.is_template_specialization = function.is_template_specialization;
    out.template_specialization_kind = std::move(function.template_specialization_kind);
    out.primary_template_qualified_name = std::move(function.primary_template_qualified_name);
    out.template_arguments = std::move(function.template_arguments);
    out.parameters = std::move(function.parameters);
}

/// @brief Copies variable-specific fields into a flattened variable declaration payload.
template <typename VariableDetails>
static void FillVariableDetails(VariableDetails& out, UEMeta::JsonVariable variable) {
    out.type = std::move(variable.type);
    out.declaration = std::move(variable.declaration);
    out.access = std::move(variable.access);
    out.storage_class = std::move(variable.storage_class);
    out.template_parameters = std::move(variable.template_parameters);
    out.is_template_specialization = variable.is_template_specialization;
    out.template_specialization_kind = std::move(variable.template_specialization_kind);
    out.primary_template_qualified_name = std::move(variable.primary_template_qualified_name);
    out.template_arguments = std::move(variable.template_arguments);
    out.is_constexpr = variable.is_constexpr;
    out.is_inline = variable.is_inline;
    out.is_static_data_member = variable.is_static_data_member;
    out.is_thread_local = variable.is_thread_local;
}

/// @brief Builds JSON metadata for a complete enum declaration.
static UEMeta::JsonEnumDeclaration BuildEnumDeclaration(const clang::EnumDecl* input, const clang::ASTContext& ctx) {
    const auto* decl = input;

    UEMeta::JsonEnumDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    out.is_scoped = decl->isScoped();
    out.scoped_kind = decl->isScoped() ? (decl->isScopedUsingClassTag() ? "class" : "struct") : "";
    out.underlying_type = decl->getIntegerType().isNull() ? "" : PrintType(ctx, decl->getIntegerType());

    for (const auto* enumerator : decl->enumerators()) {
        out.enumerators.push_back(UEMeta::JsonEnumerator{
            .name = enumerator->getNameAsString(),
            .value = EnumValueToString(enumerator),
            .file = FilePathForLocation(ctx.getSourceManager(), enumerator->getLocation()),
            .scope = BuildScope(enumerator),
            .documentation = DocumentationForDecl(enumerator, ctx)
        });
    }

    return out;
}

/// @brief Builds JSON metadata for an enum forward declaration.
static UEMeta::JsonForwardDeclaration BuildEnumForwardDeclaration(const clang::EnumDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonForwardDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    out.forward_declaration_kind = "enum";
    out.is_scoped = decl->isScoped();
    out.scoped_kind = decl->isScoped() ? (decl->isScopedUsingClassTag() ? "class" : "struct") : "";
    out.underlying_type = decl->getIntegerType().isNull() ? "" : PrintType(ctx, decl->getIntegerType());
    return out;
}

/// @brief Builds JSON metadata for a type alias declaration.
static UEMeta::JsonAliasDeclaration BuildAliasDeclaration(const clang::TypeAliasDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonAliasDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    if (const auto* templ = decl->getDescribedAliasTemplate()) {
        out.template_parameters = BuildTemplateParameters(templ->getTemplateParameters(), ctx);
    }
    out.aliased_type = PrintType(ctx, decl->getUnderlyingType());
    return out;
}

/// @brief Builds top-level JSON metadata for a global variable declaration.
static UEMeta::JsonGlobalVariableDeclaration BuildGlobalVariableDeclaration(const clang::VarDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonGlobalVariableDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    FillVariableDetails(out, BuildVariable(decl, ctx));
    return out;
}

/// @brief Builds top-level JSON metadata for a free function declaration.
static UEMeta::JsonFreeFunctionDeclaration BuildFreeFunctionDeclaration(const clang::FunctionDecl* decl, clang::ASTContext& ctx) {
    UEMeta::JsonFreeFunctionDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    FillFunctionDetails(out, BuildFunction(decl, ctx));
    return out;
}

/// @brief Appends one concrete declaration and records its encounter-order slot.
template <typename Declarations, typename Declaration>
static void AppendOrderedDeclaration(Declarations& declarations,
                                     std::vector<Declaration> Declarations::*bucket,
                                     const UEMeta::JsonDeclarationBucket kind,
                                     Declaration declaration) {
    auto& values = declarations.*bucket;
    declarations.declaration_order.push_back(UEMeta::JsonDeclarationSlot{
        .bucket = kind,
        .index = values.size()
    });
    values.push_back(std::move(declaration));
}

/// @brief Adds a nested record declaration to a containing record's nested declaration list.
static void AppendNestedRecord(const clang::RecordDecl* decl, clang::ASTContext& ctx,
                               llvm::DenseSet<const clang::Decl*>& seen,
                               llvm::DenseSet<const clang::Decl*>& seen_forwards,
                               UEMeta::JsonNestedDeclarations& nested);

/// @brief Adds a nested enum declaration to a containing record's nested declaration list.
static void AppendNestedEnum(const clang::EnumDecl* decl, const clang::ASTContext& ctx,
                             llvm::DenseSet<const clang::Decl*>& seen,
                             llvm::DenseSet<const clang::Decl*>& seen_forwards,
                             UEMeta::JsonNestedDeclarations& nested) {
    if (ShouldSkipEnum(decl)) {
        return;
    }

    const auto* target = decl;
    if (IsInSystemHeader(target, ctx)) {
        return;
    }

    if (target->isThisDeclarationADefinition()) {
        const auto* key = target->getCanonicalDecl();
        if (!seen.insert(key).second) {
            return;
        }
        AppendOrderedDeclaration(nested, &UEMeta::JsonNestedDeclarations::enums,
                                 UEMeta::JsonDeclarationBucket::Enum, BuildEnumDeclaration(target, ctx));
    } else {
        if (!seen_forwards.insert(target).second) {
            return;
        }
        AppendOrderedDeclaration(nested, &UEMeta::JsonNestedDeclarations::forward_declarations,
                                 UEMeta::JsonDeclarationBucket::ForwardDeclaration, BuildEnumForwardDeclaration(target, ctx));
    }
}

/// @brief Adds a nested type alias declaration to a containing record's nested declaration list.
static void AppendNestedAlias(const clang::TypeAliasDecl* decl, const clang::ASTContext& ctx,
                              llvm::DenseSet<const clang::Decl*>& seen,
                              UEMeta::JsonNestedDeclarations& nested) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl() || IsInSystemHeader(decl, ctx)) {
        return;
    }

    const auto* key = decl->getCanonicalDecl();
    if (!seen.insert(key).second) {
        return;
    }

    AppendOrderedDeclaration(nested, &UEMeta::JsonNestedDeclarations::aliases,
                             UEMeta::JsonDeclarationBucket::Alias, BuildAliasDeclaration(decl, ctx));
}

/// @brief Attempts to retrieve record layout and avoids querying layouts Clang cannot provide.
static const clang::ASTRecordLayout* TryGetRecordLayout(const clang::RecordDecl* decl, const clang::ASTContext& ctx) {
    if (!decl->isCompleteDefinition() || decl->isDependentContext()) {
        return nullptr;
    }

    if (const auto* cxx_record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
        cxx_record && (!cxx_record->hasDefinition() || cxx_record->hasAnyDependentBases() || !cxx_record->isCompleteDefinition())) {
        return nullptr;
    }

    return &ctx.getASTRecordLayout(decl);
}

/// @brief Builds JSON metadata for a record forward declaration.
static UEMeta::JsonForwardDeclaration BuildRecordForwardDeclaration(const clang::RecordDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonForwardDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    FillRecordTemplateDetails(out, decl, ctx);
    out.forward_declaration_kind = RecordKind(decl);
    return out;
}

/// @brief Builds JSON metadata for a complete class or struct declaration.
template <typename RecordDeclaration>
static RecordDeclaration BuildClassLikeDeclaration(const clang::RecordDecl* input, clang::ASTContext& ctx,
                                                   llvm::DenseSet<const clang::Decl*>& seen,
                                                   llvm::DenseSet<const clang::Decl*>& seen_forwards) {
    const auto* decl = input;

    RecordDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    FillRecordTemplateDetails(out, decl, ctx);
    out.is_complete_definition = decl->isCompleteDefinition();

    const auto layout = TryGetRecordLayout(decl, ctx);
    if (layout) {
        out.size_bytes = static_cast<std::uint64_t>(layout->getSize().getQuantity());
        out.align_bytes = static_cast<std::uint64_t>(layout->getAlignment().getQuantity());
    }

    if (const auto* cxx_record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
        cxx_record && cxx_record->hasDefinition()) {
        for (const auto& base : cxx_record->bases()) {
            UEMeta::JsonBase json_base;
            json_base.type = PrintType(ctx, base.getType());
            json_base.access = AccessToString(base.getAccessSpecifier());
            json_base.is_virtual = base.isVirtual();

            if (const auto* base_record = base.getType()->getAsCXXRecordDecl()) {
                json_base.qualified_name = QualifiedName(base_record);

                if (layout && !base_record->isDependentContext()) {
                    const auto* base_definition = base_record->getDefinition();
                    if (base_definition) {
                        json_base.offset = base.isVirtual()
                            ? layout->getVBaseClassOffset(base_definition).getQuantity()
                            : layout->getBaseClassOffset(base_definition).getQuantity();
                    }
                }
            }

            out.bases.push_back(std::move(json_base));
        }
    }

    for (const auto* field : decl->fields()) {
        if (!field || field->isImplicit() || field->isInvalidDecl()) {
            continue;
        }

        UEMeta::JsonField json_field;
        json_field.name = field->getNameAsString();
        json_field.file = FilePathForLocation(ctx.getSourceManager(), field->getLocation());
        json_field.scope = BuildScope(field);
        json_field.documentation = DocumentationForDecl(field, ctx);
        json_field.type = PrintType(ctx, field->getType());
        json_field.declaration = PrintType(ctx, field->getType(), json_field.name);
        json_field.access = AccessToString(field->getAccess());
        json_field.is_mutable = field->isMutable();
        json_field.is_bitfield = field->isBitField();

        if (json_field.is_bitfield && field->hasConstantIntegerBitWidth()) {
            json_field.bit_width = field->getBitWidthValue();
        }

        if (layout && field->getFieldIndex() < layout->getFieldCount()) {
            json_field.offset_bits = layout->getFieldOffset(field->getFieldIndex());
        }

        out.fields.push_back(std::move(json_field));
    }

    llvm::DenseSet<const clang::Decl*> methods_seen;
    llvm::DenseSet<const clang::Decl*> statics_seen;

    for (const auto* member : decl->decls()) {
        if (const auto* function_template = llvm::dyn_cast<clang::FunctionTemplateDecl>(member)) {
            if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(function_template->getTemplatedDecl());
                ShouldEmitRecordMethod(method) && methods_seen.insert(method->getCanonicalDecl()).second) {
                out.methods.push_back(BuildFunction(method, ctx));
            }
            continue;
        }

        if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(member)) {
            if (ShouldEmitRecordMethod(method) && methods_seen.insert(method->getCanonicalDecl()).second) {
                out.methods.push_back(BuildFunction(method, ctx));
            }
            continue;
        }

        if (const auto* var_template = llvm::dyn_cast<clang::VarTemplateDecl>(member)) {
            const auto* var = var_template->getTemplatedDecl();
            if (var && var->isStaticDataMember() && !ShouldSkipVariable(var) &&
                statics_seen.insert(var->getCanonicalDecl()).second) {
                out.static_variables.push_back(BuildVariable(var, ctx));
            }
            continue;
        }

        if (const auto* var = llvm::dyn_cast<clang::VarDecl>(member)) {
            if (var->isStaticDataMember() && !ShouldSkipVariable(var) &&
                statics_seen.insert(var->getCanonicalDecl()).second) {
                out.static_variables.push_back(BuildVariable(var, ctx));
            }
            continue;
        }

        if (const auto* class_template = llvm::dyn_cast<clang::ClassTemplateDecl>(member)) {
            AppendNestedRecord(class_template->getTemplatedDecl(), ctx, seen, seen_forwards, out.nested);
            continue;
        }

        if (const auto* nested_record = llvm::dyn_cast<clang::RecordDecl>(member)) {
            AppendNestedRecord(nested_record, ctx, seen, seen_forwards, out.nested);
            continue;
        }

        if (const auto* nested_enum = llvm::dyn_cast<clang::EnumDecl>(member)) {
            AppendNestedEnum(nested_enum, ctx, seen, seen_forwards, out.nested);
            continue;
        }

        if (const auto* alias_template = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(member)) {
            AppendNestedAlias(alias_template->getTemplatedDecl(), ctx, seen, out.nested);
            continue;
        }

        if (const auto* alias = llvm::dyn_cast<clang::TypeAliasDecl>(member)) {
            AppendNestedAlias(alias, ctx, seen, out.nested);
        }
    }

    if (const auto* cxx_record = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
        if (const auto* destructor = cxx_record->getDestructor();
            ShouldEmitImplicitVirtualDestructor(destructor) &&
            methods_seen.insert(destructor->getCanonicalDecl()).second) {
            out.methods.push_back(BuildFunction(destructor, ctx));
        }
    }

    return out;
}

/// @brief Builds JSON metadata for a complete union declaration.
static UEMeta::JsonUnionDeclaration BuildUnionDeclaration(const clang::RecordDecl* input, clang::ASTContext& ctx,
                                                          llvm::DenseSet<const clang::Decl*>& seen,
                                                          llvm::DenseSet<const clang::Decl*>& seen_forwards) {
    const auto* decl = input;

    UEMeta::JsonUnionDeclaration out;
    FillCommonDeclaration(out, decl, ctx);
    FillRecordTemplateDetails(out, decl, ctx);
    out.is_complete_definition = decl->isCompleteDefinition();

    const auto layout = TryGetRecordLayout(decl, ctx);
    if (layout) {
        out.size_bytes = static_cast<std::uint64_t>(layout->getSize().getQuantity());
        out.align_bytes = static_cast<std::uint64_t>(layout->getAlignment().getQuantity());
    }

    for (const auto* field : decl->fields()) {
        if (!field || field->isImplicit() || field->isInvalidDecl()) {
            continue;
        }

        UEMeta::JsonField json_field;
        json_field.name = field->getNameAsString();
        json_field.file = FilePathForLocation(ctx.getSourceManager(), field->getLocation());
        json_field.scope = BuildScope(field);
        json_field.documentation = DocumentationForDecl(field, ctx);
        json_field.type = PrintType(ctx, field->getType());
        json_field.declaration = PrintType(ctx, field->getType(), json_field.name);
        json_field.access = AccessToString(field->getAccess());
        json_field.is_mutable = field->isMutable();
        json_field.is_bitfield = field->isBitField();

        if (json_field.is_bitfield && field->hasConstantIntegerBitWidth()) {
            json_field.bit_width = field->getBitWidthValue();
        }

        if (layout && field->getFieldIndex() < layout->getFieldCount()) {
            json_field.offset_bits = layout->getFieldOffset(field->getFieldIndex());
        }

        out.fields.push_back(std::move(json_field));
    }

    for (const auto* member : decl->decls()) {
        if (const auto* class_template = llvm::dyn_cast<clang::ClassTemplateDecl>(member)) {
            AppendNestedRecord(class_template->getTemplatedDecl(), ctx, seen, seen_forwards, out.nested);
            continue;
        }

        if (const auto* nested_record = llvm::dyn_cast<clang::RecordDecl>(member)) {
            AppendNestedRecord(nested_record, ctx, seen, seen_forwards, out.nested);
            continue;
        }

        if (const auto* nested_enum = llvm::dyn_cast<clang::EnumDecl>(member)) {
            AppendNestedEnum(nested_enum, ctx, seen, seen_forwards, out.nested);
            continue;
        }

        if (const auto* alias_template = llvm::dyn_cast<clang::TypeAliasTemplateDecl>(member)) {
            AppendNestedAlias(alias_template->getTemplatedDecl(), ctx, seen, out.nested);
            continue;
        }

        if (const auto* alias = llvm::dyn_cast<clang::TypeAliasDecl>(member)) {
            AppendNestedAlias(alias, ctx, seen, out.nested);
        }
    }

    return out;
}

/// @brief Adds a nested record definition or forward declaration to a containing record.
static void AppendNestedRecord(const clang::RecordDecl* decl, clang::ASTContext& ctx,
                               llvm::DenseSet<const clang::Decl*>& seen,
                               llvm::DenseSet<const clang::Decl*>& seen_forwards,
                               UEMeta::JsonNestedDeclarations& nested) {
    if (ShouldSkipRecord(decl)) {
        return;
    }

    const auto* target = decl;
    if (IsInSystemHeader(target, ctx)) {
        return;
    }

    if (target->isThisDeclarationADefinition()) {
        const auto* key = target->getCanonicalDecl();
        if (!seen.insert(key).second) {
            return;
        }
        if (target->isUnion()) {
            AppendOrderedDeclaration(nested, &UEMeta::JsonNestedDeclarations::unions,
                                     UEMeta::JsonDeclarationBucket::Union,
                                     BuildUnionDeclaration(target, ctx, seen, seen_forwards));
        } else if (target->isClass()) {
            AppendOrderedDeclaration(nested, &UEMeta::JsonNestedDeclarations::classes,
                                     UEMeta::JsonDeclarationBucket::Class,
                                     BuildClassLikeDeclaration<UEMeta::JsonClassDeclaration>(target, ctx, seen, seen_forwards));
        } else {
            AppendOrderedDeclaration(nested, &UEMeta::JsonNestedDeclarations::structs,
                                     UEMeta::JsonDeclarationBucket::Struct,
                                     BuildClassLikeDeclaration<UEMeta::JsonStructDeclaration>(target, ctx, seen, seen_forwards));
        }
    } else {
        if (!seen_forwards.insert(target).second) {
            return;
        }
        AppendOrderedDeclaration(nested, &UEMeta::JsonNestedDeclarations::forward_declarations,
                                 UEMeta::JsonDeclarationBucket::ForwardDeclaration,
                                 BuildRecordForwardDeclaration(target, ctx));
    }
}

/// @brief Converts a header path into a bounded filesystem-safe stem.
static std::string SanitizeFilePathStem(const std::string_view value) {
    std::string out;
    out.reserve(value.size());

    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '/' || character == '\\') {
            if (!out.empty() && out.back() != '.') {
                out.push_back('.');
            }
        } else if (std::isalnum(byte) || character == '_' || character == '-' || character == '.') {
            out.push_back(character);
        } else if (out.empty() || out.back() != '_') {
            out.push_back('_');
        }
    }

    while (!out.empty() && out.front() == '_') {
        out.erase(out.begin());
    }

    while (!out.empty() && (out.back() == '_' || out.back() == '.')) {
        out.pop_back();
    }

    if (out.empty()) {
        out = "uemeta";
    }

    if (out.size() > 80) {
        out.resize(80);
        while (!out.empty() && out.back() == '_') {
            out.pop_back();
        }
    }

    return out;
}

/// @brief Builds an output JSON path from a header path and stable hash suffix.
static UEMeta::StablePath OutputFileForHash(const std::string& file,
                                            const std::string& hash,
                                            const std::string_view extension = "json") {
    auto filename_path = UEMeta::JsonDetail::ScrubFilePath(file);
    if (filename_path.empty()) {
        filename_path = file;
    }

    const auto stem = SanitizeFilePathStem(filename_path);
    const auto suffix = hash.empty() ? Md5Hex(file) : hash;
    return UEMeta::StablePath{
        UEMeta::Config::GetConfig().OutPath().UnderlyingPath() /
        fmtquill::format("{}-{}.{}", stem, suffix, extension)};
}

/// @brief Applies configured header whitelist and blacklist filters to a path string.
static bool StringPassesHeaderFilters(const std::string& str);

/// @brief Returns the grouping key used for file-based output, including a fallback for unknown files.
static inline std::string FileGroupKey(const std::string& file) {
    return file.empty() ? "unknown" : file;
}

/// @brief Clears every top-level declaration bucket.
static void ClearDeclarations(UEMeta::JsonTopLevelDeclarations& declarations) {
    declarations.declaration_order.clear();
    declarations.classes.clear();
    declarations.structs.clear();
    declarations.unions.clear();
    declarations.enums.clear();
    declarations.forward_declarations.clear();
    declarations.aliases.clear();
    declarations.free_functions.clear();
    declarations.globals.clear();
}

/// @brief Counts declarations across every top-level bucket.
static std::size_t DeclarationCount(const UEMeta::JsonTopLevelDeclarations& declarations) {
    return declarations.declaration_order.size();
}

/// @brief Appends one ordered top-level declaration when it passes the active header filters.
template <typename Declaration>
static void AppendIfHeaderAllowed(UEMeta::JsonTopLevelDeclarations& out,
                                  std::vector<Declaration> UEMeta::JsonTopLevelDeclarations::*bucket,
                                  const UEMeta::JsonDeclarationBucket kind,
                                  const Declaration& declaration) {
    if (!StringPassesHeaderFilters(declaration.common.file)) {
        return;
    }

    AppendOrderedDeclaration(out, bucket, kind, declaration);
}

/// @brief Applies configured header filters to every declaration bucket.
static void FilterDeclarations(UEMeta::JsonTopLevelDeclarations& declarations) {
    UEMeta::JsonTopLevelDeclarations filtered;
    for (const auto& slot : declarations.declaration_order) {
        switch (slot.bucket) {
            case UEMeta::JsonDeclarationBucket::Class:
                if (slot.index < declarations.classes.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::classes,
                                          UEMeta::JsonDeclarationBucket::Class, declarations.classes[slot.index]);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Struct:
                if (slot.index < declarations.structs.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::structs,
                                          UEMeta::JsonDeclarationBucket::Struct, declarations.structs[slot.index]);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Union:
                if (slot.index < declarations.unions.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::unions,
                                          UEMeta::JsonDeclarationBucket::Union, declarations.unions[slot.index]);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Enum:
                if (slot.index < declarations.enums.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::enums,
                                          UEMeta::JsonDeclarationBucket::Enum, declarations.enums[slot.index]);
                }
                break;
            case UEMeta::JsonDeclarationBucket::ForwardDeclaration:
                if (slot.index < declarations.forward_declarations.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::forward_declarations,
                                          UEMeta::JsonDeclarationBucket::ForwardDeclaration,
                                          declarations.forward_declarations[slot.index]);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Alias:
                if (slot.index < declarations.aliases.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::aliases,
                                          UEMeta::JsonDeclarationBucket::Alias, declarations.aliases[slot.index]);
                }
                break;
            case UEMeta::JsonDeclarationBucket::FreeFunction:
                if (slot.index < declarations.free_functions.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::free_functions,
                                          UEMeta::JsonDeclarationBucket::FreeFunction, declarations.free_functions[slot.index]);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Global:
                if (slot.index < declarations.globals.size()) {
                    AppendIfHeaderAllowed(filtered, &UEMeta::JsonTopLevelDeclarations::globals,
                                          UEMeta::JsonDeclarationBucket::Global, declarations.globals[slot.index]);
                }
                break;
        }
    }

    declarations = std::move(filtered);
}

/// @brief Reads a source file for file content hashing.
static std::optional<std::string> ReadFileContents(const std::string& file) {
    if (file.empty() || file == "unknown") {
        return std::nullopt;
    }

    const UEMeta::StablePath stable_path{file};
    std::ifstream in{stable_path.UnderlyingPath(), std::ios::binary};
    if (!in) {
        UEM_WARN("(fs) Failed to open source file \"{}\" for hashing; falling back to path hash", file);
        return std::nullopt;
    }

    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/// @brief Hashes file contents, falling back to hashing the file path when contents are unavailable.
static std::string FileContentHash(const std::string& file) {
    if (const auto contents = ReadFileContents(file)) {
        return Md5Hex(*contents);
    }

    return Md5Hex(file);
}

/// @brief Assigns per-source-file declaration order indices used by top-level JSON declarations.
static void AssignOccurrenceIndices(UEMeta::JsonTopLevelDeclarations& declarations) {
    std::map<std::string, std::uint64_t> next_indices;

    for (const auto& slot : declarations.declaration_order) {
        switch (slot.bucket) {
            case UEMeta::JsonDeclarationBucket::Class:
                if (slot.index < declarations.classes.size()) {
                    auto& common = declarations.classes[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
            case UEMeta::JsonDeclarationBucket::Struct:
                if (slot.index < declarations.structs.size()) {
                    auto& common = declarations.structs[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
            case UEMeta::JsonDeclarationBucket::Union:
                if (slot.index < declarations.unions.size()) {
                    auto& common = declarations.unions[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
            case UEMeta::JsonDeclarationBucket::Enum:
                if (slot.index < declarations.enums.size()) {
                    auto& common = declarations.enums[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
            case UEMeta::JsonDeclarationBucket::ForwardDeclaration:
                if (slot.index < declarations.forward_declarations.size()) {
                    auto& common = declarations.forward_declarations[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
            case UEMeta::JsonDeclarationBucket::Alias:
                if (slot.index < declarations.aliases.size()) {
                    auto& common = declarations.aliases[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
            case UEMeta::JsonDeclarationBucket::FreeFunction:
                if (slot.index < declarations.free_functions.size()) {
                    auto& common = declarations.free_functions[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
            case UEMeta::JsonDeclarationBucket::Global:
                if (slot.index < declarations.globals.size()) {
                    auto& common = declarations.globals[slot.index].common;
                    common.occurrence_index = next_indices[FileGroupKey(common.file)]++;
                }
                break;
        }
    }
}

/// @brief Adds source file hashes for one declaration bucket.
template <typename Declaration>
static void AddFileHashes(const std::vector<Declaration>& declarations, std::map<std::string, std::string>& hashes) {
    for (const auto& declaration : declarations) {
        const auto file = FileGroupKey(declaration.common.file);
        if (!hashes.contains(file)) {
            hashes.emplace(file, FileContentHash(file));
        }
    }
}

/// @brief Builds the per-file content hash table for emitted declarations.
static std::map<std::string, std::string> BuildFileHashes(const UEMeta::JsonTopLevelDeclarations& declarations) {
    std::map<std::string, std::string> hashes;
    AddFileHashes(declarations.classes, hashes);
    AddFileHashes(declarations.structs, hashes);
    AddFileHashes(declarations.unions, hashes);
    AddFileHashes(declarations.enums, hashes);
    AddFileHashes(declarations.forward_declarations, hashes);
    AddFileHashes(declarations.aliases, hashes);
    AddFileHashes(declarations.free_functions, hashes);
    AddFileHashes(declarations.globals, hashes);

    return hashes;
}

/// @brief Appends one top-level declaration into its file group and preserves encounter order.
template <typename Declaration>
static void GroupDeclaration(const Declaration& declaration,
                             std::map<std::string, UEMeta::JsonTopLevelDeclarations>& groups,
                             std::vector<Declaration> UEMeta::JsonTopLevelDeclarations::*bucket,
                             const UEMeta::JsonDeclarationBucket kind) {
    AppendOrderedDeclaration(groups[FileGroupKey(declaration.common.file)], bucket, kind, declaration);
}

/// @brief Groups declarations by source file while preserving concrete declaration buckets.
static std::map<std::string, UEMeta::JsonTopLevelDeclarations> GroupDeclarations(
    const UEMeta::JsonTopLevelDeclarations& declarations) {
    std::map<std::string, UEMeta::JsonTopLevelDeclarations> groups;
    for (const auto& slot : declarations.declaration_order) {
        switch (slot.bucket) {
            case UEMeta::JsonDeclarationBucket::Class:
                if (slot.index < declarations.classes.size()) {
                    GroupDeclaration(declarations.classes[slot.index], groups, &UEMeta::JsonTopLevelDeclarations::classes,
                                     UEMeta::JsonDeclarationBucket::Class);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Struct:
                if (slot.index < declarations.structs.size()) {
                    GroupDeclaration(declarations.structs[slot.index], groups, &UEMeta::JsonTopLevelDeclarations::structs,
                                     UEMeta::JsonDeclarationBucket::Struct);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Union:
                if (slot.index < declarations.unions.size()) {
                    GroupDeclaration(declarations.unions[slot.index], groups, &UEMeta::JsonTopLevelDeclarations::unions,
                                     UEMeta::JsonDeclarationBucket::Union);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Enum:
                if (slot.index < declarations.enums.size()) {
                    GroupDeclaration(declarations.enums[slot.index], groups, &UEMeta::JsonTopLevelDeclarations::enums,
                                     UEMeta::JsonDeclarationBucket::Enum);
                }
                break;
            case UEMeta::JsonDeclarationBucket::ForwardDeclaration:
                if (slot.index < declarations.forward_declarations.size()) {
                    GroupDeclaration(declarations.forward_declarations[slot.index], groups,
                                     &UEMeta::JsonTopLevelDeclarations::forward_declarations,
                                     UEMeta::JsonDeclarationBucket::ForwardDeclaration);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Alias:
                if (slot.index < declarations.aliases.size()) {
                    GroupDeclaration(declarations.aliases[slot.index], groups, &UEMeta::JsonTopLevelDeclarations::aliases,
                                     UEMeta::JsonDeclarationBucket::Alias);
                }
                break;
            case UEMeta::JsonDeclarationBucket::FreeFunction:
                if (slot.index < declarations.free_functions.size()) {
                    GroupDeclaration(declarations.free_functions[slot.index], groups,
                                     &UEMeta::JsonTopLevelDeclarations::free_functions,
                                     UEMeta::JsonDeclarationBucket::FreeFunction);
                }
                break;
            case UEMeta::JsonDeclarationBucket::Global:
                if (slot.index < declarations.globals.size()) {
                    GroupDeclaration(declarations.globals[slot.index], groups, &UEMeta::JsonTopLevelDeclarations::globals,
                                     UEMeta::JsonDeclarationBucket::Global);
                }
                break;
        }
    }
    return groups;
}

/// @brief Returns a cached file hash or computes one when the file was not present in the table.
static std::string HashForFile(const std::map<std::string, std::string>& file_hashes, const std::string& file) {
    if (const auto it = file_hashes.find(file); it != file_hashes.end()) {
        return it->second;
    }

    return FileContentHash(file);
}

/// @brief Builds an output JSON path for a single declaration split file.
static UEMeta::StablePath OutputFileForDeclaration(const UEMeta::JsonDeclarationCommon& common,
                                                   const std::string_view extension) {
    auto stem = common.qualified_name.empty() ? common.name : common.qualified_name;
    for (std::size_t index = 0; (index = stem.find("::", index)) != std::string::npos; index += 1) {
        stem.replace(index, 2, "@");
    }

    if (stem.empty()) {
        stem = common.occurrence_index
            ? fmtquill::format("_anonymous_{}", *common.occurrence_index)
            : "_anonymous";
    }

    std::string safe_stem;
    safe_stem.reserve(stem.size());
    bool previous_was_separator = false;
    for (const auto character : stem) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) || character == '_' || character == '-' || character == '.' || character == '@') {
            safe_stem.push_back(character);
            previous_was_separator = false;
        } else if (!previous_was_separator) {
            safe_stem.push_back('_');
            previous_was_separator = true;
        }
    }

    while (!safe_stem.empty() && (safe_stem.back() == '_' || safe_stem.back() == '.')) {
        safe_stem.pop_back();
    }

    if (safe_stem.empty()) {
        safe_stem = "_anonymous";
    }

    const auto suffix = common.hash.empty() ? Md5Hex(safe_stem) : common.hash;
    return UEMeta::StablePath{
        UEMeta::Config::GetConfig().OutPath().UnderlyingPath() /
        fmtquill::format("{}-{}.{}", safe_stem, suffix, extension)};
}

/// @brief Writes already-serialized JSON text to disk, creating parent directories as needed.
static bool WriteJsonTextFile(const UEMeta::StablePath& path, const std::string& json) {
    std::error_code ec;
    std::filesystem::create_directories(path.UnderlyingPath().parent_path(), ec);
    if (ec) {
        UEM_ERROR("(fs) Failed to create output directory \"{}\": {}",
                  path.UnderlyingPath().parent_path().string(), ec.message());
        return false;
    }

    std::ofstream out{path.UnderlyingPath(), std::ios::binary | std::ios::trunc};
    if (!out) {
        UEM_ERROR("(fs) Failed to open output JSON \"{}\"", path.string());
        return false;
    }

    out << json;
    return true;
}

/// @brief Serializes a value to JSON and writes it to disk.
template <typename Value>
static bool WriteJsonFile(const UEMeta::StablePath& path, const Value& value) {
    std::string json;
    if (const auto error = glz::write_json(value, json)) {
        UEM_ERROR("(glaze) Failed to build JSON for \"{}\": {}",
                  path.string(), glz::format_error(error, json));
        return false;
    }

    return WriteJsonTextFile(path, json);
}

/// @brief Appends one include edge to the include-order table.
static void AppendIncludeOrder(std::vector<UEMeta::JsonIncludeOrder>& include_order,
                               std::string file, std::string inclusion) {
    if (file.empty() || inclusion.empty()) {
        return;
    }

    const auto existing_entry = std::ranges::find_if(include_order, [&file](const auto& entry) {
        return entry.file == file;
    });

    if (existing_entry == include_order.end()) {
        include_order.push_back(UEMeta::JsonIncludeOrder{
            .file = std::move(file),
            .inclusions = {std::move(inclusion)}
        });
        return;
    }

    existing_entry->inclusions.push_back(std::move(inclusion));
}

/// @brief Returns the direct include list recorded for one source file.
static std::vector<std::string> IncludesForFile(const std::vector<UEMeta::JsonIncludeOrder>& include_order,
                                                const std::string& file) {
    const auto entry = std::ranges::find_if(include_order, [&file](const auto& candidate) {
        return candidate.file == file;
    });

    if (entry == include_order.end()) {
        return {};
    }

    return entry->inclusions;
}

/// @brief Builds the root JSON object for one file-based output.
static UEMeta::JsonFileOutput BuildFileOutput(
    const std::string& file,
    const std::string& hash,
    const std::vector<std::string>& includes,
    const UEMeta::JsonTopLevelDeclarations& declarations) {
    UEMeta::JsonFileOutput output;
    output.path = UEMeta::JsonDetail::ScrubFilePath(file);
    output.hash = hash;
    output.includes = UEMeta::JsonDetail::ScrubFilePaths(includes);
    static_cast<UEMeta::JsonTopLevelDeclarations&>(output) = declarations;
    return output;
}

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

/// @brief Clears per-translation-unit state and starts declaration traversal progress reporting.
void UEMeta::ClangHandler::BeginTranslationUnit(clang::ASTContext& ctx) {
    GuardClangCallback("BeginTranslationUnit", [&] {
        context = &ctx;
        ClearDeclarations(declarations);
        visited_decls.clear();
        visited_forward_decls.clear();
        UEM_SPINNER_START("Parsing AST for declarations (0)");
    });
}

/// @brief Applies configured header whitelist and blacklist filters to a path string.
static bool StringPassesHeaderFilters(const std::string& str) {
    const auto& cfg = UEMeta::Config::GetConfig();
    const auto& wl = cfg.HeaderWhitelist();
    const auto& bl = cfg.HeaderBlacklist();

    if (wl.empty() && bl.empty()) return true;

    const auto StringContainsToken = [&str](const std::string& token) { return str.contains(token); };

    if (!wl.empty()) {
        if (!std::ranges::any_of(wl, StringContainsToken))
            return false;
    }

    if (!bl.empty()) {
        if (std::ranges::any_of(bl, StringContainsToken))
            return false;
    }

    return true;
}

/// @brief Filters, groups, hashes, scrubs, and writes JSON after AST traversal completes.
void UEMeta::ClangHandler::EndTranslationUnit(clang::ASTContext&) {
    GuardClangCallback("EndTranslationUnit", [&] {
        UEM_SPINNER_STOP("Finished parsing AST");
        UEM_INFO("Filtering {} declarations...", DeclarationCount(declarations));
        FilterDeclarations(declarations);
        AssignOccurrenceIndices(declarations);
        UEM_INFO("Serializing {} declarations...", DeclarationCount(declarations));

        switch (UEMeta::Config::GetConfig().GetSplitStrategy()) {
            case UEMeta::SplitStrategy::ByFile: {
                const auto file_hashes = BuildFileHashes(declarations);
                const auto groups = GroupDeclarations(declarations);
                std::for_each(std::execution::par_unseq, groups.begin(), groups.end(), [&](const auto& entry) {
                    const auto& [file, file_declarations] = entry;
                    const auto hash = HashForFile(file_hashes, file);
                    const auto includes = IncludesForFile(include_order, file);
                    const auto output = BuildFileOutput(file, hash, includes, file_declarations);
                    WriteJsonFile(OutputFileForHash(file, hash), output);
                });
                break;
            }
            case UEMeta::SplitStrategy::ByDecl:
                {
                    const auto file_hashes = BuildFileHashes(declarations);
                    const auto groups = GroupDeclarations(declarations);
                    std::for_each(std::execution::par_unseq, groups.begin(), groups.end(), [&](const auto& entry) {
                        const auto& [file, file_declarations] = entry;
                        (void)file_declarations;
                        const auto hash = HashForFile(file_hashes, file);
                        WriteJsonFile(OutputFileForHash(file, hash, "file"), UEMeta::JsonFileMetadataOutput{
                            .path = UEMeta::JsonDetail::ScrubFilePath(file),
                            .hash = hash,
                            .includes = UEMeta::JsonDetail::ScrubFilePaths(IncludesForFile(include_order, file))
                        });
                    });
                }
                std::for_each(std::execution::par_unseq,
                              declarations.declaration_order.begin(),
                              declarations.declaration_order.end(),
                              [&](const auto& slot) {
                    switch (slot.bucket) {
                        case UEMeta::JsonDeclarationBucket::Class:
                            if (slot.index < declarations.classes.size()) {
                                const auto& declaration = declarations.classes[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "class"), declaration);
                            }
                            break;
                        case UEMeta::JsonDeclarationBucket::Struct:
                            if (slot.index < declarations.structs.size()) {
                                const auto& declaration = declarations.structs[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "struct"), declaration);
                            }
                            break;
                        case UEMeta::JsonDeclarationBucket::Union:
                            if (slot.index < declarations.unions.size()) {
                                const auto& declaration = declarations.unions[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "union"), declaration);
                            }
                            break;
                        case UEMeta::JsonDeclarationBucket::Enum:
                            if (slot.index < declarations.enums.size()) {
                                const auto& declaration = declarations.enums[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "enum"), declaration);
                            }
                            break;
                        case UEMeta::JsonDeclarationBucket::ForwardDeclaration:
                            if (slot.index < declarations.forward_declarations.size()) {
                                const auto& declaration = declarations.forward_declarations[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "forwardDeclaration"), declaration);
                            }
                            break;
                        case UEMeta::JsonDeclarationBucket::Alias:
                            if (slot.index < declarations.aliases.size()) {
                                const auto& declaration = declarations.aliases[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "alias"), declaration);
                            }
                            break;
                        case UEMeta::JsonDeclarationBucket::FreeFunction:
                            if (slot.index < declarations.free_functions.size()) {
                                const auto& declaration = declarations.free_functions[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "function"), declaration);
                            }
                            break;
                        case UEMeta::JsonDeclarationBucket::Global:
                            if (slot.index < declarations.globals.size()) {
                                const auto& declaration = declarations.globals[slot.index];
                                WriteJsonFile(OutputFileForDeclaration(declaration.common, "variable"), declaration);
                            }
                            break;
                    }
                });
                break;
        }
    });
}

/// @brief Visits a top-level class, struct, union, or record forward declaration.
bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl* decl) {
    return GuardClangVisitor("VisitRecordDecl", [&] {
        if (!context || ShouldSkipRecord(decl)) {
            return true;
        }

        const auto* target = decl;
        if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
            return true;
        }

        if (target->isThisDeclarationADefinition()) {
            const auto* key = target->getCanonicalDecl();
            if (!visited_decls.insert(key).second) {
                return true;
            }
            if (target->isUnion()) {
                auto declaration = BuildUnionDeclaration(target, *context, visited_decls, visited_forward_decls);
                declaration.common.hash = DeclarationSourceHash(target, *context);
                AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::unions,
                                         JsonDeclarationBucket::Union, std::move(declaration));
            } else if (target->isClass()) {
                auto declaration = BuildClassLikeDeclaration<UEMeta::JsonClassDeclaration>(target, *context, visited_decls, visited_forward_decls);
                declaration.common.hash = DeclarationSourceHash(target, *context);
                AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::classes,
                                         JsonDeclarationBucket::Class, std::move(declaration));
            } else {
                auto declaration = BuildClassLikeDeclaration<UEMeta::JsonStructDeclaration>(target, *context, visited_decls, visited_forward_decls);
                declaration.common.hash = DeclarationSourceHash(target, *context);
                AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::structs,
                                         JsonDeclarationBucket::Struct, std::move(declaration));
            }
        } else {
            if (!visited_forward_decls.insert(target).second) {
                return true;
            }
            auto declaration = BuildRecordForwardDeclaration(target, *context);
            declaration.common.hash = DeclarationSourceHash(target, *context);
            AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::forward_declarations,
                                     JsonDeclarationBucket::ForwardDeclaration, std::move(declaration));
        }

        UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", DeclarationCount(declarations)));
        return true;
    });
}

/// @brief Visits a class template through its templated record declaration.
bool UEMeta::ClangHandler::VisitClassTemplateDecl(clang::ClassTemplateDecl* decl) {
    return GuardClangVisitor("VisitClassTemplateDecl", [&] {
        return VisitRecordDecl(decl ? decl->getTemplatedDecl() : nullptr);
    });
}

/// @brief Visits a top-level enum definition or forward declaration.
bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* decl) {
    return GuardClangVisitor("VisitEnumDecl", [&] {
        if (!context || ShouldSkipEnum(decl)) {
            return true;
        }

        const auto* target = decl;
        if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
            return true;
        }

        if (target->isThisDeclarationADefinition()) {
            const auto* key = target->getCanonicalDecl();
            if (!visited_decls.insert(key).second) {
                return true;
            }
            auto declaration = BuildEnumDeclaration(target, *context);
            declaration.common.hash = DeclarationSourceHash(target, *context);
            AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::enums,
                                     JsonDeclarationBucket::Enum, std::move(declaration));
        } else {
            if (!visited_forward_decls.insert(target).second) {
                return true;
            }
            auto declaration = BuildEnumForwardDeclaration(target, *context);
            declaration.common.hash = DeclarationSourceHash(target, *context);
            AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::forward_declarations,
                                     JsonDeclarationBucket::ForwardDeclaration, std::move(declaration));
        }

        UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", DeclarationCount(declarations)));
        return true;
    });
}

/// @brief Visits a top-level free function declaration.
bool UEMeta::ClangHandler::VisitFunctionDecl(clang::FunctionDecl* decl) {
    return GuardClangVisitor("VisitFunctionDecl", [&] {
        if (!context || ShouldSkipFunction(decl) || llvm::isa<clang::CXXMethodDecl>(decl)) {
            return true;
        }

        const auto* target = decl->getCanonicalDecl();
        if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
            return true;
        }

        if (!visited_decls.insert(target).second) {
            return true;
        }

        auto declaration = BuildFreeFunctionDeclaration(decl, *context);
        declaration.common.hash = DeclarationSourceHash(decl, *context);
        AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::free_functions,
                                 JsonDeclarationBucket::FreeFunction, std::move(declaration));
        UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", DeclarationCount(declarations)));
        return true;
    });
}

/// @brief Visits a function template through its templated function declaration.
bool UEMeta::ClangHandler::VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* decl) {
    return GuardClangVisitor("VisitFunctionTemplateDecl", [&] {
        return VisitFunctionDecl(decl ? decl->getTemplatedDecl() : nullptr);
    });
}

/// @brief Visits a top-level type alias declaration.
bool UEMeta::ClangHandler::VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
    return GuardClangVisitor("VisitTypeAliasDecl", [&] {
        if (!context || !decl || decl->isImplicit() || decl->isInvalidDecl()) {
            return true;
        }

        if (!IsTopLevelNamedDecl(decl) || IsInSystemHeader(decl, *context)) {
            return true;
        }

        const auto* key = decl->getCanonicalDecl();
        if (!visited_decls.insert(key).second) {
            return true;
        }

        auto declaration = BuildAliasDeclaration(decl, *context);
        declaration.common.hash = DeclarationSourceHash(decl, *context);
        AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::aliases,
                                 JsonDeclarationBucket::Alias, std::move(declaration));
        UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", DeclarationCount(declarations)));
        return true;
    });
}

/// @brief Visits a type alias template through its templated alias declaration.
bool UEMeta::ClangHandler::VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl* decl) {
    return GuardClangVisitor("VisitTypeAliasTemplateDecl", [&] {
        return VisitTypeAliasDecl(decl ? decl->getTemplatedDecl() : nullptr);
    });
}

/// @brief Visits a top-level global variable declaration.
bool UEMeta::ClangHandler::VisitVarDecl(clang::VarDecl* decl) {
    return GuardClangVisitor("VisitVarDecl", [&] {
        if (!context || ShouldSkipVariable(decl) || decl->isStaticDataMember() || !decl->hasGlobalStorage()) {
            return true;
        }

        const auto* target = decl->getCanonicalDecl();
        if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
            return true;
        }

        if (!visited_decls.insert(target).second) {
            return true;
        }

        auto declaration = BuildGlobalVariableDeclaration(decl, *context);
        declaration.common.hash = DeclarationSourceHash(decl, *context);
        AppendOrderedDeclaration(declarations, &JsonTopLevelDeclarations::globals,
                                 JsonDeclarationBucket::Global, std::move(declaration));
        UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", DeclarationCount(declarations)));
        return true;
    });
}

/// @brief Visits a variable template through its templated variable declaration.
bool UEMeta::ClangHandler::VisitVarTemplateDecl(clang::VarTemplateDecl* decl) {
    return GuardClangVisitor("VisitVarTemplateDecl", [&] {
        return VisitVarDecl(decl ? decl->getTemplatedDecl() : nullptr);
    });
}

/// @brief Creates the AST consumer and include-order callback for one translation unit.
std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                            llvm::StringRef file) {
    try {
        const auto tu_name = std::filesystem::path(file.str()).filename().string();
        UEM_SPINNER_START(fmtquill::format("Parsing TU '{}' (this may take a moment)",
                                           tu_name.empty() ? file.str() : tu_name));
        ticker_thread = std::jthread([tu_name](std::stop_token token) {
            GuardClangCallback("translation unit spinner", [&] {
                while (true) {
                    if (token.stop_requested()) {
                        UEM_SPINNER_STOP(fmtquill::format("TU '{}' parsed!", tu_name)); // NOLINT(*-lambda-function-name)
                        return;
                    }
                    UEM_SPINNER_TICK; // NOLINT(*-lambda-function-name)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });
        });
        include_order.clear();

        /// @brief AST consumer that drives traversal and output once Clang finishes parsing a translation unit.
        class Consumer : public clang::ASTConsumer {
        public:
            /// @brief Creates a consumer tied to the owning ClangHandler.
            explicit Consumer(ClangHandler* owner) : owner(owner) {}

            /// @brief Stops the TU spinner, traverses declarations, and emits output.
            void HandleTranslationUnit(clang::ASTContext& ctx) override {
                GuardClangCallback("HandleTranslationUnit", [&] {
                    if (owner->ticker_thread.joinable()) {
                        owner->ticker_thread.request_stop();
                        owner->ticker_thread.join();
                    }
                    UEM_INFO("Starting AST traversal...");
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
        };

        /// @brief Preprocessor callback that records source-to-include edges in directive order.
        class IncludeOrderCallback : public clang::PPCallbacks {
        public:
            /// @brief Creates a callback tied to the owning ClangHandler and source manager.
            IncludeOrderCallback(ClangHandler* owner, const clang::SourceManager& source_manager)
                : owner(owner), source_manager(source_manager) {}

            /// @brief Records an include edge after applying configured header filters.
            void InclusionDirective(clang::SourceLocation hash_location, const clang::Token&, llvm::StringRef,
                                    bool, clang::CharSourceRange, clang::OptionalFileEntryRef included_file,
                                    llvm::StringRef, llvm::StringRef, const clang::Module*,
                                    bool, clang::SrcMgr::CharacteristicKind) override {
                GuardClangCallback("InclusionDirective", [&] {
                    if (!included_file) {
                        return;
                    }

                    const auto src_file = FilePathForLocation(source_manager, hash_location);
                    auto included_path = included_file->getFileEntry().tryGetRealPathName();
                    if (included_path.empty()) {
                        included_path = included_file->getName();
                    }
                    auto included_stable_path = UEMeta::JsonDetail::StablePathString(included_path.str());
                    if (!StringPassesHeaderFilters(src_file) || !StringPassesHeaderFilters(included_stable_path)) return;
                    AppendIncludeOrder(owner->include_order, src_file, included_stable_path);
                });
            }

        private:
            ClangHandler* owner;
            const clang::SourceManager& source_manager;
        };

        compiler.getPreprocessor().addPPCallbacks(std::make_unique<IncludeOrderCallback>(this, compiler.getSourceManager()));
        return std::make_unique<Consumer>(this);
    } catch (const std::exception& ex) {
        LogClangException("CreateASTConsumer", ex);
    } catch (...) {
        LogClangUnknownException("CreateASTConsumer");
    }

    return std::make_unique<clang::ASTConsumer>();
}
