#include "UEMeta/wrappers/VarDeclWrapper.hpp"
#include "clang/AST/DeclTemplate.h"
#include "UEMeta/wrappers/DeclDb.hpp"

void UEMeta::VarDeclWrapper::serialize(const std::filesystem::path &out_dir) const {
    ParserTypes::TLGlobalVariableDeclaration* p_msg = MessageAllocator::GetGlobalVariable();
    const std::string fqn = computeFQN();

    // takes care of DeclarationMetadata, TemplateDetails, and type_ref
    putMetadata(p_msg->mutable_metadata(), true, fqn, computeDeclIdWithTemplateDetailsAndType(fqn, p_msg));

    SetVersioned(p_msg->mutable_storage_class(),
        decl->getTLSKind() != clang::VarDecl::TLS_None ? ParserTypes::VAR_STORAGE_CLASS_THREAD_LOCAL
        : decl->getStorageClass() == clang::SC_Static ? ParserTypes::VAR_STORAGE_CLASS_STATIC
        : decl->getStorageClass() == clang::SC_Extern && decl->isExternC() ? ParserTypes::VAR_STORAGE_CLASS_EXTERN_C
        : decl->getStorageClass() == clang::SC_Extern ? ParserTypes::VAR_STORAGE_CLASS_EXTERN
        : ParserTypes::VAR_STORAGE_CLASS_UNSPECIFIED);

    SetVersioned(p_msg->mutable_constant_evaluation_kind(),
        decl->isConstexpr() ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR : ParserTypes::CONSTANT_EVALUATION_NONE);

    if (const auto* initializer = decl->getInit()) {
        std::string out;
        llvm::raw_string_ostream os(out);
        initializer->printPretty(os, nullptr, getASTContext().getPrintingPolicy());
        SetVersionedString(p_msg->mutable_default_value(), out);
    }
}

std::string UEMeta::VarDeclWrapper::computeFQN() const {
    std::string out{};
    llvm::raw_string_ostream os{out};
    putContextFQN(os);
    os << "::";
    decl->printName(os);
    return out;
}

UEMeta::Hash UEMeta::VarDeclWrapper::computeDeclIdWithTemplateDetailsAndType(std::string_view fqn, ParserTypes::TLGlobalVariableDeclaration *p_msg) const {
    boost::hash2::xxh3_128 hasher;
    hasher.update(fqn.data(), fqn.size());

    {
        const clang::QualType declared_type = decl->getType(); // this should always be what we print for the type's type_name
        const clang::QualType template_resolved_type = resolveTemplatedInstantiation(declared_type); // type of the underlying primary or specialized template, if it exists
        auto type_query = DeclDb::queryType(template_resolved_type.isNull() ? declared_type : template_resolved_type);
        if (get_if<std::monostate>(&type_query)) {
            throw std::runtime_error{"Failed to query global variable type (exception)!"};
        }
        ParserTypes::VersionedTypeRefOrAnon_VersionItem* type_ref_or_anon_version = p_msg->mutable_type_ref()->add_versions();

        type_ref_or_anon_version->add_source_versions(Config::GetConfig().Version());
        ParserTypes::TypeRefOrAnon* type_ref_or_anon = type_ref_or_anon_version->mutable_value();
        // todo if type_query is false and the type is an anonymous record/enum, populate it

        ParserTypes::TypeRef* type_ref = type_ref_or_anon->mutable_type_ref();
        std::string type_name = clang::TypeName::getFullyQualifiedName(declared_type, getASTContext(),
                                                                 getASTContext().getPrintingPolicy(), true);
        putTypeRef(type_name, type_query, type_ref);
        if (decl->getDescribedVarTemplate()) [[unlikely]] {
            hasher.update(type_name.data(), type_name.size());
        }
    }


    if (decl->getDescribedVarTemplate()) [[unlikely]] {
        std::vector<AnyString> out;
        DeclDb::QueryResult resolved_primary_template = decl->getTemplateSpecializationKind() == clang::TSK_Undeclared
            ? DeclDb::QueryResult{false}
            : DeclDb::queryDeclIdentity(decl->getTemplateInstantiationPattern());

        putTemplateDetails(decl->getDescribedVarTemplate()->getTemplateParameters(),
            p_msg->mutable_template_details(),
            resolved_primary_template,
            &out);

        for (AnyString str: out) {
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

bool UEMeta::VarDeclWrapper::computeHasIdentity() const {
    return true;
}
