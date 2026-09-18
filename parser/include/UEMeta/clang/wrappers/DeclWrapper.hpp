#pragma once
#include <fstream>
#include <memory>
#include <string>
#include <string_view>

#include "BS_thread_pool.hpp"
#include "UEMeta/clang/DeclDb.hpp"
#include "TopLevel.pb.h"
#include "UEMeta/utility/DeclUtility.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/QualTypeNames.h"
#include "clang/Basic/SourceManager.h"
#include "google/protobuf/json/json.h"
#include "google/protobuf/util/json_util.h"

namespace UEMeta {
    namespace Detail {
        class DeclWrapperStatics {
        public:
            static uint64_t allocateDeclOccurrence();
            static void     awaitPendingSerializations();

        protected:
            template <TopLevelProto PT>
            static void saveToFile(const PT* msg, const std::shared_ptr<google::protobuf::Arena>& arena) {
                if (!msg)
                    throw std::invalid_argument("Failed to serialize because msg is null!");
                if (!arena)
                    throw std::invalid_argument("Failed to serialize because arena is null!");

                static const auto&                                    cfg       = Config::getConfig();
                static const auto                                     is_json   = cfg.getFormat() == Config::SerializationFormat::Json;
                static const auto                                     open_mode = (is_json ? std::ios::out : std::ios::binary) | std::ios::trunc;
                static constexpr google::protobuf::json::PrintOptions json_options{.add_whitespace                       = true,
                                                                                   .always_print_fields_with_no_presence = true};
                static const std::string_view                         type    = is_json ? "json" : "bin";
                static const auto&                                    out_dir = cfg.getOutputDirectory().getUnderlyingPath();

                auto serialize_fn = [msg, arena] {
                    std::filesystem::path                   out_file_path;
                    const ParserTypes::DeclarationMetadata& metadata = msg->metadata();
                    if (cfg.prefersFullNameInFileName()) {
                        auto name = std::string_view{metadata.qualified_name()} | std::views::split(std::string_view{"::"}) |
                                    std::views::join_with(std::string_view{"."}) | std::ranges::to<std::string>();
                        out_file_path = out_dir / fmtquill::format("{}-{}.{}{}", name, metadata.occurrence_index().versions(0).value(),
                                                                   UEMeta::TOP_LEVEL_EXT<PT>, type);
                    }
                    else {
                        out_file_path = out_dir / fmtquill::format("{}{}-{}.{}{}", metadata.decl_id().a(), metadata.decl_id().b(),
                                                                   metadata.occurrence_index().versions(0).value(), UEMeta::TOP_LEVEL_EXT<PT>, type);
                    }

                    std::ofstream out_file(out_file_path, open_mode);
                    if (!out_file) {
                        throw std::runtime_error("Failed to open file for writing!");
                    }

                    if (is_json) {
                        thread_local std::string buffer{};
                        buffer.clear();
                        if (google::protobuf::util::MessageToJsonString(*msg, &buffer, json_options).ok()) {
                            out_file << buffer;
                        }
                        else {
                            UEM_ERROR("Failed to write to file: {}", out_file_path.string());
                        }
                    }
                    else if (!msg->SerializeToOstream(&out_file)) {
                        UEM_ERROR("Failed to write to file: {}", out_file_path.string());
                    }

                    out_file.close();
                    (void)arena; // ensure the compiler doesn't do any weird optimizations with arena
                };

                if (cfg.syncSerialization()) {
                    serialize_fn();
                }

                else {
                    serialization_pool.detach_task([serialize_fn = std::move(serialize_fn)] {
                        try {
                            serialize_fn();
                        }
                        catch (const std::exception& e) {
                            UEM_ERROR("Failed to toIntermediateRepresentation declaration: {}", e.what());
                        }
                        catch (...) {
                            UEM_ERROR("Failed to toIntermediateRepresentation declaration with unknown exception!");
                        }
                    });
                }
            }

        private:
            static BS::thread_pool<> serialization_pool;
        };
    } // namespace Detail

    template <WrapableDecl T>
    class DeclWrapper : public Detail::DeclWrapperStatics {
    protected:
        // ReSharper disable once CppNonExplicitConvertingConstructor
        DeclWrapper(const T* decl, const std::shared_ptr<google::protobuf::Arena>& arena) : decl(decl), arena(arena) {}

        void putMetadata(ParserTypes::DeclarationMetadata* metadata, const bool has_identity, std::string_view fqn = "",
                         const Hash& decl_id = {}) const {
            const clang::SourceManager& source_manager = getASTContext().getSourceManager();
            setVersionedString(metadata->mutable_file_path(), source_manager.getFilename(source_manager.getExpansionLoc(decl->getLocation())));
            if (const clang::RawComment* comment = getASTContext().getRawCommentForAnyRedecl(decl)) {
                setVersionedString(metadata->mutable_documentation(), comment->getRawText(source_manager));
            }

            setVersioned(metadata->mutable_occurrence_index(), allocateDeclOccurrence());

            if (has_identity) {
                if (fqn.empty() || fqn == "::") {
                    throw DeclException(decl, "Failed to construct FQN!");
                }

                metadata->set_qualified_name(fqn);

                if (decl_id.a == 0 && decl_id.b == 0) {
                    throw DeclException(decl, "Invalid type_id!");
                }

                decl_id.putProtoHash(metadata->mutable_decl_id());
            }
            else {
                metadata->set_is_anonymous(true);
            }
        }

        void putContextFQN(llvm::raw_string_ostream& out_stream, const clang::Decl* for_decl = nullptr) const {
            const clang::Decl* context_owner = for_decl ? for_decl : decl;
            if (!context_owner || !context_owner->getDeclContext()) {
                throw DeclException(decl, "DeclContext not found!");
            }
            // Clang resolves the supplied declaration's context itself. Passing
            // that context would skip a scope (or the translation unit entirely).
            const clang::NestedNameSpecifier scope_nns =
                clang::TypeName::getFullyQualifiedDeclaredContext(decl->getASTContext(), context_owner, true);
            if (!scope_nns) {
                throw DeclException(decl, "Failed to get scope of anonymous enumerators!");
            }
            scope_nns.print(out_stream, decl->getASTContext().getPrintingPolicy());
        }

        void putTemplateDetails(
            const clang::TemplateParameterList* declared_params, ParserTypes::TemplateDetails* p_msg,
            const clang::TemplateArgumentList* specialization_args = nullptr, const DeclDb::QueryResult& primary_template_id = {false},
            std::vector<AnyString>* id_out_ptr = nullptr) const { // potential optimization: bool template param to prevent append_out calls
            const auto append_out = [&](const AnyString& str) -> const AnyString& {
                if (id_out_ptr) {
                    id_out_ptr->push_back(str);
                }
                return str;
            };

            if ((!declared_params && !specialization_args) || !p_msg) {
                throw DeclException(decl, "Template parameters are not valid!");
            }

            p_msg->set_specialization_kind(getTemplateSpecializationKind());
            const bool* unresolved_primary = get_if<bool>(&primary_template_id);
            if ((!unresolved_primary || *unresolved_primary) && !std::get_if<std::monostate>(&primary_template_id)) {
                putTypeRef(decl->getDeclName().isIdentifier() ? decl->getName().str() : decl->getNameAsString(), primary_template_id,
                           p_msg->mutable_primary_template_decl_id());
            }

            const auto put_generic_type_ref = [](const std::string& type_name, ParserTypes::TypeRef* p_type) {
                if (!type_name.empty()) {
                    setVersionedString(p_type->mutable_type_name(), type_name);
                }
                p_type->set_is_builtin_or_template(true);
            };

            // recursively parses template params through any nested params
            const auto put_params = [&append_out, &put_generic_type_ref, id_out_ptr, this](this auto self, const clang::TemplateParameterList* params,
                                                                                           auto* p_details_or_param) {
                if (params->empty())
                    return;
                append_out(std::string_view{"<"});

                for (const clang::NamedDecl* param : *params) {
                    ParserTypes::TemplateParameter* p_param = p_details_or_param->add_parameters();
                    const std::string param_name            = param->getDeclName().isIdentifier() ? param->getName().str() : param->getNameAsString();

                    if (const auto* type_param = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
                        p_param->set_kind(type_param->hasTypeConstraint() || type_param->wasDeclaredWithTypename()
                                              ? ParserTypes::TEMPLATE_PARAMETER_KIND_TYPENAME
                                              : ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS);
                        put_generic_type_ref(param_name, p_param->mutable_type());
                        append_out(std::string_view{"typename"});
                        if (type_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            append_out(std::string_view{"..."});
                        }
                        if (type_param->hasDefaultArgument()) {
                            putDefaultType(type_param->getDefaultArgument().getArgument(), p_param->mutable_default_type());
                        }
                    }
                    else if (const auto* non_type_param = llvm::dyn_cast<clang::NonTypeTemplateParmDecl>(param)) {
                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_NON_TYPE);
                        if (!param_name.empty()) {
                            setVersionedString(p_param->mutable_name(), param_name);
                        }
                        putType(non_type_param->getType(), p_param->mutable_type(), id_out_ptr);
                        if (non_type_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            append_out(std::string_view{"..."});
                        }
                        if (non_type_param->hasDefaultArgument()) {
                            std::string              out;
                            llvm::raw_string_ostream os{out};
                            non_type_param->getDefaultArgument().getArgument().print(decl->getASTContext().getPrintingPolicy(), os, true);
                            setVersionedString(p_param->mutable_value(), out);
                        }
                    }
                    else if (const auto* template_param = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
                        p_param->set_kind(template_param->wasDeclaredWithTypename() ? ParserTypes::TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE
                                                                                    : ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS_TEMPLATE);
                        put_generic_type_ref(param_name, p_param->mutable_type());
                        append_out(std::string_view{"typename"});
                        if (template_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            append_out(std::string_view{"..."});
                        }
                        if (template_param->hasDefaultArgument()) {
                            putDefaultType(template_param->getDefaultArgument().getArgument(), p_param->mutable_default_type());
                        }
                        self(template_param->getTemplateParameters(), p_param);
                    }
                }
                append_out(std::string_view{">"});
            };

            if (declared_params) {
                put_params(declared_params, p_msg);
            }

            // handle specializations
            if (specialization_args) {
                const auto print_argument = [this](const clang::TemplateArgument& argument) {
                    std::string              out;
                    llvm::raw_string_ostream os{out};
                    argument.print(decl->getASTContext().getPrintingPolicy(), os, true);
                    return out;
                };

                const auto get_carried_generic = [](const clang::TemplateArgument& argument) -> const clang::NamedDecl* {
                    const clang::TemplateArgument pattern = argument.isPackExpansion() ? argument.getPackExpansionPattern() : argument;

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

                enum class SpecializationArgumentKind { Generic, ConcreteType, ConcreteTemplate, ConcreteValue };

                const auto classify_specialization_argument = [&get_carried_generic](const clang::TemplateArgument& argument) {
                    if (get_carried_generic(argument))
                        return SpecializationArgumentKind::Generic;
                    if (argument.getKind() == clang::TemplateArgument::Type) {
                        return argument.isDependent() ? SpecializationArgumentKind::Generic : SpecializationArgumentKind::ConcreteType;
                    }
                    if (argument.getKind() == clang::TemplateArgument::Template || argument.getKind() == clang::TemplateArgument::TemplateExpansion) {
                        return argument.isDependent() ? SpecializationArgumentKind::Generic : SpecializationArgumentKind::ConcreteTemplate;
                    }
                    return SpecializationArgumentKind::ConcreteValue;
                };

                const auto put_specialization_argument = [&append_out, &classify_specialization_argument, &get_carried_generic, &print_argument,
                                                          &put_generic_type_ref, id_out_ptr,
                                                          this](this auto self, const clang::TemplateArgument& argument, auto add_parameter) -> void {
                    if (argument.getKind() == clang::TemplateArgument::Null) {
                        throw DeclException(decl, "Encountered a null template specialization argument!");
                    }

                    if (argument.getKind() == clang::TemplateArgument::Pack) {
                        for (const clang::TemplateArgument& pack_element : argument.pack_elements()) {
                            self(pack_element, add_parameter);
                        }
                        return;
                    }

                    ParserTypes::TemplateParameter*  p_param       = add_parameter();
                    const SpecializationArgumentKind argument_kind = classify_specialization_argument(argument);
                    if (argument_kind == SpecializationArgumentKind::Generic) {
                        const clang::NamedDecl* generic = get_carried_generic(argument);
                        if (!generic && argument.getKind() != clang::TemplateArgument::Type &&
                            argument.getKind() != clang::TemplateArgument::Template &&
                            argument.getKind() != clang::TemplateArgument::TemplateExpansion) {
                            throw DeclException(decl, "Failed to resolve a carried-over generic argument!");
                        }

                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_GENERIC);
                        const bool is_parameter_pack = argument.isPackExpansion();
                        if (is_parameter_pack) {
                            p_param->set_is_parameter_pack(true);
                        }

                        if (argument.getKind() == clang::TemplateArgument::Type) {
                            const clang::TemplateArgument pattern = argument.isPackExpansion() ? argument.getPackExpansionPattern() : argument;
                            if (generic && !pattern.getAsType().hasQualifiers()) {
                                put_generic_type_ref(generic->getNameAsString(), p_param->mutable_type());
                            }
                            else {
                                const std::string generic_type_name = clang::TypeName::getFullyQualifiedName(
                                    pattern.getAsType(), getASTContext(), getASTContext().getPrintingPolicy(), true);
                                put_generic_type_ref(generic_type_name, p_param->mutable_type());
                            }
                            append_out(std::string_view{"typename"});
                            if (is_parameter_pack)
                                append_out(std::string_view{"..."});
                            return;
                        }

                        if (generic) {
                            put_generic_type_ref(generic->getNameAsString(), p_param->mutable_type());
                        }
                        else {
                            const clang::TemplateArgument pattern = argument.isPackExpansion() ? argument.getPackExpansionPattern() : argument;
                            const std::string             generic_template_name = print_argument(pattern);
                            put_generic_type_ref(generic_template_name, p_param->mutable_type());
                        }
                        append_out(std::string_view{"typename"});
                        if (is_parameter_pack)
                            append_out(std::string_view{"..."});
                        return;
                    }

                    if (argument_kind == SpecializationArgumentKind::ConcreteType) {
                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TYPE);
                        const bool is_parameter_pack = argument.isPackExpansion();
                        if (is_parameter_pack) {
                            p_param->set_is_parameter_pack(true);
                        }
                        const clang::TemplateArgument pattern = is_parameter_pack ? argument.getPackExpansionPattern() : argument;
                        putType(pattern.getAsType(), p_param->mutable_type(), id_out_ptr);
                        if (is_parameter_pack)
                            append_out(std::string_view{"..."});
                        return;
                    }

                    if (argument_kind == SpecializationArgumentKind::ConcreteTemplate) {
                        p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_TEMPLATE);
                        const bool is_parameter_pack = argument.isPackExpansion();
                        if (is_parameter_pack) {
                            p_param->set_is_parameter_pack(true);
                        }
                        putTemplateRef(argument, p_param->mutable_type(), id_out_ptr);
                        if (is_parameter_pack)
                            append_out(std::string_view{"..."});
                        return;
                    }

                    p_param->set_kind(ParserTypes::TEMPLATE_PARAMETER_KIND_SPEC_CONCRETE_VALUE);
                    const bool is_parameter_pack = argument.isPackExpansion();
                    if (is_parameter_pack) {
                        p_param->set_is_parameter_pack(true);
                    }
                    std::string concrete_value = print_argument(argument);
                    setVersionedString(p_param->mutable_value(), concrete_value);
                    if (is_parameter_pack) {
                        append_out(print_argument(argument.getPackExpansionPattern()));
                        append_out(std::string_view{"..."});
                    }
                    else {
                        append_out(concrete_value);
                    }
                };

                append_out(std::string_view{"<"});
                for (const clang::TemplateArgument& argument : specialization_args->asArray()) {
                    put_specialization_argument(argument, [p_msg] { return p_msg->add_specialized_parameters(); });
                }
                append_out(std::string_view{">"});
            }
        }

        // Populates a TypeRef with the given type_name and QueryResult
        void putTypeRef(const std::string& type_name, const DeclDb::QueryResult& result, ParserTypes::TypeRef* p_ref) const {
            if (std::get_if<std::monostate>(&result)) {
                throw DeclException(decl, "DeclDb query returned std::monostate!");
            }

            setVersionedString(p_ref->mutable_type_name(), type_name);
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

        [[nodiscard]] clang::ASTContext& getASTContext() const { return decl->getASTContext(); }

        const T*                                 decl;
        std::shared_ptr<google::protobuf::Arena> arena;

    private:
        [[nodiscard]] ParserTypes::TemplateSpecializationKind getTemplateSpecializationKind() const {
            // Record wrappers accept C records too, so recover C++ template information dynamically.
            clang::TemplateSpecializationKind kind = clang::TSK_Undeclared;
            if constexpr (std::same_as<T, clang::RecordDecl>) {
                if (const auto* cxx = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
                    kind = cxx->getTemplateSpecializationKind();
                }
            }
            else if constexpr (TemplateSpecializableDeclType<T>) {
                kind = decl->getTemplateSpecializationKind();
            }

            switch (kind) {
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
                    throw DeclException(decl, "Unknown template specialization kind!");
            }
        }

        void putTemplateRef(const clang::TemplateArgument& argument, ParserTypes::TypeRef* p_ref,
                            std::vector<AnyString>* id_out_ptr = nullptr) const {
            if (argument.getKind() != clang::TemplateArgument::Template && argument.getKind() != clang::TemplateArgument::TemplateExpansion) {
                throw DeclException(decl, "Template argument is not a template name!");
            }

            const clang::TemplateName  template_name = argument.getAsTemplateOrTemplatePattern();
            const clang::TemplateDecl* template_decl = template_name.getAsTemplateDecl();
            std::string                fqn;
            if (template_decl) {
                {
                    llvm::raw_string_ostream os{fqn};
                    template_decl->printQualifiedName(os, getASTContext().getPrintingPolicy());
                }
                if (!llvm::isa<clang::TemplateTemplateParmDecl>(template_decl) && !fqn.starts_with("::")) {
                    fqn.insert(0, "::");
                }
            }
            else {
                llvm::raw_string_ostream os{fqn};
                template_name.print(os, getASTContext().getPrintingPolicy());
            }

            if (id_out_ptr)
                id_out_ptr->emplace_back(fqn);
            const DeclDb::QueryResult result = argument.isDependent()
                                                   ? DeclDb::QueryResult{true}
                                                   : DeclDb::queryDeclIdentity(template_decl ? template_decl->getTemplatedDecl() : nullptr);
            putTypeRef(fqn, result, p_ref);
        }

        void putDefaultType(const clang::TemplateArgument& def, ParserTypes::VersionedTypeRef* p_def,
                            std::vector<AnyString>* id_out_ptr = nullptr) const {
            ParserTypes::VersionedTypeRef_VersionItem* p_version = p_def->add_versions();
            p_version->add_source_versions(Config::getConfig().getVersion());
            ParserTypes::TypeRef* p_type_ref = p_version->mutable_value();

            if (def.getKind() == clang::TemplateArgument::Type) {
                return putType(def.getAsType(), p_type_ref, id_out_ptr);
            }
            if (def.getKind() == clang::TemplateArgument::Template || def.getKind() == clang::TemplateArgument::TemplateExpansion) {
                return putTemplateRef(def, p_type_ref, id_out_ptr);
            }

            std::string              out;
            llvm::raw_string_ostream os{out};
            def.print(decl->getASTContext().getPrintingPolicy(), os, true);
            setVersionedString(p_type_ref->mutable_type_name(), out);
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
            // DeclDb resolves generated instantiations to the source template/specialization declaration.
            const DeclDb::QueryResult result = DeclDb::queryType(type);
            putTypeRef(fqn, result, p_def);
        }
    };
} // namespace UEMeta
