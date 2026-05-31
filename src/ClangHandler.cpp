// ReSharper disable CppMemberFunctionMayBeStatic
#include "UEMeta/ClangHandler.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/GlobalDecl.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RawCommentList.h>
#include <clang/AST/RecordLayout.h>
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
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/MD5.h>
#include <llvm/Support/raw_ostream.h>

#include "UEMeta/Cli.hpp"
#include "UEMeta/StablePath.hpp"

static clang::PrintingPolicy MakePrintingPolicy(const clang::ASTContext& ctx) {
    clang::PrintingPolicy policy{ctx.getLangOpts()};
    policy.adjustForCPlusPlus();
    policy.SuppressScope = false;
    policy.SuppressUnwrittenScope = true;
    policy.SuppressTagKeyword = true;
    policy.ConstantsAsWritten = false;
    return policy;
}

static std::string PrintType(const clang::ASTContext& ctx, const clang::QualType type, const std::string& placeholder = {}) {
    if (type.isNull()) {
        return "";
    }

    std::string out;
    llvm::raw_string_ostream stream{out};
    type.print(stream, MakePrintingPolicy(ctx), placeholder);
    return stream.str();
}

static const clang::DeclContext* NonTransparentContext(const clang::DeclContext* context) {
    while (context && context->isTransparentContext()) {
        context = context->getParent();
    }
    return context;
}

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

static std::string Md5Hex(const std::string_view value) {
    llvm::MD5 hash;
    hash.update(llvm::StringRef{value.data(), value.size()});
    auto result = hash.final();
    const auto digest = result.digest();
    return digest.str().str();
}

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

static inline bool IsImplicitInstantiation(const clang::TemplateSpecializationKind kind) {
    return kind == clang::TSK_ImplicitInstantiation;
}

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

static bool ShouldSkipFunction(const clang::FunctionDecl* decl) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl()) {
        return true;
    }

    if (decl->isFunctionTemplateSpecialization() && IsImplicitInstantiation(decl->getTemplateSpecializationKind())) {
        return true;
    }

    return false;
}

static bool ShouldSkipVariable(const clang::VarDecl* decl) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl()) {
        return true;
    }

    if (IsImplicitInstantiation(decl->getTemplateSpecializationKind())) {
        return true;
    }

    return false;
}

static bool ShouldSkipEnum(const clang::EnumDecl* decl) {
    return !decl || decl->isImplicit() || decl->isInvalidDecl() ||
           IsImplicitInstantiation(decl->getTemplateSpecializationKind());
}

static bool IsTopLevelNamedDecl(const clang::NamedDecl* decl) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl() || decl->getParentFunctionOrMethod()) {
        return false;
    }

    const auto* context = NonTransparentContext(decl->getDeclContext());
    return llvm::isa_and_nonnull<clang::TranslationUnitDecl>(context) ||
           llvm::isa_and_nonnull<clang::NamespaceDecl>(context);
}

static bool IsInSystemHeader(const clang::Decl* decl, const clang::ASTContext& ctx) {
    if (!decl) {
        return true;
    }

    const auto& source_manager = ctx.getSourceManager();
    const auto location = source_manager.getExpansionLoc(decl->getLocation());
    return location.isInvalid() || source_manager.isInSystemHeader(location);
}

static inline std::string StablePathString(const std::filesystem::path& path) {
    return UEMeta::StablePath{path}.string();
}

static std::string FilePathForLocation(const clang::SourceManager& source_manager,
                                       const clang::SourceLocation location) {
    const auto expansion_location = source_manager.getExpansionLoc(location);
    if (expansion_location.isInvalid()) {
        return "";
    }

    if (const auto presumed = source_manager.getPresumedLoc(expansion_location); presumed.isValid()) {
        return StablePathString(presumed.getFilename());
    }

    if (auto file_entry = source_manager.getFileEntryRefForID(source_manager.getFileID(expansion_location))) {
        return StablePathString(file_entry->getName().str());
    }

    return "";
}

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

static std::string DeclarationSourceHash(const clang::NamedDecl* decl, const clang::ASTContext& ctx) {
    const auto text = RawSourceTextForHash(decl, ctx);
    if (!text) {
        return "";
    }

    return Md5Hex(*text);
}

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

static void FillCommonDeclaration(UEMeta::JsonDeclaration& out, const clang::NamedDecl* decl, const clang::ASTContext& ctx) {
    out.name = decl->getNameAsString();
    out.qualified_name = QualifiedName(decl);
    out.file = FilePathForLocation(ctx.getSourceManager(), decl->getLocation());
    out.scope = BuildScope(decl);
    out.is_anonymous = out.name.empty();
    out.documentation = DocumentationForDecl(decl, ctx);
}

static UEMeta::JsonTemplateParameter BuildTemplateParameter(const clang::NamedDecl* param, const clang::ASTContext& ctx);

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

static std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForRecord(const clang::RecordDecl* decl,
                                                                              const clang::ASTContext& ctx) {
    if (const auto* cxx_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl)) {
        if (const auto* templ = cxx_record->getDescribedClassTemplate()) {
            return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
        }
    }

    return {};
}

static inline std::string EnumValueToString(const clang::EnumConstantDecl* enumerator) {
    llvm::SmallString<32> value;
    enumerator->getInitVal().toString(value, 10);
    return value.str().str();
}

static std::string RecordKind(const clang::RecordDecl* decl) {
    if (decl->isUnion()) {
        return "union";
    }

    if (decl->isClass()) {
        return "class";
    }

    return "struct";
}

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

static UEMeta::JsonFunction BuildFunction(const clang::FunctionDecl* decl, clang::ASTContext& ctx) {
    UEMeta::JsonFunction out;
    out.kind = "function";
    out.name = decl->getNameAsString();
    out.qualified_name = QualifiedName(decl);
    out.file = FilePathForLocation(ctx.getSourceManager(), decl->getLocation());
    out.scope = BuildScope(decl);
    out.documentation = DocumentationForDecl(decl, ctx);
    out.return_type = PrintType(ctx, decl->getReturnType());
    out.storage_class = StorageClassToString(decl->getStorageClass());
    out.is_constexpr = decl->isConstexpr();
    out.is_consteval = decl->isConsteval();
    out.is_inline = decl->isInlined() || decl->isInlineSpecified();
    out.is_deleted = decl->isDeleted();
    out.is_defaulted = decl->isDefaulted();
    out.exception_spec = ExceptionSpecToString(decl->getExceptionSpecType());
    if (const auto* templ = decl->getDescribedFunctionTemplate()) {
        out.template_parameters = BuildTemplateParameters(templ->getTemplateParameters(), ctx);
    }

    if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
        out.kind = "method";
        out.access = AccessToString(method->getAccess());
        out.is_const = method->isConst();
        out.is_volatile = method->isVolatile();
        out.is_static = method->isStatic();
        out.is_virtual = method->isVirtual();
        out.is_pure = method->isPureVirtual();
        out.ref_qualifier = RefQualifierToString(method->getRefQualifier());
        out.vtable_index = BuildVTableIndex(method, ctx);

        if (const auto* constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(method)) {
            out.kind = "constructor";
            out.return_type.clear();
            out.is_explicit = constructor->isExplicit();
        } else if (llvm::isa<clang::CXXDestructorDecl>(method)) {
            out.kind = "destructor";
            out.return_type.clear();
        } else if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(method)) {
            out.kind = "conversion";
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
    out.is_constexpr = decl->isConstexpr();
    out.is_inline = decl->isInline();
    out.is_static_data_member = decl->isStaticDataMember();
    out.is_thread_local = decl->getTLSKind() != clang::VarDecl::TLS_None;
    return out;
}

static UEMeta::JsonDeclaration BuildEnumDeclaration(const clang::EnumDecl* input, const clang::ASTContext& ctx) {
    const auto* decl = input;

    UEMeta::JsonDeclaration out;
    out.kind = "enum";
    FillCommonDeclaration(out, decl, ctx);
    out.template_parameters = BuildTemplateParameters(nullptr, ctx);
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

static UEMeta::JsonDeclaration BuildEnumForwardDeclaration(const clang::EnumDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonDeclaration out;
    out.kind = "forwardDeclaration";
    FillCommonDeclaration(out, decl, ctx);
    out.forward_declaration_kind = "enum";
    out.is_scoped = decl->isScoped();
    out.scoped_kind = decl->isScoped() ? (decl->isScopedUsingClassTag() ? "class" : "struct") : "";
    out.underlying_type = decl->getIntegerType().isNull() ? "" : PrintType(ctx, decl->getIntegerType());
    return out;
}

static UEMeta::JsonDeclaration BuildAliasDeclaration(const clang::TypeAliasDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonDeclaration out;
    out.kind = "alias";
    FillCommonDeclaration(out, decl, ctx);
    if (const auto* templ = decl->getDescribedAliasTemplate()) {
        out.template_parameters = BuildTemplateParameters(templ->getTemplateParameters(), ctx);
    }
    out.aliased_type = PrintType(ctx, decl->getUnderlyingType());
    return out;
}

static UEMeta::JsonDeclaration BuildVariableDeclaration(const clang::VarDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonDeclaration out;
    out.kind = "variable";
    FillCommonDeclaration(out, decl, ctx);
    if (const auto* templ = decl->getDescribedVarTemplate()) {
        out.template_parameters = BuildTemplateParameters(templ->getTemplateParameters(), ctx);
    }
    out.variable = BuildVariable(decl, ctx);
    return out;
}

static UEMeta::JsonDeclaration BuildFunctionDeclaration(const clang::FunctionDecl* decl, clang::ASTContext& ctx) {
    UEMeta::JsonDeclaration out;
    out.kind = "function";
    FillCommonDeclaration(out, decl, ctx);
    out.function = BuildFunction(decl, ctx);
    return out;
}

static void AppendNestedRecord(const clang::RecordDecl* decl, clang::ASTContext& ctx,
                               llvm::DenseSet<const clang::Decl*>& seen,
                               llvm::DenseSet<const clang::Decl*>& seen_forwards,
                               std::vector<UEMeta::JsonDeclaration>& nested);

static void AppendNestedEnum(const clang::EnumDecl* decl, const clang::ASTContext& ctx,
                             llvm::DenseSet<const clang::Decl*>& seen,
                             llvm::DenseSet<const clang::Decl*>& seen_forwards,
                             std::vector<UEMeta::JsonDeclaration>& nested) {
    if (ShouldSkipEnum(decl)) {
        return;
    }

    const auto* target = decl;
    if (IsInSystemHeader(target, ctx)) {
        return;
    }

    UEMeta::JsonDeclaration declaration;
    if (target->isThisDeclarationADefinition()) {
        const auto* key = target->getCanonicalDecl();
        if (!seen.insert(key).second) {
            return;
        }
        declaration = BuildEnumDeclaration(target, ctx);
    } else {
        if (!seen_forwards.insert(target).second) {
            return;
        }
        declaration = BuildEnumForwardDeclaration(target, ctx);
    }

    nested.push_back(std::move(declaration));
}

static void AppendNestedAlias(const clang::TypeAliasDecl* decl, const clang::ASTContext& ctx,
                              llvm::DenseSet<const clang::Decl*>& seen,
                              std::vector<UEMeta::JsonDeclaration>& nested) {
    if (!decl || decl->isImplicit() || decl->isInvalidDecl() || IsInSystemHeader(decl, ctx)) {
        return;
    }

    const auto* key = decl->getCanonicalDecl();
    if (!seen.insert(key).second) {
        return;
    }

    nested.push_back(BuildAliasDeclaration(decl, ctx));
}

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

static UEMeta::JsonDeclaration BuildRecordForwardDeclaration(const clang::RecordDecl* decl, const clang::ASTContext& ctx) {
    UEMeta::JsonDeclaration out;
    out.kind = "forwardDeclaration";
    FillCommonDeclaration(out, decl, ctx);
    out.template_parameters = TemplateParametersForRecord(decl, ctx);
    out.forward_declaration_kind = RecordKind(decl);
    return out;
}

static UEMeta::JsonDeclaration BuildRecordDeclaration(const clang::RecordDecl* input, clang::ASTContext& ctx,
                                                      llvm::DenseSet<const clang::Decl*>& seen,
                                                      llvm::DenseSet<const clang::Decl*>& seen_forwards) {
    const auto* decl = input;

    UEMeta::JsonDeclaration out;
    out.kind = RecordKind(decl);
    FillCommonDeclaration(out, decl, ctx);
    out.template_parameters = TemplateParametersForRecord(decl, ctx);
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
                method && !ShouldSkipFunction(method) && methods_seen.insert(method->getCanonicalDecl()).second) {
                out.methods.push_back(BuildFunction(method, ctx));
            }
            continue;
        }

        if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(member)) {
            if (!ShouldSkipFunction(method) && methods_seen.insert(method->getCanonicalDecl()).second) {
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

    return out;
}

static void AppendNestedRecord(const clang::RecordDecl* decl, clang::ASTContext& ctx,
                               llvm::DenseSet<const clang::Decl*>& seen,
                               llvm::DenseSet<const clang::Decl*>& seen_forwards,
                               std::vector<UEMeta::JsonDeclaration>& nested) {
    if (ShouldSkipRecord(decl)) {
        return;
    }

    const auto* target = decl;
    if (IsInSystemHeader(target, ctx)) {
        return;
    }

    UEMeta::JsonDeclaration declaration;
    if (target->isThisDeclarationADefinition()) {
        const auto* key = target->getCanonicalDecl();
        if (!seen.insert(key).second) {
            return;
        }
        declaration = BuildRecordDeclaration(target, ctx, seen, seen_forwards);
    } else {
        if (!seen_forwards.insert(target).second) {
            return;
        }
        declaration = BuildRecordForwardDeclaration(target, ctx);
    }

    nested.push_back(std::move(declaration));
}

static std::string SanitizeFileStem(const std::string_view value) {
    std::string out;
    out.reserve(value.size());

    bool previous_was_separator = false;
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) || character == '_' || character == '-' || character == '.') {
            out.push_back(character);
            previous_was_separator = false;
        } else if (!previous_was_separator) {
            out.push_back('_');
            previous_was_separator = true;
        }
    }

    while (!out.empty() && out.front() == '_') {
        out.erase(out.begin());
    }

    while (!out.empty() && out.back() == '_') {
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

static UEMeta::StablePath OutputFileForHash(const std::string& label, const std::string& hash) {
    const auto stem = SanitizeFileStem(label);
    const auto suffix = hash.empty() ? Md5Hex(label) : hash;
    return UEMeta::StablePath{
        UEMeta::Config::GetConfig().OutPath().UnderlyingPath() /
        fmtquill::format("{}-{}.json", stem, suffix)};
}

static inline std::string FileGroupKey(const std::string& file) {
    return file.empty() ? "unknown" : file;
}

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

static std::string FileContentHash(const std::string& file) {
    if (const auto contents = ReadFileContents(file)) {
        return Md5Hex(*contents);
    }

    return Md5Hex(file);
}

static std::map<std::string, std::string> BuildFileHashes(const std::vector<UEMeta::JsonDeclaration>& declarations) {
    std::map<std::string, std::string> hashes;
    for (const auto& declaration : declarations) {
        const auto file = FileGroupKey(declaration.file);
        if (!hashes.contains(file)) {
            hashes.emplace(file, FileContentHash(file));
        }
    }

    return hashes;
}

static std::string HashForFile(const std::map<std::string, std::string>& file_hashes, const std::string& file) {
    if (const auto it = file_hashes.find(file); it != file_hashes.end()) {
        return it->second;
    }

    return FileContentHash(file);
}

static std::map<std::string, std::string> ScrubFileHashes(const std::map<std::string, std::string>& file_hashes) {
    std::map<std::string, std::string> out;
    for (const auto& [file, hash] : file_hashes) {
        out[UEMeta::JsonDetail::ScrubFilePath(file)] = hash;
    }

    return out;
}

static std::string ParentDirectoryGroup(const std::string& file) {
    const UEMeta::StablePath file_path{file};
    const auto file_string = file_path.string();
    auto lowercase_file = file_string;
    std::ranges::transform(lowercase_file, lowercase_file.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    for (const auto& configured_parent : UEMeta::Config::GetConfig().PdPaths()) {
        auto parent_string = configured_parent.string();
        std::ranges::transform(parent_string, parent_string.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        if (!parent_string.ends_with(std::filesystem::path::preferred_separator)) {
            parent_string.push_back(std::filesystem::path::preferred_separator);
        }

        if (lowercase_file.starts_with(parent_string)) {
            return configured_parent.string();
        }
    }

    return file;
}

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

static std::vector<std::string> ScrubFilePaths(const std::vector<std::string>& paths) {
    std::vector<std::string> out;
    out.reserve(paths.size());
    for (const auto& path : paths) {
        out.push_back(UEMeta::JsonDetail::ScrubFilePath(path));
    }
    return out;
}

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

static std::vector<UEMeta::JsonIncludeOrder> ScrubIncludeOrder(const std::vector<UEMeta::JsonIncludeOrder>& include_order) {
    std::vector<UEMeta::JsonIncludeOrder> out;
    out.reserve(include_order.size());
    for (const auto& entry : include_order) {
        out.push_back(UEMeta::JsonIncludeOrder{
            .file = UEMeta::JsonDetail::ScrubFilePath(entry.file),
            .inclusions = ScrubFilePaths(entry.inclusions)
        });
    }
    return out;
}

bool UEMeta::ClangHandler::shouldVisitTemplateInstantiations() const { return true; }
bool UEMeta::ClangHandler::shouldVisitImplicitCode() const { return false; }
bool UEMeta::ClangHandler::shouldVisitLambdaBody() const { return false; }

void UEMeta::ClangHandler::BeginTranslationUnit(clang::ASTContext& ctx) {
    context = &ctx;
    declarations.clear();
    visited_decls.clear();
    visited_forward_decls.clear();
    UEM_SPINNER_START("Parsing AST for declarations (0)");
}

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

void UEMeta::ClangHandler::EndTranslationUnit(clang::ASTContext&) {
    UEM_SPINNER_STOP("Finished parsing AST");
    const auto scrubbed_include_order = ScrubIncludeOrder(include_order);
    WriteJsonFile(StablePath{Config::GetConfig().OutPath().UnderlyingPath() / "IncludeOrder.json"}, scrubbed_include_order);
    UEM_INFO("Filtering {} declarations...", declarations.size());
    std::erase_if(declarations, [](const JsonDeclaration& decl){ return !StringPassesHeaderFilters(decl.file); });
    const auto file_hashes = BuildFileHashes(declarations);
    WriteJsonFile(StablePath{Config::GetConfig().OutPath().UnderlyingPath() / "FileHashes.json"}, ScrubFileHashes(file_hashes));
    UEM_INFO("Serializing {} declarations...", declarations.size());
    switch (Config::GetConfig().SplitStrategy()) {
        case FileSplitStrategy::Monofile: {
            WriteJsonFile(StablePath{Config::GetConfig().OutPath().UnderlyingPath() / "uemeta.json"}, declarations);
            break;
        }
        case FileSplitStrategy::ByClass: {
            std::size_t anonymous_index = 0;
            for (const auto& declaration : declarations) {
                const auto key = declaration.qualified_name.empty()
                    ? fmtquill::format("{}-anonymous-{}", declaration.kind, anonymous_index++)
                    : declaration.qualified_name;
                const auto label = declaration.qualified_name.empty() ? key : declaration.qualified_name;
                const auto hash = declaration.hash.empty() ? Md5Hex(key) : declaration.hash;
                WriteJsonFile(OutputFileForHash(label, hash), std::vector{declaration});
            }
            break;
        }
        case FileSplitStrategy::ByParentDirectory: {
            std::map<std::string, std::vector<JsonDeclaration>> groups;
            for (const auto& declaration : declarations) {
                const auto group_key = ParentDirectoryGroup(declaration.file);
                groups[group_key].push_back(declaration);
            }

            for (const auto& [group_key, group_declarations] : groups) {
                const auto label = std::filesystem::path{group_key}.filename().string().empty()
                    ? group_key
                    : std::filesystem::path{group_key}.filename().string();
                WriteJsonFile(OutputFileForHash(label, Md5Hex(group_key)), group_declarations);
            }
            break;
        }
        case FileSplitStrategy::Default:
        case FileSplitStrategy::ByFile: {
            std::map<std::string, std::vector<JsonDeclaration>> groups;
            for (const auto& declaration : declarations) {
                groups[FileGroupKey(declaration.file)].push_back(declaration);
            }

            for (const auto& [file, file_declarations] : groups) {
                const auto filename = std::filesystem::path{file}.filename().string();
                WriteJsonFile(OutputFileForHash(filename.empty() ? file : filename, HashForFile(file_hashes, file)),
                              file_declarations);
            }
            break;
        }
    }
}

bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl* decl) {
    if (!context || ShouldSkipRecord(decl)) {
        return true;
    }

    const auto* target = decl;
    if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
        return true;
    }

    JsonDeclaration declaration;
    if (target->isThisDeclarationADefinition()) {
        const auto* key = target->getCanonicalDecl();
        if (!visited_decls.insert(key).second) {
            return true;
        }
        declaration = BuildRecordDeclaration(target, *context, visited_decls, visited_forward_decls);
    } else {
        if (!visited_forward_decls.insert(target).second) {
            return true;
        }
        declaration = BuildRecordForwardDeclaration(target, *context);
    }

    declaration.hash = DeclarationSourceHash(target, *context);
    declarations.push_back(std::move(declaration));
    UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", declarations.size()));
    return true;
}

bool UEMeta::ClangHandler::VisitClassTemplateDecl(clang::ClassTemplateDecl* decl) {
    return VisitRecordDecl(decl ? decl->getTemplatedDecl() : nullptr);
}

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* decl) {
    if (!context || ShouldSkipEnum(decl)) {
        return true;
    }

    const auto* target = decl;
    if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
        return true;
    }

    JsonDeclaration declaration;
    if (target->isThisDeclarationADefinition()) {
        const auto* key = target->getCanonicalDecl();
        if (!visited_decls.insert(key).second) {
            return true;
        }
        declaration = BuildEnumDeclaration(target, *context);
    } else {
        if (!visited_forward_decls.insert(target).second) {
            return true;
        }
        declaration = BuildEnumForwardDeclaration(target, *context);
    }

    declaration.hash = DeclarationSourceHash(target, *context);
    declarations.push_back(std::move(declaration));
    UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", declarations.size()));
    return true;
}

bool UEMeta::ClangHandler::VisitFunctionDecl(clang::FunctionDecl* decl) {
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

    auto declaration = BuildFunctionDeclaration(target, *context);
    declaration.hash = DeclarationSourceHash(target, *context);
    declarations.push_back(std::move(declaration));
    UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", declarations.size()));
    return true;
}

bool UEMeta::ClangHandler::VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* decl) {
    return VisitFunctionDecl(decl ? decl->getTemplatedDecl() : nullptr);
}

bool UEMeta::ClangHandler::VisitTypeAliasDecl(clang::TypeAliasDecl* decl) {
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
    declaration.hash = DeclarationSourceHash(decl, *context);
    declarations.push_back(std::move(declaration));
    UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", declarations.size()));
    return true;
}

bool UEMeta::ClangHandler::VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl* decl) {
    return VisitTypeAliasDecl(decl ? decl->getTemplatedDecl() : nullptr);
}

bool UEMeta::ClangHandler::VisitVarDecl(clang::VarDecl* decl) {
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

    auto declaration = BuildVariableDeclaration(target, *context);
    declaration.hash = DeclarationSourceHash(target, *context);
    declarations.push_back(std::move(declaration));
    UEM_SPINNER_UPDATE(fmtquill::format("Parsing AST for declarations ({})", declarations.size()));
    return true;
}

bool UEMeta::ClangHandler::VisitVarTemplateDecl(clang::VarTemplateDecl* decl) {
    return VisitVarDecl(decl ? decl->getTemplatedDecl() : nullptr);
}

std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                            llvm::StringRef file) {
    const auto tu_name = std::filesystem::path(file.str()).filename().string();
    UEM_SPINNER_START(fmtquill::format("Parsing TU '{}' (this may take a moment)",
                                       tu_name.empty() ? file.str() : tu_name));
    ticker_thread = std::jthread([tu_name](std::stop_token token) {
        while (true) {
            if (token.stop_requested()) {
                UEM_SPINNER_STOP(fmtquill::format("TU '{}' parsed!", tu_name)); // NOLINT(*-lambda-function-name)
                return;
            }
            UEM_SPINNER_TICK; // NOLINT(*-lambda-function-name)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    include_order.clear();

    class Consumer : public clang::ASTConsumer {
    public:
        explicit Consumer(ClangHandler* owner) : owner(owner) {}

        void HandleTranslationUnit(clang::ASTContext& ctx) override {
            owner->ticker_thread.request_stop();
            owner->ticker_thread.join();
            UEM_INFO("Starting AST traversal...");
            owner->BeginTranslationUnit(ctx);
            owner->TraverseDecl(ctx.getTranslationUnitDecl());
            owner->EndTranslationUnit(ctx);
        }

    private:
        ClangHandler* owner;
    };

    class IncludeOrderCallback : public clang::PPCallbacks {
    public:
        IncludeOrderCallback(ClangHandler* owner, const clang::SourceManager& source_manager)
            : owner(owner), source_manager(source_manager) {}

        void InclusionDirective(clang::SourceLocation hash_location, const clang::Token&, llvm::StringRef,
                                bool, clang::CharSourceRange, clang::OptionalFileEntryRef included_file,
                                llvm::StringRef, llvm::StringRef, const clang::Module*,
                                bool, clang::SrcMgr::CharacteristicKind) override {
            if (!included_file) {
                return;
            }

            const auto src_file = FilePathForLocation(source_manager, hash_location);
            auto included_path = included_file->getFileEntry().tryGetRealPathName();
            if (included_path.empty()) {
                included_path = included_file->getName();
            }
            auto included_stable_path = StablePathString(included_path.str());
            if (!StringPassesHeaderFilters(src_file) || !StringPassesHeaderFilters(included_stable_path)) return;
            AppendIncludeOrder(owner->include_order, src_file, included_stable_path);
        }

    private:
        ClangHandler* owner;
        const clang::SourceManager& source_manager;
    };

    compiler.getPreprocessor().addPPCallbacks(std::make_unique<IncludeOrderCallback>(this, compiler.getSourceManager()));
    return std::make_unique<Consumer>(this);
}
