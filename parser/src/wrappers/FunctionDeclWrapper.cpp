#include "UEMeta/wrappers/FunctionDeclWrapper.hpp"

#include "boost/hash2/hash_append_fwd.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/QualTypeNames.h"
#include "UEMeta/wrappers/DeclDb.hpp"
#include "UEMeta/wrappers/MessageAllocator.hpp"

void UEMeta::FunctionDeclWrapper::serialize(const std::filesystem::path& out_dir, ProtoType* out_msg) const {
    const std::string fqn = computeFQN();
    putMetadata(out_msg->mutable_metadata(), true, fqn, computeDeclId(fqn));
    putFunctionCommon(out_msg->mutable_common());
}

void UEMeta::FunctionDeclWrapper::putFunctionCommon(ParserTypes::FunctionCommon* p_msg) const {
    const auto PutFunctionTypeRef = [this](const clang::QualType declared_type, ParserTypes::TypeRef* p_type_ref) {
        const clang::QualType template_resolved_type = resolveTemplatedInstantiation(declared_type);
        const DeclDb::QueryResult type_query = DeclDb::queryType(
            template_resolved_type.isNull() ? declared_type : template_resolved_type);
        putTypeRef(
            clang::TypeName::getFullyQualifiedName(
                declared_type, getASTContext(), getASTContext().getPrintingPolicy(), true),
            type_query,
            p_type_ref);
    };

    if (const auto* constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(decl)) {
        p_msg->set_kind(ParserTypes::FUNCTION_KIND_CONSTRUCTOR);
        p_msg->set_is_explicit(constructor->isExplicit());
    }
    else if (llvm::isa<clang::CXXDestructorDecl>(decl)) {
        p_msg->set_kind(ParserTypes::FUNCTION_KIND_DESTRUCTOR);
    }
    else if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(decl)) {
        p_msg->set_kind(ParserTypes::FUNCTION_KIND_MEMBER_CONVERSION);
        p_msg->set_is_explicit(conversion->isExplicit());
    }
    else if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
        p_msg->set_kind(method->isStatic()
                            ? ParserTypes::FUNCTION_KIND_STATIC_MEMBER
                            : ParserTypes::FUNCTION_KIND_MEMBER);
    }
    else {
        p_msg->set_kind(ParserTypes::FUNCTION_KIND_FREE);
    }

    if (!llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl>(decl)) {
        ParserTypes::VersionedTypeRef_VersionItem* return_type_version =
            p_msg->mutable_return_type()->add_versions();
        return_type_version->add_source_versions(Config::GetConfig().Version());
        PutFunctionTypeRef(decl->getReturnType(), return_type_version->mutable_value());
    }

    SetVersioned(
        p_msg->mutable_storage_class(),
        decl->getStorageClass() == clang::SC_Extern && decl->isExternC()
            ? ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN_C
        : decl->getStorageClass() == clang::SC_Extern
            ? ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN
        : decl->isStatic()
            ? ParserTypes::FUN_VAR_STORAGE_CLASS_STATIC
            : ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED);

    SetVersioned(
        p_msg->mutable_consteval_kind(),
        decl->isConsteval()
            ? ParserTypes::CONSTANT_EVALUATION_CONSTEVAL
        : decl->isConstexpr()
            ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR
            : ParserTypes::CONSTANT_EVALUATION_NONE);

    if (decl->getFriendObjectKind() != clang::Decl::FOK_None) {
        p_msg->set_is_friend(true);
    }

    if (decl->doesThisDeclarationHaveABody()) {
        if (const clang::Stmt* body = decl->getBody()) {
            std::string out;
            llvm::raw_string_ostream os{out};
            body->printPretty(os, nullptr, getASTContext().getPrintingPolicy());
            SetVersionedString(p_msg->mutable_inline_definition(), out);
        }
    }

    if (const clang::FunctionTemplateDecl* described_template = decl->getDescribedFunctionTemplate()) {
        putTemplateDetails(
            described_template->getTemplateParameters(),
            p_msg->mutable_template_details());
    }
    else if (const clang::FunctionTemplateDecl* primary_template = decl->getPrimaryTemplate()) {
        putTemplateDetails(
            primary_template->getTemplateParameters(),
            p_msg->mutable_template_details(),
            DeclDb::queryDeclIdentity(primary_template->getTemplatedDecl()));
    }

    for (const clang::ParmVarDecl* parameter : decl->parameters()) {
        ParserTypes::Parameter* p_parameter = p_msg->add_parameters();
        if (parameter->getDeclName().isIdentifier()) {
            SetVersionedString(p_parameter->mutable_name(), parameter->getName());
        }
        else {
            SetVersionedString(p_parameter->mutable_name(), parameter->getNameAsString());
        }

        PutFunctionTypeRef(parameter->getType(), p_parameter->mutable_type_ref());

        if (parameter->hasDefaultArg() && !parameter->hasUnparsedDefaultArg()) {
            if (const clang::Expr* default_argument = parameter->getDefaultArg()) {
                std::string out;
                llvm::raw_string_ostream os{out};
                default_argument->printPretty(os, nullptr, getASTContext().getPrintingPolicy());
                SetVersionedString(p_parameter->mutable_default_value(), out);
            }
        }
    }

    if (llvm::isa<clang::CXXMethodDecl>(decl) || decl->isDefaulted() || decl->isDeleted()) {
        p_msg->set_definition_kind(
            decl->isDefaulted()
                ? ParserTypes::FUNCTION_DEFINITION_DEFAULTED
            : decl->isDeleted()
                ? ParserTypes::FUNCTION_DEFINITION_DELETED
                : ParserTypes::FUNCTION_DEFINITION_NORMAL);
    }
}

std::string UEMeta::FunctionDeclWrapper::computeFQN() const {
    std::string out;
    llvm::raw_string_ostream os{out};
    putContextFQN(os);

    if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(decl)) {
        os << "operator " << clang::TypeName::getFullyQualifiedName(
            conversion->getConversionType(),
            getASTContext(),
            getASTContext().getPrintingPolicy(),
            true);
    }
    else {
        decl->printName(os, getASTContext().getPrintingPolicy());
    }

    return out;
}

UEMeta::Hash UEMeta::FunctionDeclWrapper::computeDeclId(const std::string_view fqn) const {
    boost::hash2::xxh3_128 hasher;
    boost::hash2::hash_append(hasher, boost::hash2::endian::little, fqn);

    for (const clang::ParmVarDecl* parameter : decl->parameters()) {
        const std::string parameter_type = clang::TypeName::getFullyQualifiedName(
            parameter->getType(),
            getASTContext(),
            getASTContext().getPrintingPolicy(),
            true);
        hasher.update(parameter_type.data(), parameter_type.size());
    }

    const clang::FunctionTemplateDecl* function_template = decl->getDescribedFunctionTemplate();
    if (!function_template) function_template = decl->getPrimaryTemplate();
    if (function_template) {
        ParserTypes::TemplateDetails template_details;
        std::vector<AnyString> template_identity;
        putTemplateDetails(
            function_template->getTemplateParameters(),
            &template_details,
            DeclDb::QueryResult{false},
            &template_identity);

        for (const AnyString& str : template_identity) {
            if (const auto* ref = get_if<llvm::StringRef>(&str)) {
                hasher.update(ref->data(), ref->size());
            }
            else if (const auto* view = get_if<std::string_view>(&str)) {
                hasher.update(view->data(), view->size());
            }
            else if (const auto* string = get_if<std::string>(&str)) {
                hasher.update(string->data(), string->size());
            }
        }

        if (const clang::TemplateArgumentList* arguments = decl->getTemplateSpecializationArgs()) {
            for (const clang::TemplateArgument& argument : arguments->asArray()) {
                std::string out;
                llvm::raw_string_ostream os{out};
                argument.print(getASTContext().getPrintingPolicy(), os, true);
                boost::hash2::hash_append(hasher, boost::hash2::endian::little, out);
            }
        }
    }

    return Hash{hasher};
}
