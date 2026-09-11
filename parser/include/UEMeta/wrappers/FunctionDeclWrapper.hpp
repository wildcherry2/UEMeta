#pragma once

#include "DeclWrapper.hpp"
#include "boost/hash2/hash_append_fwd.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/QualTypeNames.h"
#include "UEMeta/wrappers/DeclDb.hpp"
#include "UEMeta/wrappers/MessageAllocator.hpp"

namespace UEMeta {
    template<std::derived_from<clang::FunctionDecl> T = clang::FunctionDecl>
    class FunctionDeclWrapper : public DeclWrapper<T> {
    public:
        FunctionDeclWrapper(const T* decl, const boost::local_shared_ptr<google::protobuf::Arena>& arena)
            : DeclWrapper<T>(decl, arena) {}

        using super = DeclWrapper<T>;

        [[nodiscard]] ParserTypes::TLFreeFunctionDeclaration* serialize() const {
            const auto out_msg = google::protobuf::Arena::Create<ParserTypes::TLFreeFunctionDeclaration>(arena.get());
            const std::string fqn = computeFQN();
            super::putMetadata(out_msg->mutable_metadata(), true, fqn, computeDeclId(fqn));
            putFunctionCommon(out_msg->mutable_common());
            return out_msg;
        }
    protected:
        void putFunctionCommon(ParserTypes::FunctionCommon* p_msg) const {
            const auto PutFunctionTypeRef = [this](const clang::QualType declared_type, ParserTypes::TypeRef* p_type_ref) {
                const clang::QualType template_resolved_type = super::resolveTemplatedInstantiation(declared_type);
                const DeclDb::QueryResult type_query = DeclDb::queryType(
                    template_resolved_type.isNull() ? declared_type : template_resolved_type);
                super::putTypeRef(
                    clang::TypeName::getFullyQualifiedName(
                        declared_type, super::getASTContext(), super::getASTContext().getPrintingPolicy(), true),
                    type_query,
                    p_type_ref);
            };

            if (const auto* constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(super::decl)) {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_CONSTRUCTOR);
                SetVersionedBool(p_msg->mutable_is_explicit(), constructor->isExplicit());
            }
            else if (llvm::isa<clang::CXXDestructorDecl>(super::decl)) {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_DESTRUCTOR);
            }
            else if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(super::decl)) {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_MEMBER_CONVERSION);
                SetVersionedBool(p_msg->mutable_is_explicit(), conversion->isExplicit());
            }
            else if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(super::decl)) {
                p_msg->set_kind(method->isStatic()
                                    ? ParserTypes::FUNCTION_KIND_STATIC_MEMBER
                                    : ParserTypes::FUNCTION_KIND_MEMBER);
            }
            else {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_FREE);
            }

            if (!llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl>(super::decl)) {
                ParserTypes::VersionedTypeRef_VersionItem* return_type_version =
                    p_msg->mutable_return_type()->add_versions();
                return_type_version->add_source_versions(Config::GetConfig().Version());
                PutFunctionTypeRef(super::decl->getReturnType(), return_type_version->mutable_value());
            }

            SetVersioned(
                p_msg->mutable_storage_class(),
                super::decl->getStorageClass() == clang::SC_Extern && super::decl->isExternC()
                    ? ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN_C
                : super::decl->getStorageClass() == clang::SC_Extern
                    ? ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN
                : super::decl->isStatic()
                    ? ParserTypes::FUN_VAR_STORAGE_CLASS_STATIC
                    : ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED);

            SetVersioned(
                p_msg->mutable_consteval_kind(),
                super::decl->isConsteval()
                    ? ParserTypes::CONSTANT_EVALUATION_CONSTEVAL
                : super::decl->isConstexpr()
                    ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR
                    : ParserTypes::CONSTANT_EVALUATION_NONE);

            if (super::decl->getFriendObjectKind() != clang::Decl::FOK_None) {
                SetVersionedBool(p_msg->mutable_is_friend(), true);
            }

            if (super::decl->doesThisDeclarationHaveABody()) {
                if (const clang::Stmt* body = super::decl->getBody()) {
                    std::string out;
                    llvm::raw_string_ostream os{out};
                    body->printPretty(os, nullptr, super::getASTContext().getPrintingPolicy());
                    SetVersionedString(p_msg->mutable_inline_definition(), out);
                }
            }

            if (const clang::FunctionTemplateDecl* described_template = super::decl->getDescribedFunctionTemplate()) {
                super::putTemplateDetails(
                    described_template->getTemplateParameters(),
                    p_msg->mutable_template_details());
            }
            else if (const clang::FunctionTemplateDecl* primary_template = super::decl->getPrimaryTemplate()) {
                super::putTemplateDetails(
                    primary_template->getTemplateParameters(),
                    p_msg->mutable_template_details(),
                    DeclDb::queryDeclIdentity(primary_template->getTemplatedDecl()));
            }

            for (const clang::ParmVarDecl* parameter : super::decl->parameters()) {
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
                        default_argument->printPretty(os, nullptr, super::getASTContext().getPrintingPolicy());
                        SetVersionedString(p_parameter->mutable_default_value(), out);
                    }
                }
            }

            if (llvm::isa<clang::CXXMethodDecl>(super::decl) || super::decl->isDefaulted() || super::decl->isDeleted()) {
                p_msg->set_definition_kind(
                    super::decl->isDefaulted()
                        ? ParserTypes::FUNCTION_DEFINITION_DEFAULTED
                    : super::decl->isDeleted()
                        ? ParserTypes::FUNCTION_DEFINITION_DELETED
                        : ParserTypes::FUNCTION_DEFINITION_NORMAL);
            }
        }

    private:
        [[nodiscard]] std::string computeFQN() const {
            std::string out;
            llvm::raw_string_ostream os{out};
            super::putContextFQN(os);

            if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(super::decl)) {
                os << "operator " << clang::TypeName::getFullyQualifiedName(
                    conversion->getConversionType(),
                    super::getASTContext(),
                    super::getASTContext().getPrintingPolicy(),
                    true);
            }
            else {
                super::decl->printName(os, super::getASTContext().getPrintingPolicy());
            }

            return out;
        }

        [[nodiscard]] Hash computeDeclId(std::string_view fqn) const {
            boost::hash2::xxh3_128 hasher;
            boost::hash2::hash_append(hasher, boost::hash2::endian::little, fqn);

            for (const clang::ParmVarDecl* parameter : super::decl->parameters()) {
                const std::string parameter_type = clang::TypeName::getFullyQualifiedName(
                    parameter->getType(),
                    super::getASTContext(),
                    super::getASTContext().getPrintingPolicy(),
                    true);
                hasher.update(parameter_type.data(), parameter_type.size());
            }

            const clang::FunctionTemplateDecl* function_template = super::decl->getDescribedFunctionTemplate();
            if (!function_template) function_template = super::decl->getPrimaryTemplate();
            if (function_template) {
                ParserTypes::TemplateDetails template_details;
                std::vector<AnyString> template_identity;
                super::putTemplateDetails(
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

                if (const clang::TemplateArgumentList* arguments = super::decl->getTemplateSpecializationArgs()) {
                    for (const clang::TemplateArgument& argument : arguments->asArray()) {
                        std::string out;
                        llvm::raw_string_ostream os{out};
                        argument.print(super::getASTContext().getPrintingPolicy(), os, true);
                        boost::hash2::hash_append(hasher, boost::hash2::endian::little, out);
                    }
                }
            }

            return Hash{hasher};
        }
    };
}
