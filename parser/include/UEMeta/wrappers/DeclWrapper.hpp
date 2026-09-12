#pragma once
#include <string>
#include <string_view>

#include "DeclDb.hpp"
#include "MessageAllocator.hpp"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/QualTypeNames.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/DeclTemplate.h"
#include "UEMeta/wrappers/Types.hpp"
#include "boost/smart_ptr/local_shared_ptr.hpp"

namespace UEMeta {
    template<WrapableDecl T>
    class DeclWrapper {
    protected:
        // ReSharper disable once CppNonExplicitConvertingConstructor
        DeclWrapper(const T* decl, const boost::local_shared_ptr<google::protobuf::Arena>& arena) : decl(decl), arena(arena) {}

        void putMetadata(ParserTypes::DeclarationMetadata* metadata, const bool has_identity, std::string_view fqn = "", const Hash& decl_id = {}) const {
            const clang::SourceManager& source_manager = getASTContext().getSourceManager();
            SetVersionedString(metadata->mutable_file_path(), source_manager.getFilename(source_manager.getExpansionLoc(decl->getLocation())));
            if (const clang::RawComment* comment = getASTContext().getRawCommentForAnyRedecl(decl)) {
                SetVersionedString(metadata->mutable_documentation(), comment->getRawText(source_manager));
            }

            SetVersionedInteger(metadata->mutable_occurrence_index(), allocateDeclOccurrence());

            if (has_identity) {
                if (fqn.empty() || fqn == "::") {
                    throw std::runtime_error("Failed to construct FQN!");
                }

                metadata->set_qualified_name(fqn);

                if (decl_id.a == 0 && decl_id.b == 0) {
                    throw std::runtime_error("Invalid type_id!");
                }

                decl_id.putProtoHash(metadata->mutable_decl_id());
            }
            else {
                metadata->set_is_anonymous(true);
            }
        }

        void putContextFQN(llvm::raw_string_ostream& out_stream, const clang::Decl* for_decl = nullptr) const {
            const auto* this_decl_as_decl_context = llvm::dyn_cast_or_null<clang::DeclContext>(for_decl ? for_decl : decl);
            if (!this_decl_as_decl_context) {
                throw std::runtime_error("DeclContext not found!");
            }
            const clang::Decl* decl_context = clang::Decl::castFromDeclContext(this_decl_as_decl_context->getNonTransparentContext());
            const clang::NestedNameSpecifier scope_nns = clang::TypeName::getFullyQualifiedDeclaredContext(
                decl->getASTContext(), decl_context, true);
            if (!scope_nns) {
                throw std::runtime_error("Failed to get scope of anonymous enumerators!");
            }
            scope_nns.print(out_stream, decl->getASTContext().getPrintingPolicy());
        }

        void putTemplateDetails(const clang::TemplateParameterList* declared_params,
                                ParserTypes::TemplateDetails* p_msg,
                                const clang::TemplateArgumentList* specialization_args = nullptr,
                                const DeclDb::QueryResult& primary_template_id = {false},
                                std::vector<AnyString>* id_out_ptr = nullptr) const { // potential optimization: bool template param to prevent AppendOut calls
            const auto AppendOut = [&](const AnyString& str) -> const AnyString& {
                if (id_out_ptr) {
                    id_out_ptr->push_back(str);
                }
                return str;
            };

            if ((!declared_params && !specialization_args) || !p_msg) {
                throw std::invalid_argument("Template parameters are not valid!");
            }

            p_msg->set_specialization_kind(getTemplateSpecializationKind());
            const bool* unresolved_primary = get_if<bool>(&primary_template_id);
            if ((!unresolved_primary || *unresolved_primary)
                && !std::get_if<std::monostate>(&primary_template_id)) {
                putTypeRef(decl->getDeclName().isIdentifier() ? decl->getName().str() : decl->getNameAsString(), primary_template_id, p_msg->mutable_primary_template_decl_id());
            }

            const auto putGenericTypeRef = [](const std::string& type_name, ParserTypes::TypeRef* p_type) {
                if (!type_name.empty()) {
                    SetVersionedString(p_type->mutable_type_name(), type_name);
                }
                p_type->set_is_builtin_or_template(true);
            };

            // recursively parses template params through any nested params
            const auto putParams = [&AppendOut, &putGenericTypeRef, id_out_ptr, this](
                this auto self,
                const clang::TemplateParameterList* params,
                auto* p_details_or_param) {
                if (params->empty()) return;
                AppendOut(std::string_view{"<"});

                for (const clang::NamedDecl* param : *params) {
                    ParserTypes::TemplateParameter* p_param = p_details_or_param->add_parameters();
                    const std::string param_name = param->getDeclName().isIdentifier()
                        ? param->getName().str()
                        : param->getNameAsString();

                    if (const auto* type_param = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
                        p_param->set_kind(type_param->hasTypeConstraint() || type_param->wasDeclaredWithTypename()
                                              ? ParserTypes::TEMPLATE_PARAMETER_KIND_TYPENAME
                                              : ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS);
                        putGenericTypeRef(param_name, p_param->mutable_type());
                        AppendOut(std::string_view{"typename"});
                        if (type_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            AppendOut(std::string_view{"..."});
                        }
                        if (type_param->hasDefaultArgument()) {
                            putDefaultType(type_param->getDefaultArgument().getArgument(), p_param->mutable_default_type());
                        }
                    }
                    else if (const auto* non_type_param = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param)) {
                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_NON_TYPE);
                        if (!param_name.empty()) {
                            SetVersionedString(p_param->mutable_name(), param_name);
                        }
                        putType(non_type_param->getType(), p_param->mutable_type(), id_out_ptr);
                        if (non_type_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            AppendOut(std::string_view{"..."});
                        }
                        if (non_type_param->hasDefaultArgument()) {
                            std::string out;
                            llvm::raw_string_ostream os {out};
                            non_type_param->getDefaultArgument().getArgument().print(
                                decl->getASTContext().getPrintingPolicy(), os, true);
                            SetVersionedString(p_param->mutable_value(), out);
                        }
                    }
                    else if (const auto* template_param = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
                        p_param->set_kind(template_param->wasDeclaredWithTypename()
                                              ? ParserTypes::TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE
                                              : ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS_TEMPLATE);
                        putGenericTypeRef(param_name, p_param->mutable_type());
                        AppendOut(std::string_view{"typename"});
                        if (template_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            AppendOut(std::string_view{"..."});
                        }
                        if (template_param->hasDefaultArgument()) {
                            putDefaultType(
                                template_param->getDefaultArgument().getArgument(),
                                p_param->mutable_default_type());
                        }
                        self(template_param->getTemplateParameters(), p_param);
                    }
                }
                AppendOut(std::string_view{">"});
            };

            if (declared_params) {
                putParams(declared_params, p_msg);
            }

            // handle specializations
            if (specialization_args) {
                const auto printArgument = [this](const clang::TemplateArgument& argument) {
                    std::string out;
                    llvm::raw_string_ostream os{out};
                    argument.print(decl->getASTContext().getPrintingPolicy(), os, true);
                    return out;
                };

                const auto getCarriedGeneric = [](const clang::TemplateArgument& argument) -> const clang::NamedDecl* {
                    const clang::TemplateArgument pattern = argument.isPackExpansion()
                        ? argument.getPackExpansionPattern()
                        : argument;

                    switch (pattern.getKind()) {
                        case clang::TemplateArgument::Type: {
                            const clang::QualType type = pattern.getAsType();
                            if (const auto* type_param = type->getAsCanonical<clang::TemplateTypeParmType>()) {
                                return type_param->getDecl();
                            }
                            return nullptr;
                        }
                        case clang::TemplateArgument::Expression: {
                            const clang::Expr* expression = pattern.getAsExpr()->IgnoreParenImpCasts();
                            if (const auto* decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(expression)) {
                                return llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(decl_ref->getDecl());
                            }
                            return nullptr;
                        }
                        case clang::TemplateArgument::Template:
                        case clang::TemplateArgument::TemplateExpansion:
                            return llvm::dyn_cast_or_null<clang::TemplateTemplateParmDecl>(
                                pattern.getAsTemplateOrTemplatePattern().getAsTemplateDecl());
                        default:
                            return nullptr;
                    }
                };

                enum class SpecializationArgumentKind {
                    Generic,
                    ConcreteType,
                    ConcreteTemplate,
                    ConcreteValue
                };

                const auto classifySpecializationArgument = [&getCarriedGeneric](const clang::TemplateArgument& argument) {
                    if (getCarriedGeneric(argument)) return SpecializationArgumentKind::Generic;
                    if (argument.getKind() == clang::TemplateArgument::Type) {
                        return argument.isDependent()
                            ? SpecializationArgumentKind::Generic
                            : SpecializationArgumentKind::ConcreteType;
                    }
                    if (argument.getKind() == clang::TemplateArgument::Template
                        || argument.getKind() == clang::TemplateArgument::TemplateExpansion) {
                        return argument.isDependent()
                            ? SpecializationArgumentKind::Generic
                            : SpecializationArgumentKind::ConcreteTemplate;
                    }
                    return SpecializationArgumentKind::ConcreteValue;
                };

                const auto putSpecializationArgument =
                    [&AppendOut, &classifySpecializationArgument, &getCarriedGeneric, &printArgument,
                     &putGenericTypeRef, id_out_ptr, this](
                        this auto self,
                        const clang::TemplateArgument& argument,
                        auto add_parameter) -> void {
                    if (argument.getKind() == clang::TemplateArgument::Null) {
                        throw std::runtime_error("Encountered a null template specialization argument!");
                    }

                    if (argument.getKind() == clang::TemplateArgument::Pack) {
                        for (const clang::TemplateArgument& pack_element : argument.pack_elements()) {
                            self(pack_element, add_parameter);
                        }
                        return;
                    }

                    ParserTypes::TemplateParameter* p_param = add_parameter();
                    const SpecializationArgumentKind argument_kind = classifySpecializationArgument(argument);
                    if (argument_kind == SpecializationArgumentKind::Generic) {
                        const clang::NamedDecl* generic = getCarriedGeneric(argument);
                        if (!generic
                            && argument.getKind() != clang::TemplateArgument::Type
                            && argument.getKind() != clang::TemplateArgument::Template
                            && argument.getKind() != clang::TemplateArgument::TemplateExpansion) {
                            throw std::runtime_error("Failed to resolve a carried-over generic argument!");
                        }

                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_GENERIC);
                        const bool is_parameter_pack = argument.isPackExpansion();
                        if (is_parameter_pack) {
                            p_param->set_is_parameter_pack(true);
                        }

                        if (argument.getKind() == clang::TemplateArgument::Type) {
                            const clang::TemplateArgument pattern = argument.isPackExpansion()
                                ? argument.getPackExpansionPattern()
                                : argument;
                            if (generic && !pattern.getAsType().hasQualifiers()) {
                                putGenericTypeRef(generic->getNameAsString(), p_param->mutable_type());
                            }
                            else {
                                const std::string generic_type_name = clang::TypeName::getFullyQualifiedName(
                                    pattern.getAsType(), getASTContext(), getASTContext().getPrintingPolicy(), true);
                                putGenericTypeRef(generic_type_name, p_param->mutable_type());
                            }
                            AppendOut(std::string_view{"typename"});
                            if (is_parameter_pack) AppendOut(std::string_view{"..."});
                            return;
                        }

                        if (generic) {
                            putGenericTypeRef(generic->getNameAsString(), p_param->mutable_type());
                        }
                        else {
                            const clang::TemplateArgument pattern = argument.isPackExpansion()
                                ? argument.getPackExpansionPattern()
                                : argument;
                            const std::string generic_template_name = printArgument(pattern);
                            putGenericTypeRef(generic_template_name, p_param->mutable_type());
                        }
                        AppendOut(std::string_view{"typename"});
                        if (is_parameter_pack) AppendOut(std::string_view{"..."});
                        return;
                    }

                    if (argument_kind == SpecializationArgumentKind::ConcreteType) {
                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
                        const bool is_parameter_pack = argument.isPackExpansion();
                        if (is_parameter_pack) {
                            p_param->set_is_parameter_pack(true);
                        }
                        const clang::TemplateArgument pattern = is_parameter_pack
                            ? argument.getPackExpansionPattern()
                            : argument;
                        putType(pattern.getAsType(), p_param->mutable_type(), id_out_ptr);
                        if (is_parameter_pack) AppendOut(std::string_view{"..."});
                        return;
                    }

                    if (argument_kind == SpecializationArgumentKind::ConcreteTemplate) {
                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TEMPLATE);
                        const bool is_parameter_pack = argument.isPackExpansion();
                        if (is_parameter_pack) {
                            p_param->set_is_parameter_pack(true);
                        }
                        putTemplateRef(argument, p_param->mutable_type(), id_out_ptr);
                        if (is_parameter_pack) AppendOut(std::string_view{"..."});
                        return;
                    }

                    p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE);
                    const bool is_parameter_pack = argument.isPackExpansion();
                    if (is_parameter_pack) {
                        p_param->set_is_parameter_pack(true);
                    }
                    std::string concrete_value = printArgument(argument);
                    SetVersionedString(p_param->mutable_value(), concrete_value);
                    if (is_parameter_pack) {
                        AppendOut(printArgument(argument.getPackExpansionPattern()));
                        AppendOut(std::string_view{"..."});
                    }
                    else {
                        AppendOut(concrete_value);
                    }
                };

                AppendOut(std::string_view{"<"});
                for (const clang::TemplateArgument& argument : specialization_args->asArray()) {
                    putSpecializationArgument(argument, [p_msg] {
                        return p_msg->add_specialized_parameters();
                    });
                }
                AppendOut(std::string_view{">"});
            }
        }

        // Populates a TypeRef with the given type_name and QueryResult
        void putTypeRef(const std::string& type_name, const DeclDb::QueryResult& result, ParserTypes::TypeRef* p_ref) const {
            if (std::get_if<std::monostate>(&result)) {
                throw std::runtime_error("DeclDb query returned std::monostate!");
            }

            SetVersionedString(p_ref->mutable_type_name(), type_name);
            if (const Hash* hash_ptr = get_if<Hash>(&result)) {
                hash_ptr->putProtoHash(p_ref->mutable_decl_id());
            }
            else if (const uint64_t* fwd_ptr = get_if<uint64_t>(&result)) {
                p_ref->set_forward_decl_index(*fwd_ptr);
            }
            else if (const llvm::StringRef* header_ptr = get_if<llvm::StringRef>(&result)) {
                p_ref->set_header(header_ptr->str());
            }
            else if (const bool* implicit_ptr = get_if<bool>(&result)) {
                p_ref->set_is_builtin_or_template(*implicit_ptr);
            }
        }

        // Given a QualType, likely from a variable, field, param, or return value, gets the QualType of the instantiated
        // template, if it exists. This is so fields like `vector<int>` get type ids equivalent to the `vector<T>` decl,
        // rather than a (possibly skipped) generated `vector<int>` implicit specialization
        [[nodiscard]] clang::QualType resolveTemplatedInstantiation(const clang::QualType in) const {
            if (const auto as_record = in->getAsCXXRecordDecl()) {
                if (const auto instantiated_from = as_record->getTemplateInstantiationPattern()) {
                    if (const auto* partial = llvm::dyn_cast<clang::ClassTemplatePartialSpecializationDecl>(instantiated_from)) {
                        return partial->getCanonicalInjectedSpecializationType(getASTContext());
                    }

                    if (const auto* primary = instantiated_from->getDescribedClassTemplate()) {
                        return primary->getCanonicalInjectedSpecializationType(getASTContext());
                    }
                }
            }

            return clang::QualType{};
        }

        [[nodiscard]] clang::ASTContext& getASTContext() const { return decl->getASTContext(); }

        const T* decl;
        boost::local_shared_ptr<google::protobuf::Arena> arena;
    private:
        [[nodiscard]] ParserTypes::TemplateSpecializationKind getTemplateSpecializationKind() const requires TemplateSpecializableDeclType<T> {
            switch (decl->getTemplateSpecializationKind()) {
                case clang::TSK_Undeclared:
                    return ParserTypes::TEMPLATE_SPECIALIZATION_NONE;
                case clang::TSK_ImplicitInstantiation:
                    return ParserTypes::TEMPLATE_SPECIALIZATION_IMPLICIT;
                case clang::TSK_ExplicitSpecialization:
                    return ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT;
                case clang::TSK_ExplicitInstantiationDeclaration:
                    return ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DECLARATION;
                case clang::TSK_ExplicitInstantiationDefinition:
                    return ParserTypes::TEMPLATE_SPECIALIZATION_EXPLICIT_INSTANTIATION_DEFINITION;
                default:
                    throw std::runtime_error("Unknown template specialization kind!");
            }
        }

        [[nodiscard]] ParserTypes::TemplateSpecializationKind getTemplateSpecializationKind() const requires (!TemplateSpecializableDeclType<T>) {
            return ParserTypes::TEMPLATE_SPECIALIZATION_NONE;
        }

        void putTemplateRef(const clang::TemplateArgument& argument,
                            ParserTypes::TypeRef* p_ref,
                            std::vector<AnyString>* id_out_ptr = nullptr) const {
            if (argument.getKind() != clang::TemplateArgument::Template
                && argument.getKind() != clang::TemplateArgument::TemplateExpansion) {
                throw std::invalid_argument("Template argument is not a template name!");
            }

            const clang::TemplateName template_name = argument.getAsTemplateOrTemplatePattern();
            const clang::TemplateDecl* template_decl = template_name.getAsTemplateDecl();
            std::string fqn;
            if (template_decl) {
                {
                    llvm::raw_string_ostream os{fqn};
                    template_decl->printQualifiedName(os, getASTContext().getPrintingPolicy());
                }
                if (!llvm::isa<clang::TemplateTemplateParmDecl>(template_decl)
                    && !fqn.starts_with("::")) {
                    fqn.insert(0, "::");
                }
            }
            else {
                llvm::raw_string_ostream os{fqn};
                template_name.print(os, getASTContext().getPrintingPolicy());
            }

            if (id_out_ptr) id_out_ptr->emplace_back(fqn);
            const DeclDb::QueryResult result = argument.isDependent()
                ? DeclDb::QueryResult{true}
                : DeclDb::queryDeclIdentity(template_decl ? template_decl->getTemplatedDecl() : nullptr);
            putTypeRef(fqn, result, p_ref);
        }

        void putDefaultType(const clang::TemplateArgument& def, ParserTypes::VersionedTypeRef* p_def, std::vector<AnyString>* id_out_ptr = nullptr) const {
            ParserTypes::VersionedTypeRef_VersionItem* p_version = p_def->add_versions();
            p_version->add_source_versions(Config::GetConfig().Version());
            ParserTypes::TypeRef* p_type_ref = p_version->mutable_value();

            if (def.getKind() == clang::TemplateArgument::Type) {
                return putType(def.getAsType(), p_type_ref, id_out_ptr);
            }
            if (def.getKind() == clang::TemplateArgument::Template
                || def.getKind() == clang::TemplateArgument::TemplateExpansion) {
                return putTemplateRef(def, p_type_ref, id_out_ptr);
            }

            std::string out;
            llvm::raw_string_ostream os {out};
            def.print(decl->getASTContext().getPrintingPolicy(), os, true);
            SetVersionedString(p_type_ref->mutable_type_name(), out);
            p_type_ref->set_is_builtin_or_template(true);
        }

        void putType(const clang::QualType type, ParserTypes::TypeRef* p_def, std::vector<AnyString>* id_out_ptr = nullptr) const {
            std::string fqn = clang::TypeName::getFullyQualifiedName(type, getASTContext(), getASTContext().getPrintingPolicy(), true);
            if (id_out_ptr) {
                if (type->isDependentType()) {
                    id_out_ptr->emplace_back(std::string_view{"typename"});
                }
                else {
                    id_out_ptr->emplace_back(fqn);
                }
            }
            const clang::QualType template_resolved_type = resolveTemplatedInstantiation(type);
            const DeclDb::QueryResult result = DeclDb::queryType(
                template_resolved_type.isNull() ? type : template_resolved_type);
            putTypeRef(fqn, result, p_def);
        }
    };
}
