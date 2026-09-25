#pragma once

#include "DeclWrapper.hpp"
#include "UEMeta/clang/DeclDb.hpp"
#include "UEMeta/utility/DeclUtility.hpp"
#include "boost/hash2/hash_append.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/QualTypeNames.h"

namespace UEMeta {
    template <std::derived_from<clang::FunctionDecl> T = clang::FunctionDecl>
    class FunctionDeclWrapper : public DeclWrapper<T> {
    public:
        FunctionDeclWrapper(const T* decl, const std::shared_ptr<google::protobuf::Arena>& arena) : DeclWrapper<T>(decl, arena) {}

        using Super = DeclWrapper<T>;
        using IntermediateRepresentation = ParserTypes::TLFreeFunctionDeclaration*;

        [[nodiscard]] ParserTypes::TLFreeFunctionDeclaration* toIntermediateRepresentation() const {
            const auto        out_msg = google::protobuf::Arena::Create<ParserTypes::TLFreeFunctionDeclaration>(Super::arena.get());
            const std::string fqn     = computeFQN();
            const Hash        decl_id = computeDeclIdWithTemplateDetails(fqn, out_msg->mutable_common());
            Super::putMetadata(out_msg->mutable_metadata(), true, fqn, decl_id);
            if (!llvm::isa<clang::CXXMethodDecl>(Super::decl)) {
                DeclDb::addDeclIdentity(Super::decl, decl_id);
            }
            putFunctionCommon(out_msg->mutable_common());
            return out_msg;
        }

        void toFile() const { return toFile(toIntermediateRepresentation(), Super::arena); }

        static void toFile(const ParserTypes::TLFreeFunctionDeclaration* ir, const std::shared_ptr<google::protobuf::Arena>& arena) {
            Detail::DeclWrapperStatics::saveToFile(ir, arena);
        }

        static void toString(const IntermediateRepresentation& ir, std::string& out) {
            Detail::DeclWrapperStatics::saveToString(ir, out);
        }

    protected:
        void putFunctionCommon(ParserTypes::FunctionCommon* p_msg) const {
            if (const auto* constructor = llvm::dyn_cast<clang::CXXConstructorDecl>(Super::decl)) {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_CONSTRUCTOR);
                setVersionedBool(p_msg->mutable_is_explicit(), constructor->isExplicit());
            }
            else if (llvm::isa<clang::CXXDestructorDecl>(Super::decl)) {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_DESTRUCTOR);
            }
            else if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(Super::decl)) {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_MEMBER_CONVERSION);
                setVersionedBool(p_msg->mutable_is_explicit(), conversion->isExplicit());
            }
            else if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(Super::decl)) {
                p_msg->set_kind(method->isStatic() ? ParserTypes::FUNCTION_KIND_STATIC_MEMBER : ParserTypes::FUNCTION_KIND_MEMBER);
            }
            else {
                p_msg->set_kind(ParserTypes::FUNCTION_KIND_FREE);
            }

            if (!llvm::isa<clang::CXXConstructorDecl, clang::CXXDestructorDecl>(Super::decl)) {
                putFunctionTypeRef(Super::decl->getReturnType(), p_msg->mutable_return_type());
            }

            setVersioned(p_msg->mutable_storage_class(),
                         Super::decl->getStorageClass() == clang::SC_Extern && Super::decl->isExternC() ? ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN_C
                         : Super::decl->getStorageClass() == clang::SC_Extern                           ? ParserTypes::FUN_VAR_STORAGE_CLASS_EXTERN
                         : Super::decl->isStatic()                                                      ? ParserTypes::FUN_VAR_STORAGE_CLASS_STATIC
                                                   : ParserTypes::FUN_VAR_STORAGE_CLASS_UNSPECIFIED);

            setVersioned(p_msg->mutable_consteval_kind(), Super::decl->isConsteval()   ? ParserTypes::CONSTANT_EVALUATION_CONSTEVAL
                                                          : Super::decl->isConstexpr() ? ParserTypes::CONSTANT_EVALUATION_CONSTEXPR
                                                                                       : ParserTypes::CONSTANT_EVALUATION_NONE);

            if (Super::decl->getFriendObjectKind() != clang::Decl::FOK_None) {
                setVersionedBool(p_msg->mutable_is_friend(), true);
            }

            if (Super::decl->doesThisDeclarationHaveABody()) {
                if (const clang::Stmt* body = Super::decl->getBody()) {
                    std::string              out;
                    llvm::raw_string_ostream os{out};
                    body->printPretty(os, nullptr, Super::getASTContext().getPrintingPolicy());
                    setVersionedString(p_msg->mutable_inline_definition(), out);
                }
            }

            for (const clang::ParmVarDecl* parameter : Super::decl->parameters()) {
                ParserTypes::Parameter* p_parameter = p_msg->add_parameters();
                if (parameter->getDeclName().isIdentifier()) {
                    setVersionedString(p_parameter->mutable_name(), parameter->getName());
                }
                else {
                    setVersionedString(p_parameter->mutable_name(), parameter->getNameAsString());
                }

                putFunctionTypeRef(parameter->getType(), p_parameter->mutable_type_ref());

                if (parameter->hasDefaultArg() && !parameter->hasUnparsedDefaultArg()) {
                    if (const clang::Expr* default_argument = parameter->getDefaultArg()) {
                        std::string              out;
                        llvm::raw_string_ostream os{out};
                        default_argument->printPretty(os, nullptr, Super::getASTContext().getPrintingPolicy());
                        setVersionedString(p_parameter->mutable_default_value(), out);
                    }
                }
            }

            if (llvm::isa<clang::CXXMethodDecl>(Super::decl) || Super::decl->isDefaulted() || Super::decl->isDeleted()) {
                setVersioned(p_msg->mutable_definition_kind(), Super::decl->isDefaulted() ? ParserTypes::FUNCTION_DEFINITION_DEFAULTED
                                                                   : Super::decl->isDeleted() ? ParserTypes::FUNCTION_DEFINITION_DELETED
                                                                   : ParserTypes::FUNCTION_DEFINITION_NORMAL);
            }
        }

        // Keep name and identity construction available to the member-function wrapper.
        [[nodiscard]] std::string computeFQN() const {
            std::string              out;
            llvm::raw_string_ostream os{out};
            Super::putContextFQN(os);
            os << computeName();
            return out;
        }

        // Conversion operators need a fully qualified target type in their name.
        [[nodiscard]] std::string computeName() const {
            std::string              out;
            llvm::raw_string_ostream os{out};
            if (const auto* conversion = llvm::dyn_cast<clang::CXXConversionDecl>(Super::decl)) {
                os << "operator "
                   << clang::TypeName::getFullyQualifiedName(conversion->getConversionType().getCanonicalType(), Super::getASTContext(),
                                                             Super::getASTContext().getPrintingPolicy(), true);
            }
            else {
                Super::decl->printName(os, Super::getASTContext().getPrintingPolicy());
            }

            return out;
        }

        [[nodiscard]] Hash computeDeclIdWithTemplateDetails(std::string_view fqn, ParserTypes::FunctionCommon* p_msg) const {
            boost::hash2::xxh3_128 hasher;
            boost::hash2::hash_append(hasher, boost::hash2::little_endian_flavor{}, fqn);

            for (const clang::ParmVarDecl* parameter : Super::decl->parameters()) {
                const std::string parameter_type = clang::TypeName::getFullyQualifiedName(parameter->getType(), Super::getASTContext(),
                                                                                          Super::getASTContext().getPrintingPolicy(), true);
                hasher.update(parameter_type.data(), parameter_type.size());
            }

            // cv/ref qualifiers distinguish otherwise identical member overloads.
            if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(Super::decl)) {
                if (method->isConst())
                    hasher.update(" const", 6);
                if (method->isVolatile())
                    hasher.update(" volatile", 9);
                if (method->getRefQualifier() == clang::RQ_LValue)
                    hasher.update(" &", 2);
                if (method->getRefQualifier() == clang::RQ_RValue)
                    hasher.update(" &&", 3);
            }

            const clang::FunctionTemplateDecl* described_template = Super::decl->getDescribedFunctionTemplate();
            const clang::FunctionTemplateDecl* primary_template   = nullptr;
            DeclDb::QueryResult                primary_template_id{false};
            if (!described_template) {
                primary_template = Super::decl->getPrimaryTemplate();
                // Member func_ids are owned by records and are not top-level DeclDb identities.
                if (primary_template && !llvm::isa<clang::CXXMethodDecl>(Super::decl)) {
                    primary_template_id = DeclDb::queryDeclIdentity(primary_template->getTemplatedDecl());
                }
            }

            const clang::TemplateParameterList* declared_params     = described_template ? described_template->getTemplateParameters() : nullptr;
            const clang::TemplateArgumentList*  specialization_args = Super::decl->getTemplateSpecializationArgs();

            if (declared_params || specialization_args) {
                std::vector<AnyString> template_identity;
                Super::putTemplateDetails(declared_params, p_msg->mutable_template_details(), specialization_args, primary_template_id,
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
            }

            return Hash{hasher};
        }

    private:
        // Keep function/method type spelling here; DeclDb owns declaration and pattern lookup.
        template <typename Ref>
            requires (std::same_as<Ref, ParserTypes::TypeRef> || std::same_as<Ref, ParserTypes::VersionedTypeRef>)
        void putFunctionTypeRef(clang::QualType declared_type, Ref* p_type_ref) const {
            Super::putTypeRef(
                clang::TypeName::getFullyQualifiedName(declared_type, Super::getASTContext(), Super::getASTContext().getPrintingPolicy(), true),
                DeclDb::queryType(declared_type), p_type_ref);
        }
    };

    // Member functions share function serialization but belong to their record's arena.
    class MethodDeclWrapper final : public FunctionDeclWrapper<clang::CXXMethodDecl> {
    public:
        explicit MethodDeclWrapper(const clang::CXXMethodDecl* decl, const std::shared_ptr<google::protobuf::Arena>& arena) :
            FunctionDeclWrapper(decl, arena) {}

        // The record supplies layout availability; dependent records have no vtable offsets.
        [[nodiscard]] ParserTypes::MemberFunction* serialize(bool has_known_layout = false) const;

    private:
        void putVTableDetails(ParserTypes::MemberFunction* p_msg) const;
    };
} // namespace UEMeta
