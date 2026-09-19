#include "UEMeta/clang/wrappers/EnumDeclWrapper.hpp"

#include "UEMeta/utility/DeclException.hpp"
#include "UEMeta/utility/DeclUtility.hpp"
#include "boost/hash2/hash_append.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/QualTypeNames.h"
#include "llvm/ADT/StringExtras.h"
#include "UEMeta/clang/ReflectionDb.hpp"

UEMeta::EnumDeclWrapper::IntermediateRepresentation UEMeta::EnumDeclWrapper::toIntermediateRepresentation() const {
    clang::QualType underlying = decl->getIntegerType();
    if (underlying.isNull())
        underlying = decl->getPromotionType();
    if (underlying.isNull())
        throw DeclException(decl, "Underlying type of enumerator is unknown!");

    // Named, typedef-named and embedded enums retain an enum representation.
    if (computeHasIdentity() || decl->isEmbeddedInDeclarator()) {
        const auto out_msg = google::protobuf::Arena::Create<ParserTypes::TLEnumDeclaration>(arena.get());
        {
            const std::string fqn     = computeFQN();
            const Hash        decl_id = computeDeclId(fqn);
            putMetadata(out_msg->mutable_metadata(), true, fqn, decl_id);
            DeclDb::addDeclIdentity(decl, decl_id);
        }
        // A non-null enum integer type has a spelling; missing types were rejected above.
        setVersionedString(out_msg->mutable_underlying_type(), underlying.getAsString());

        out_msg->set_scope(!decl->isScoped()               ? ParserTypes::ENUM_SCOPE_UNSCOPED
                           : decl->isScopedUsingClassTag() ? ParserTypes::ENUM_SCOPE_CLASS
                                                           : ParserTypes::ENUM_SCOPE_STRUCT);
        for (const clang::EnumConstantDecl* enumerator : decl->enumerators()) {
            auto* p_enumerator = out_msg->add_enumerators();

            p_enumerator->set_name(enumerator->getNameAsString());

            if (const clang::RawComment* comment = getASTContext().getRawCommentForAnyRedecl(enumerator)) {
                setVersionedString(p_enumerator->mutable_documentation(), comment->getRawText(getASTContext().getSourceManager()));
            }

            setVersionedString(p_enumerator->mutable_value(), llvm::toString(enumerator->getInitVal(), 10));
        }
        std::string_view package = ReflectionDb::registerReflectable(decl);
        if (!package.empty()) {
            setVersionedString(out_msg->mutable_reflected_package(), package);
        }
        return out_msg;
    }

    // if it doesn't have a stable identity, then the enumerators will become owned by the nearest enclosing
    // non-anonymous scope as static constexpr globally accessible variables
    std::string context_fqn;
    {
        llvm::raw_string_ostream os{context_fqn};
        putContextFQN(os);
    }

    const std::string type_name =
        clang::TypeName::getFullyQualifiedName(underlying.getCanonicalType(), getASTContext(), getASTContext().getPrintingPolicy(), true);

    std::vector<ParserTypes::TLGlobalVariableDeclaration*> variables;
    for (const auto* enumerator : decl->enumerators()) {
        auto*                  p_variable = google::protobuf::Arena::Create<ParserTypes::TLGlobalVariableDeclaration>(arena.get());
        const std::string      fqn        = context_fqn + enumerator->getNameAsString();
        boost::hash2::xxh3_128 hasher;
        hasher.update(fqn.data(), fqn.size());
        putMetadata(p_variable->mutable_metadata(), true, fqn, Hash{hasher});

        p_variable->set_is_anon_enum_value(true);
        setVersioned(p_variable->mutable_constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        setVersionedString(p_variable->mutable_default_value(), llvm::toString(enumerator->getInitVal(), 10));
        setVersioned(p_variable->mutable_storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);

        auto* version = p_variable->mutable_type_ref()->add_versions();
        version->add_source_versions(Config::getConfig().getVersion());
        auto* type_ref = version->mutable_value()->mutable_type_ref();
        setVersionedString(type_ref->mutable_type_name(), type_name);
        type_ref->set_is_builtin_or_template(true);
        variables.push_back(p_variable);
    }
    return variables;
}

void UEMeta::EnumDeclWrapper::toFile() const { return toFile(toIntermediateRepresentation(), arena); }

void UEMeta::EnumDeclWrapper::toFile(IntermediateRepresentation&& ir, const std::shared_ptr<google::protobuf::Arena>& arena) {
    if (const auto* vec = std::get_if<std::vector<ParserTypes::TLGlobalVariableDeclaration*>>(&ir)) {
        for (const auto* p_var : *vec) {
            saveToFile(p_var, arena);
        }
    }
    else {
        saveToFile(std::get<ParserTypes::TLEnumDeclaration*>(ir), arena);
    }
}

void UEMeta::EnumDeclWrapper::serializeAsFields(ParserTypes::AccessSpecifier access, ParserTypes::TLRecordDeclaration* dest) const {
    clang::QualType type = decl->getIntegerType();
    if (type.isNull())
        type = decl->getPromotionType();
    if (type.isNull())
        throw DeclException(decl, "Underlying type of enumerator is unknown!");
    const std::string type_name =
        clang::TypeName::getFullyQualifiedName(type.getCanonicalType(), getASTContext(), getASTContext().getPrintingPolicy(), true);

    for (const auto* enumerator : decl->enumerators()) {
        auto* p_field = dest->add_fields();
        if (enumerator->getDeclName())
            p_field->set_name(enumerator->getNameAsString());
        setVersioned(p_field->mutable_access(), access);
        if (const auto* comment = getASTContext().getRawCommentForAnyRedecl(enumerator)) {
            setVersionedString(p_field->mutable_documentation(), comment->getRawText(getASTContext().getSourceManager()));
        }

        // Every synthesized field shares the enum's underlying integer type.
        auto* version = p_field->mutable_type_ref()->add_versions();
        version->add_source_versions(Config::getConfig().getVersion());
        auto* type_ref = version->mutable_value()->mutable_type_ref();
        setVersionedString(type_ref->mutable_type_name(), type_name);
        type_ref->set_is_builtin_or_template(true);

        p_field->set_is_anon_enum_value(true);
        setVersionedBool(p_field->mutable_is_mutable(), false);
        setVersionedBool(p_field->mutable_is_bitfield(), false);
        setVersioned(p_field->mutable_storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
        setVersioned(p_field->mutable_constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);

        // Dependent enumerators retain their initializer expression until their values are known.
        if (const auto* initializer = enumerator->getInitExpr(); initializer && initializer->isValueDependent()) {
            std::string              out;
            llvm::raw_string_ostream os{out};
            initializer->printPretty(os, nullptr, getASTContext().getPrintingPolicy());
            setVersionedString(p_field->mutable_default_value(), out);
        }
        else {
            setVersionedString(p_field->mutable_default_value(), llvm::toString(enumerator->getInitVal(), 10));
        }
    }
}

// enums don't have params or templates, so we just hash the fqn; exclude the underlying type since that's version
// sensitive
UEMeta::Hash UEMeta::EnumDeclWrapper::computeDeclId(std::string_view fqn) const {
    boost::hash2::xxh3_128 hasher;
    boost::hash2::hash_append(hasher, boost::hash2::little_endian_flavor{}, fqn);
    return Hash{hasher};
}

// typedef enums are treated as normal enums;
// if the name is attached to a variable, it's identity is linked to the variable, and this decl doesn't have an identity
// if the name is 'free standing' (meaning, not attached to a 'declarator' like a variable), its enumerators will
// be synthesized as variables/static fields in the surrounding scope, and this decl doesn't have an identity
bool UEMeta::EnumDeclWrapper::computeHasIdentity() const { return decl->hasNameForLinkage(); }

std::string UEMeta::EnumDeclWrapper::computeFQN() const {
    // ASTContext supplies the canonical tag type for a valid EnumDecl.
    const clang::QualType type = getASTContext().getCanonicalTagType(decl);
    return clang::TypeName::getFullyQualifiedName(type, getASTContext(), getASTContext().getPrintingPolicy(), true);
}
