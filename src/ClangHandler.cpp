// ReSharper disable CppMemberFunctionMayBeStatic
#include "UEMeta/ClangHandler.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <string_view>

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/GlobalDecl.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecordLayout.h>
#include <clang/AST/VTableBuilder.h>
#include <clang/Basic/ExceptionSpecificationType.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/Specifiers.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/raw_ostream.h>

#include "UEMeta/Cli.hpp"

namespace {
    clang::PrintingPolicy MakePrintingPolicy(const clang::ASTContext& ctx) {
        clang::PrintingPolicy policy{ctx.getLangOpts()};
        policy.adjustForCPlusPlus();
        policy.SuppressScope = false;
        policy.SuppressUnwrittenScope = true;
        policy.SuppressTagKeyword = true;
        policy.ConstantsAsWritten = false;
        return policy;
    }

    std::string PrintType(const clang::ASTContext& ctx, const clang::QualType type, const std::string& placeholder = {}) {
        if (type.isNull()) {
            return "";
        }

        std::string out;
        llvm::raw_string_ostream stream{out};
        type.print(stream, MakePrintingPolicy(ctx), placeholder);
        return stream.str();
    }

    const clang::DeclContext* NonTransparentContext(const clang::DeclContext* context) {
        while (context && context->isTransparentContext()) {
            context = context->getParent();
        }
        return context;
    }

    std::string FullyQualify(std::string value) {
        if (value.empty() || value.starts_with("::")) {
            return value;
        }
        return "::" + value;
    }

    std::string QualifiedName(const clang::NamedDecl* decl) {
        if (!decl || !decl->getDeclName()) {
            return "";
        }
        return FullyQualify(decl->getQualifiedNameAsString());
    }

    std::string AccessToString(const clang::AccessSpecifier access) {
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

    std::string StorageClassToString(const clang::StorageClass storage_class) {
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

    std::string RefQualifierToString(const clang::RefQualifierKind qualifier) {
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

    std::string ExceptionSpecToString(const clang::ExceptionSpecificationType spec) {
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

    bool IsImplicitInstantiation(const clang::TemplateSpecializationKind kind) {
        return kind == clang::TSK_ImplicitInstantiation;
    }

    bool ShouldSkipRecord(const clang::RecordDecl* decl) {
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

    bool ShouldSkipFunction(const clang::FunctionDecl* decl) {
        if (!decl || decl->isImplicit() || decl->isInvalidDecl()) {
            return true;
        }

        if (decl->isFunctionTemplateSpecialization() && IsImplicitInstantiation(decl->getTemplateSpecializationKind())) {
            return true;
        }

        return false;
    }

    bool ShouldSkipVariable(const clang::VarDecl* decl) {
        if (!decl || decl->isImplicit() || decl->isInvalidDecl()) {
            return true;
        }

        if (IsImplicitInstantiation(decl->getTemplateSpecializationKind())) {
            return true;
        }

        return false;
    }

    bool ShouldSkipEnum(const clang::EnumDecl* decl) {
        return !decl || decl->isImplicit() || decl->isInvalidDecl() ||
               IsImplicitInstantiation(decl->getTemplateSpecializationKind());
    }

    bool IsTopLevelNamedDecl(const clang::NamedDecl* decl) {
        if (!decl || decl->isImplicit() || decl->isInvalidDecl() || decl->getParentFunctionOrMethod()) {
            return false;
        }

        const auto* context = NonTransparentContext(decl->getDeclContext());
        return llvm::isa_and_nonnull<clang::TranslationUnitDecl>(context) ||
               llvm::isa_and_nonnull<clang::NamespaceDecl>(context);
    }

    bool IsInSystemHeader(const clang::Decl* decl, const clang::ASTContext& ctx) {
        if (!decl) {
            return true;
        }

        const auto& source_manager = ctx.getSourceManager();
        const auto location = source_manager.getExpansionLoc(decl->getLocation());
        return location.isInvalid() || source_manager.isInSystemHeader(location);
    }

    std::filesystem::path NormalizePath(std::filesystem::path path) {
        std::error_code ec;
        auto normalized = std::filesystem::weakly_canonical(path, ec);
        if (ec) {
            ec.clear();
            normalized = std::filesystem::absolute(path, ec);
        }

        if (ec) {
            normalized = std::move(path);
        }

        normalized = normalized.lexically_normal();
        normalized.make_preferred();
        return normalized;
    }

    std::string DeclFilePath(const clang::Decl* decl, const clang::ASTContext& ctx) {
        if (!decl) {
            return "";
        }

        const auto& source_manager = ctx.getSourceManager();
        const auto location = source_manager.getExpansionLoc(decl->getLocation());
        if (location.isInvalid()) {
            return "";
        }

        if (const auto presumed = source_manager.getPresumedLoc(location); presumed.isValid()) {
            return NormalizePath(presumed.getFilename()).string();
        }

        if (auto file_entry = source_manager.getFileEntryRefForID(source_manager.getFileID(location))) {
            return NormalizePath(file_entry->getName().str()).string();
        }

        return "";
    }

    std::vector<std::string> BuildScope(const clang::NamedDecl* decl) {
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

    void FillCommonDeclaration(UEMeta::JsonDeclaration& out, const clang::NamedDecl* decl, const clang::ASTContext& ctx) {
        out.name = decl->getNameAsString();
        out.qualified_name = QualifiedName(decl);
        out.file = DeclFilePath(decl, ctx);
        out.scope = BuildScope(decl);
        out.is_anonymous = out.name.empty();
    }

    UEMeta::JsonTemplateParameter BuildTemplateParameter(const clang::NamedDecl* param, const clang::ASTContext& ctx);

    std::vector<UEMeta::JsonTemplateParameter> BuildTemplateParameters(const clang::TemplateParameterList* parameters,
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

    UEMeta::JsonTemplateParameter BuildTemplateParameter(const clang::NamedDecl* param, const clang::ASTContext& ctx) {
        UEMeta::JsonTemplateParameter out;
        if (!param) {
            return out;
        }

        out.name = param->getNameAsString();
        out.is_parameter_pack = param->isParameterPack();

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

    std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForRecord(const clang::RecordDecl* decl,
                                                                           const clang::ASTContext& ctx) {
        if (const auto* cxx_record = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(decl)) {
            if (const auto* templ = cxx_record->getDescribedClassTemplate()) {
                return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
            }
        }

        return {};
    }

    std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForFunction(const clang::FunctionDecl* decl,
                                                                             const clang::ASTContext& ctx) {
        if (const auto* templ = decl->getDescribedFunctionTemplate()) {
            return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
        }

        return {};
    }

    std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForAlias(const clang::TypeAliasDecl* decl,
                                                                          const clang::ASTContext& ctx) {
        if (const auto* templ = decl->getDescribedAliasTemplate()) {
            return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
        }

        return {};
    }

    std::vector<UEMeta::JsonTemplateParameter> TemplateParametersForVariable(const clang::VarDecl* decl,
                                                                             const clang::ASTContext& ctx) {
        if (const auto* templ = decl->getDescribedVarTemplate()) {
            return BuildTemplateParameters(templ->getTemplateParameters(), ctx);
        }

        return {};
    }

    std::string EnumValueToString(const clang::EnumConstantDecl* enumerator) {
        llvm::SmallString<32> value;
        enumerator->getInitVal().toString(value, 10);
        return value.str().str();
    }

    std::string RecordKind(const clang::RecordDecl* decl) {
        if (decl->isUnion()) {
            return "union";
        }

        if (decl->isClass()) {
            return "class";
        }

        return "struct";
    }

    std::optional<UEMeta::JsonVTableIndex> BuildVTableIndex(const clang::CXXMethodDecl* method, clang::ASTContext& ctx) {
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

    UEMeta::JsonFunction BuildFunction(const clang::FunctionDecl* decl, clang::ASTContext& ctx) {
        UEMeta::JsonFunction out;
        out.kind = "function";
        out.name = decl->getNameAsString();
        out.qualified_name = QualifiedName(decl);
        out.file = DeclFilePath(decl, ctx);
        out.scope = BuildScope(decl);
        out.return_type = PrintType(ctx, decl->getReturnType());
        out.storage_class = StorageClassToString(decl->getStorageClass());
        out.is_constexpr = decl->isConstexpr();
        out.is_consteval = decl->isConsteval();
        out.is_inline = decl->isInlined() || decl->isInlineSpecified();
        out.is_deleted = decl->isDeleted();
        out.is_defaulted = decl->isDefaulted();
        out.exception_spec = ExceptionSpecToString(decl->getExceptionSpecType());
        out.template_parameters = TemplateParametersForFunction(decl, ctx);

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
            out.parameters.push_back(std::move(json_param));
        }

        return out;
    }

    UEMeta::JsonVariable BuildVariable(const clang::VarDecl* decl, const clang::ASTContext& ctx) {
        UEMeta::JsonVariable out;
        out.name = decl->getNameAsString();
        out.qualified_name = QualifiedName(decl);
        out.file = DeclFilePath(decl, ctx);
        out.scope = BuildScope(decl);
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

    UEMeta::JsonDeclaration BuildEnumDeclaration(const clang::EnumDecl* input, const clang::ASTContext& ctx) {
        const auto* decl = input->getDefinition() ? input->getDefinition() : input;

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
                .file = DeclFilePath(enumerator, ctx),
                .scope = BuildScope(enumerator)
            });
        }

        return out;
    }

    UEMeta::JsonDeclaration BuildAliasDeclaration(const clang::TypeAliasDecl* decl, const clang::ASTContext& ctx) {
        UEMeta::JsonDeclaration out;
        out.kind = "alias";
        FillCommonDeclaration(out, decl, ctx);
        out.template_parameters = TemplateParametersForAlias(decl, ctx);
        out.aliased_type = PrintType(ctx, decl->getUnderlyingType());
        return out;
    }

    UEMeta::JsonDeclaration BuildVariableDeclaration(const clang::VarDecl* decl, const clang::ASTContext& ctx) {
        UEMeta::JsonDeclaration out;
        out.kind = "variable";
        FillCommonDeclaration(out, decl, ctx);
        out.template_parameters = TemplateParametersForVariable(decl, ctx);
        out.variable = BuildVariable(decl, ctx);
        return out;
    }

    UEMeta::JsonDeclaration BuildFunctionDeclaration(const clang::FunctionDecl* decl, clang::ASTContext& ctx) {
        UEMeta::JsonDeclaration out;
        out.kind = "function";
        FillCommonDeclaration(out, decl, ctx);
        out.function = BuildFunction(decl, ctx);
        return out;
    }

    void AppendNestedRecord(const clang::RecordDecl* decl, clang::ASTContext& ctx,
                            llvm::DenseSet<const clang::Decl*>& seen,
                            std::vector<UEMeta::JsonDeclaration>& nested);

    void AppendNestedEnum(const clang::EnumDecl* decl, const clang::ASTContext& ctx,
                          llvm::DenseSet<const clang::Decl*>& seen,
                          std::vector<UEMeta::JsonDeclaration>& nested) {
        if (ShouldSkipEnum(decl)) {
            return;
        }

        const auto* target = decl->getDefinition() ? decl->getDefinition() : decl;
        if (IsInSystemHeader(target, ctx)) {
            return;
        }

        const auto* key = target->getCanonicalDecl();
        if (!seen.insert(key).second) {
            return;
        }

        nested.push_back(BuildEnumDeclaration(target, ctx));
    }

    void AppendNestedAlias(const clang::TypeAliasDecl* decl, const clang::ASTContext& ctx,
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

    const clang::ASTRecordLayout* TryGetRecordLayout(const clang::RecordDecl* decl, const clang::ASTContext& ctx) {
        if (!decl->isCompleteDefinition() || decl->isDependentContext()) {
            return nullptr;
        }

        if (const auto* cxx_record = llvm::dyn_cast<clang::CXXRecordDecl>(decl);
            cxx_record && (!cxx_record->hasDefinition() || cxx_record->hasAnyDependentBases() || !cxx_record->isCompleteDefinition())) {
            return nullptr;
        }

        return &ctx.getASTRecordLayout(decl);
    }

    UEMeta::JsonDeclaration BuildRecordDeclaration(const clang::RecordDecl* input, clang::ASTContext& ctx,
                                                   llvm::DenseSet<const clang::Decl*>& seen) {
        const auto* decl = input->getDefinition() ? input->getDefinition() : input;

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
            json_field.file = DeclFilePath(field, ctx);
            json_field.scope = BuildScope(field);
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
                AppendNestedRecord(class_template->getTemplatedDecl(), ctx, seen, out.nested);
                continue;
            }

            if (const auto* nested_record = llvm::dyn_cast<clang::RecordDecl>(member)) {
                AppendNestedRecord(nested_record, ctx, seen, out.nested);
                continue;
            }

            if (const auto* nested_enum = llvm::dyn_cast<clang::EnumDecl>(member)) {
                AppendNestedEnum(nested_enum, ctx, seen, out.nested);
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

    void AppendNestedRecord(const clang::RecordDecl* decl, clang::ASTContext& ctx,
                            llvm::DenseSet<const clang::Decl*>& seen,
                            std::vector<UEMeta::JsonDeclaration>& nested) {
        if (ShouldSkipRecord(decl)) {
            return;
        }

        const auto* target = decl->getDefinition() ? decl->getDefinition() : decl;
        if (IsInSystemHeader(target, ctx)) {
            return;
        }

        const auto* key = target->getCanonicalDecl();
        if (!seen.insert(key).second) {
            return;
        }

        nested.push_back(BuildRecordDeclaration(target, ctx, seen));
    }

    std::uint64_t StableHash(const std::string_view value) {
        std::uint64_t hash = 14695981039346656037ull;
        for (const auto character : value) {
            hash ^= static_cast<unsigned char>(character);
            hash *= 1099511628211ull;
        }
        return hash;
    }

    std::string SanitizeFileStem(const std::string_view value) {
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

    std::filesystem::path OutputFileForKey(const std::string& label, const std::string& key) {
        const auto stem = SanitizeFileStem(label);
        return UEMeta::Config::GetConfig().OutPath() /
               std::format("{}-{:016x}.json", stem, StableHash(key));
    }

    std::string ParentDirectoryGroup(const std::string& file) {
        const auto file_path = NormalizePath(file);
        const auto file_string = file_path.string();
        auto lowercase_file = file_string;
        std::ranges::transform(lowercase_file, lowercase_file.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        for (const auto& configured_parent : UEMeta::Config::GetConfig().PdPaths()) {
            const auto parent_path = NormalizePath(configured_parent);
            auto parent_string = parent_path.string();
            std::ranges::transform(parent_string, parent_string.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });

            if (!parent_string.ends_with(std::filesystem::path::preferred_separator)) {
                parent_string.push_back(std::filesystem::path::preferred_separator);
            }

            if (lowercase_file.starts_with(parent_string)) {
                return parent_path.string();
            }
        }

        return file;
    }

    bool WriteJsonFile(const std::filesystem::path& path, const std::vector<UEMeta::JsonDeclaration>& declarations) {
        simdjson::builder::string_builder builder{std::max<std::size_t>(1024, declarations.size() * 1024)};
        UEMeta::AppendDeclarationArray(builder, declarations);

        if (!builder.validate_unicode()) {
            std::cerr << std::format("(simdjson) Refusing to write non-UTF8 JSON to \"{}\"", path.string()) << std::endl;
            return false;
        }

        auto view = builder.view();
        if (view.error()) {
            std::cerr << std::format("(simdjson) Failed to build JSON for \"{}\" with error: {}",
                                     path.string(), static_cast<int>(view.error())) << std::endl;
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            std::cerr << std::format("(fs) Failed to create output directory \"{}\": {}",
                                     path.parent_path().string(), ec.message()) << std::endl;
            return false;
        }

        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        if (!out) {
            std::cerr << std::format("(fs) Failed to open output JSON \"{}\"", path.string()) << std::endl;
            return false;
        }

        out << *view;
        return true;
    }
}

bool UEMeta::ClangHandler::shouldVisitTemplateInstantiations() const { return true; }
bool UEMeta::ClangHandler::shouldVisitImplicitCode() const { return false; }
bool UEMeta::ClangHandler::shouldVisitLambdaBody() const { return false; }

void UEMeta::ClangHandler::BeginTranslationUnit(clang::ASTContext& ctx) {
    context = &ctx;
    declarations.clear();
    visited_decls.clear();
}

void UEMeta::ClangHandler::EndTranslationUnit(clang::ASTContext&) {
    switch (Config::GetConfig().SplitStrategy()) {
        case FileSplitStrategy::Monofile: {
            WriteJsonFile(Config::GetConfig().OutPath() / "uemeta.json", declarations);
            break;
        }
        case FileSplitStrategy::ByClass: {
            std::size_t anonymous_index = 0;
            for (const auto& declaration : declarations) {
                const auto key = declaration.qualified_name.empty()
                    ? std::format("{}-anonymous-{}", declaration.kind, anonymous_index++)
                    : declaration.qualified_name;
                const auto label = declaration.qualified_name.empty() ? key : declaration.qualified_name;
                WriteJsonFile(OutputFileForKey(label, key), std::vector{declaration});
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
                WriteJsonFile(OutputFileForKey(label, group_key), group_declarations);
            }
            break;
        }
        case FileSplitStrategy::Default:
        case FileSplitStrategy::ByFile: {
            std::map<std::string, std::vector<JsonDeclaration>> groups;
            for (const auto& declaration : declarations) {
                groups[declaration.file.empty() ? "unknown" : declaration.file].push_back(declaration);
            }

            for (const auto& [file, file_declarations] : groups) {
                const auto filename = std::filesystem::path{file}.filename().string();
                WriteJsonFile(OutputFileForKey(filename.empty() ? file : filename, file), file_declarations);
            }
            break;
        }
    }
}

bool UEMeta::ClangHandler::VisitRecordDecl(clang::RecordDecl* decl) {
    if (!context || ShouldSkipRecord(decl)) {
        return true;
    }

    const auto* target = decl->getDefinition() ? decl->getDefinition() : decl;
    if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
        return true;
    }

    const auto* key = target->getCanonicalDecl();
    if (!visited_decls.insert(key).second) {
        return true;
    }

    declarations.push_back(BuildRecordDeclaration(target, *context, visited_decls));
    return true;
}

bool UEMeta::ClangHandler::VisitClassTemplateDecl(clang::ClassTemplateDecl* decl) {
    return VisitRecordDecl(decl ? decl->getTemplatedDecl() : nullptr);
}

bool UEMeta::ClangHandler::VisitEnumDecl(clang::EnumDecl* decl) {
    if (!context || ShouldSkipEnum(decl)) {
        return true;
    }

    const auto* target = decl->getDefinition() ? decl->getDefinition() : decl;
    if (!IsTopLevelNamedDecl(target) || IsInSystemHeader(target, *context)) {
        return true;
    }

    const auto* key = target->getCanonicalDecl();
    if (!visited_decls.insert(key).second) {
        return true;
    }

    declarations.push_back(BuildEnumDeclaration(target, *context));
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

    declarations.push_back(BuildFunctionDeclaration(target, *context));
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

    declarations.push_back(BuildAliasDeclaration(decl, *context));
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

    declarations.push_back(BuildVariableDeclaration(target, *context));
    return true;
}

bool UEMeta::ClangHandler::VisitVarTemplateDecl(clang::VarTemplateDecl* decl) {
    return VisitVarDecl(decl ? decl->getTemplatedDecl() : nullptr);
}

std::unique_ptr<clang::ASTConsumer> UEMeta::ClangHandler::CreateASTConsumer(clang::CompilerInstance& compiler,
                                                                            llvm::StringRef file) {
    (void)compiler;
    (void)file;

    class Consumer : public clang::ASTConsumer {
    public:
        explicit Consumer(ClangHandler* owner) : owner(owner) {}

        void HandleTranslationUnit(clang::ASTContext& ctx) override {
            owner->BeginTranslationUnit(ctx);
            owner->TraverseDecl(ctx.getTranslationUnitDecl());
            owner->EndTranslationUnit(ctx);
        }

    private:
        ClangHandler* owner;
    };

    return std::make_unique<Consumer>(this);
}
