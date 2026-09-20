#include "UEMeta/clang/wrappers/VarDeclWrapper.hpp"
#include "UEMeta/clang/DeclDb.hpp"
#include "UEMeta/clang/wrappers/EnumDeclWrapper.hpp"
#include "UEMeta/clang/wrappers/RecordDeclWrapper.hpp"
#include "UEMeta/utility/DeclException.hpp"
#include "clang/AST/DeclTemplate.h"

ParserTypes::TLGlobalVariableDeclaration* UEMeta::VarDeclWrapper::toIntermediateRepresentation() const {
    const auto        out_msg = google::protobuf::Arena::Create<ParserTypes::TLGlobalVariableDeclaration>(arena.get());
    const std::string fqn     = computeFQN();

    // takes care of DeclarationMetadata, TemplateDetails, and type_ref
    putMetadata(out_msg->mutable_metadata(), true, fqn, computeDeclIdWithTemplateDetailsAndType(fqn, out_msg));

    setVersioned(out_msg->mutable_storage_class(), decl->getTLSKind() != clang::VarDecl::TLS_None ? ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL
                                                   : decl->getStorageClass() == clang::SC_Static  ? ParserTypes::VAR_STORAGE_CLASS_STATIC
                                                   : decl->getStorageClass() == clang::SC_Extern && decl->isExternC()
                                                       ? ParserTypes::VAR_STORAGE_CLASS_EXTERN_C
                                                   : decl->getStorageClass() == clang::SC_Extern ? ParserTypes::VAR_STORAGE_CLASS_EXTERN
                                                                                                 : ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED);

    setVersioned(out_msg->mutable_constant_evaluation_kind(),
                 decl->isConstexpr() ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR : ParserTypes::CONSTANT_EVALUATION_NONE);

    if (const auto* initializer = decl->getInit()) {
        std::string              out;
        llvm::raw_string_ostream os(out);
        initializer->printPretty(os, nullptr, getASTContext().getPrintingPolicy());
        setVersionedString(out_msg->mutable_default_value(), out);
    }

    return out_msg;
}

void UEMeta::VarDeclWrapper::toFile() const { return toFile(toIntermediateRepresentation(), arena); }

void UEMeta::VarDeclWrapper::toFile(const ParserTypes::TLGlobalVariableDeclaration* ir, const std::shared_ptr<google::protobuf::Arena>& arena) {
    saveToFile(ir, arena);
}

void UEMeta::VarDeclWrapper::toString(const ParserTypes::TLGlobalVariableDeclaration* ir, std::string& out) {
    saveToString(ir, out);
}

std::string UEMeta::VarDeclWrapper::computeFQN() const {
    std::string              out{};
    llvm::raw_string_ostream os{out};
    putContextFQN(os);
    decl->printName(os);
    return out;
}

UEMeta::Hash UEMeta::VarDeclWrapper::computeDeclIdWithTemplateDetailsAndType(std::string_view                          fqn,
                                                                             ParserTypes::TLGlobalVariableDeclaration* p_msg) const {
    boost::hash2::xxh3_128 hasher;
    hasher.update(fqn.data(), fqn.size());

    const clang::VarTemplateDecl* described_template     = decl->getDescribedVarTemplate();
    const auto*                   specialization         = llvm::dyn_cast<clang::VarTemplateSpecializationDecl>(decl);
    const auto*                   partial_specialization = llvm::dyn_cast<clang::VarTemplatePartialSpecializationDecl>(decl);

    {
        const clang::QualType declared_type = decl->getType(); // this should always be what we print for the type's type_name
        // Let DeclDb resolve instantiations to their source template/specialization declaration.
        clang::QualType underlying;
        const auto      type_query = DeclDb::queryType(declared_type, &underlying);
        if (get_if<std::monostate>(&type_query)) {
            throw DeclException(decl, "Failed to query global variable type (exception)!");
        }
        ParserTypes::VersionedTypeRefOrAnon_VersionItem* type_ref_or_anon_version = p_msg->mutable_type_ref()->add_versions();

        type_ref_or_anon_version->add_source_versions(Config::getConfig().getVersion());
        ParserTypes::TypeRefOrAnon* type_ref_or_anon = type_ref_or_anon_version->mutable_value();

        // Use DeclDb's unwrapped type to embed unnamed definitions in the variable's arena.
        if (auto* tag = underlying.isNull() ? nullptr : underlying->getAsTagDecl();
            tag && !tag->hasNameForLinkage() && tag->isEmbeddedInDeclarator() && !tag->isFreeStanding()) {
            if (auto* record = llvm::dyn_cast_or_null<clang::RecordDecl>(tag->getDefinition())) {
                DeclDb::addDeclarationAsVisited(record);
                const auto  result = RecordDeclWrapper(record, arena).toIntermediateRepresentation();
                const auto* nested = std::get_if<ParserTypes::TLRecordDeclaration*>(&result);
                if (!nested)
                    throw DeclException(decl, "An embedded anonymous record did not produce a record!");
                type_ref_or_anon->set_allocated_anon_record(*nested);
            }
            else if (auto* enumeration = llvm::dyn_cast_or_null<clang::EnumDecl>(tag->getDefinition())) {
                DeclDb::addDeclarationAsVisited(enumeration);
                const auto  result = EnumDeclWrapper(enumeration, arena).toIntermediateRepresentation();
                const auto* nested = std::get_if<ParserTypes::TLEnumDeclaration*>(&result);
                if (!nested)
                    throw DeclException(decl, "An embedded anonymous enum did not produce an enum!");
                type_ref_or_anon->set_allocated_anon_enum(*nested);
            }
        }

        std::string type_name = clang::TypeName::getFullyQualifiedName(declared_type, getASTContext(), getASTContext().getPrintingPolicy(), true);
        if (type_ref_or_anon->value_case() == ParserTypes::TypeRefOrAnon::VALUE_NOT_SET) {
            putTypeRef(type_name, type_query, type_ref_or_anon->mutable_type_ref());
        }
        if (described_template || specialization) [[unlikely]] {
            hasher.update(type_name.data(), type_name.size());
        }
    }

    const clang::TemplateParameterList* declared_params     = described_template       ? described_template->getTemplateParameters()
                                                              : partial_specialization ? partial_specialization->getTemplateParameters()
                                                                                       : nullptr;
    const clang::TemplateArgumentList*  specialization_args = specialization ? &specialization->getTemplateArgs() : nullptr;

    if (declared_params || specialization_args) [[unlikely]] {
        std::vector<AnyString>    out;
        const DeclDb::QueryResult resolved_primary_template =
            specialization ? DeclDb::queryDeclIdentity(specialization->getSpecializedTemplate()->getTemplatedDecl()) : DeclDb::QueryResult{false};

        putTemplateDetails(declared_params, p_msg->mutable_template_details(), specialization_args, resolved_primary_template, &out);

        for (AnyString str : out) {
            if (const auto* ref = get_if<llvm::StringRef>(&str)) {
                hasher.update(ref->data(), ref->size());
            }
            else if (const auto* sv = get_if<std::string_view>(&str)) {
                hasher.update(sv->data(), sv->size());
            }
            else if (const auto* string = get_if<std::string>(&str)) {
                hasher.update(string->data(), string->size());
            }
        }
    }

    return Hash{hasher};
}
