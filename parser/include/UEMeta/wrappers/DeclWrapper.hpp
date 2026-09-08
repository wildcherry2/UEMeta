#pragma once
#include <filesystem>
#include <string>
#include <string_view>

#include "DeclDb.hpp"
#include "MessageAllocator.hpp"
#include "TopLevel.pb.h"
#include "clang/AST/Decl.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/QualTypeNames.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/DeclTemplate.h"
#include "UEMeta/wrappers/Types.hpp"

namespace UEMeta {
    template<WrapableDecl T>
    class DeclWrapper {
    public:
        virtual ~DeclWrapper() noexcept = default;
// update to take in optional T*? qne make out_dir optional?
        virtual void serialize(const std::filesystem::path &out_dir) const = 0;
    protected:
        // ReSharper disable once CppNonExplicitConvertingConstructor
        DeclWrapper(const T* decl) : decl(decl) {}

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

        void putTemplateDetails(const clang::TemplateParameterList* params,
                                ParserTypes::TemplateDetails* p_msg,
                                const DeclDb::QueryResult& primary_template_id = {false},
                                std::vector<AnyString>* id_out_ptr = nullptr) const { // potential optimization: bool template param to prevent AppendOut calls
            const auto AppendOut = [&](const AnyString& str) -> const AnyString& {
                if (id_out_ptr) {
                    id_out_ptr->push_back(str);
                }
                return str;
            };

            if (!params || !p_msg) throw std::invalid_argument("Parameters are not valid!");

            p_msg->set_specialization_kind(getTemplateSpecializationKind());
            const bool* unresolved_primary = get_if<bool>(&primary_template_id);
            if ((!unresolved_primary || *unresolved_primary)
                && !std::get_if<std::monostate>(&primary_template_id)) {
                putTypeRef(decl->getDeclName().isIdentifier() ? decl->getName().str() : decl->getNameAsString(), primary_template_id, p_msg->mutable_primary_template_decl_id());
            }

            auto putParams = [&AppendOut, id_out_ptr, this](this auto self, const clang::TemplateParameterList* params, auto* p_details_or_param) {
                if (params->empty()) return;
                AppendOut(std::string_view{"<"});

                for (const clang::NamedDecl* param : *params) {
                    ParserTypes::TemplateParameter* p_param = p_details_or_param->add_parameters();
                    if (param->getDeclName().isIdentifier()) {
                        SetVersionedString(p_param->mutable_name(), param->getName());
                    }
                    else {
                        SetVersionedString(p_param->mutable_name(), param->getNameAsString());
                    }

                    if (const auto* type_param = llvm::dyn_cast<clang::TemplateTypeParmDecl>(param)) {
                        p_param->set_kind(type_param->wasDeclaredWithTypename()
                                              ? ParserTypes::TEMPLATE_PARAMETER_KIND_TYPENAME
                                              : ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS);
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
                        putType(non_type_param->getType(), p_param->mutable_type(), id_out_ptr);
                        if (non_type_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            AppendOut(std::string_view{"..."});
                        }
                        if (non_type_param->hasDefaultArgument()) {
                            llvm::raw_string_ostream os {*p_param->mutable_default_value()};
                            non_type_param->getDefaultArgument().getArgument().print(decl->getASTContext().getPrintingPolicy(), os, true);
                        }
                    }
                    else if (const auto* template_param = llvm::dyn_cast<clang::TemplateTemplateParmDecl>(param)) {
                        p_param->set_kind(template_param->wasDeclaredWithTypename()
                                              ? ParserTypes::TEMPLATE_PARAMETER_KIND_TYPENAME_TEMPLATE
                                              : ParserTypes::TEMPLATE_PARAMETER_KIND_CLASS_TEMPLATE);
                        AppendOut(std::string_view{"typename"});
                        if (template_param->isParameterPack()) {
                            p_param->set_is_parameter_pack(true);
                            AppendOut(std::string_view{"..."});
                        }
                        if (template_param->hasDefaultArgument()) {
                            putDefaultType(template_param->getDefaultArgument().getArgument(), p_param->mutable_default_type());
                        }
                        self(template_param->getTemplateParameters(), p_param);
                    }
                }
                AppendOut(std::string_view{">"});
            };
            putParams(params, p_msg);
        }

        // Populates a TypeRef with the given type_name and QueryResult
        void putTypeRef(const std::string& type_name, const DeclDb::QueryResult& result, ParserTypes::TypeRef* p_ref) const {
            if (std::get_if<std::monostate>(&result)) {
                throw std::runtime_error("DeclDb::queryType() returned std::monostate!");
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
    private:
        ParserTypes::TemplateSpecializationKind getTemplateSpecializationKind() const requires TemplateSpecializableDeclType<T> {
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

        ParserTypes::TemplateSpecializationKind getTemplateSpecializationKind() const requires (!TemplateSpecializableDeclType<T>) {
            return ParserTypes::TEMPLATE_SPECIALIZATION_NONE;
        }

        void putDefaultType(const clang::TemplateArgument& def, ParserTypes::TypeRef* p_def, std::vector<AnyString>* id_out_ptr = nullptr) const {
            if (def.getKind() == clang::TemplateArgument::Type) {
                return putType(def.getAsType(), p_def, id_out_ptr);
            }

            // we only call this when we're dealing with type and template params,
            // so if it's not a concrete type we'll just label it as a template dependent type
            std::string out;
            llvm::raw_string_ostream os {out};
            def.print(decl->getASTContext().getPrintingPolicy(), os, true);
            SetVersionedString(p_def->mutable_type_name(), out);
            p_def->set_is_builtin_or_template(true);
        }

        void putType(const clang::QualType type, ParserTypes::TypeRef* p_def, std::vector<AnyString>* id_out_ptr = nullptr) const {
            std::string fqn = clang::TypeName::getFullyQualifiedName(type, getASTContext(), getASTContext().getPrintingPolicy(), true);
            if (id_out_ptr) id_out_ptr->emplace_back(fqn);
            const clang::QualType template_resolved_type = resolveTemplatedInstantiation(type);
            const DeclDb::QueryResult result = DeclDb::queryType(
                template_resolved_type.isNull() ? type : template_resolved_type);
            putTypeRef(fqn, result, p_def);
        }
    };
}
