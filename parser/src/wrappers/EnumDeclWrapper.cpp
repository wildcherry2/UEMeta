#include "UEMeta/wrappers/EnumDeclWrapper.hpp"

#include "boost/hash2/hash_append_fwd.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/QualTypeNames.h"
#include "llvm/ADT/StringExtras.h"
#include "UEMeta/wrappers/Utility.hpp"

UEMeta::EnumDeclWrapper::IntermediateRepresentation UEMeta::EnumDeclWrapper::toIntermediateRepresentation() const {
    clang::QualType underlying = decl->getIntegerType();
    if (underlying.isNull()) underlying = decl->getPromotionType();
    if (underlying.isNull()) throw std::runtime_error("Underlying type of enumerator is unknown!");

    // if it has a stable identity or depends on a declarator, toIntermediateRepresentation with global thread-local message allocation
    // and return it
    if (computeHasIdentity() || decl->isEmbeddedInDeclarator()) {
        const auto out_msg = google::protobuf::Arena::Create<ParserTypes::TLEnumDeclaration>(arena.get());
        {
            const std::string fqn = computeFQN();
            const Hash decl_id = computeDeclId(fqn);
            putMetadata(out_msg->mutable_metadata(), true, fqn, decl_id);
            DeclDb::addDeclIdentity(decl, decl_id);
        }
        {
            const auto underlying_type = underlying.getAsString();
            if (underlying_type.empty()) {
                throw std::runtime_error("Underlying type of enumerator is unknown!");
            }
            SetVersionedString(out_msg->mutable_underlying_type(), underlying_type);
        }

        out_msg->set_scope(!decl->isScoped() ? ParserTypes::ENUM_SCOPE_UNSCOPED : decl->isScopedUsingClassTag() ? ParserTypes::ENUM_SCOPE_CLASS : ParserTypes::ENUM_SCOPE_STRUCT);
        for (const clang::EnumConstantDecl* enumerator : decl->enumerators()) {
            auto* p_enumerator = out_msg->add_enumerators();

            if (decl->getDeclName().isIdentifier()) {
                p_enumerator->set_name(decl->getName().str());
            }
            else {
                p_enumerator->set_name(decl->getNameAsString());
            }

            if (const clang::RawComment* comment = getASTContext().getRawCommentForAnyRedecl(enumerator)) {
                SetVersionedString(p_enumerator->mutable_documentation(), comment->getRawText(getASTContext().getSourceManager()));
            }

            SetVersionedString(p_enumerator->mutable_value(), llvm::toString(enumerator->getInitVal(), 10));
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

    const std::string type_name = clang::TypeName::getFullyQualifiedName(
        underlying.getCanonicalType(), getASTContext(), getASTContext().getPrintingPolicy(), true);

    std::vector<ParserTypes::TLGlobalVariableDeclaration*> variables;
    for (const auto* enumerator : decl->enumerators()) {
        auto* p_variable = google::protobuf::Arena::Create<ParserTypes::TLGlobalVariableDeclaration>(arena.get());
        const std::string fqn = context_fqn + enumerator->getNameAsString();
        boost::hash2::xxh3_128 hasher;
        hasher.update(fqn.data(), fqn.size());
        putMetadata(p_variable->mutable_metadata(), true, fqn, Hash{hasher});

        p_variable->set_is_anon_enum_value(true);
        SetVersioned(p_variable->mutable_constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);
        SetVersionedString(p_variable->mutable_default_value(), llvm::toString(enumerator->getInitVal(), 10));
        SetVersioned(p_variable->mutable_storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);

        auto* version = p_variable->mutable_type_ref()->add_versions();
        version->add_source_versions(Config::GetConfig().Version());
        auto* type_ref = version->mutable_value()->mutable_type_ref();
        SetVersionedString(type_ref->mutable_type_name(), type_name);
        type_ref->set_is_builtin_or_template(true);
        variables.push_back(p_variable);
    }
    return variables;
}

void UEMeta::EnumDeclWrapper::toFile() const {
    return toFile(toIntermediateRepresentation(), arena);
}

void UEMeta::EnumDeclWrapper::toFile(IntermediateRepresentation&& ir, const std::shared_ptr<google::protobuf::Arena>& arena) {
    if (const auto* vec = std::get_if<std::vector<ParserTypes::TLGlobalVariableDeclaration*>>(&ir)) {
        for (const auto* p_var : *vec) {
            saveToFile(p_var, arena);
        }
    }
    else if (auto** p_enum = std::get_if<ParserTypes::TLEnumDeclaration*>(&ir)) {
        saveToFile(*p_enum, arena);
    }
}

void UEMeta::EnumDeclWrapper::serializeAsFields(
    ParserTypes::AccessSpecifier access, ParserTypes::TLRecordDeclaration* dest) const {
    clang::QualType type = decl->getIntegerType();
    if (type.isNull()) type = decl->getPromotionType();
    if (type.isNull()) throw std::runtime_error("Underlying type of enumerator is unknown!");
    const std::string type_name = clang::TypeName::getFullyQualifiedName(
        type.getCanonicalType(), getASTContext(), getASTContext().getPrintingPolicy(), true);

    for (const auto* enumerator : decl->enumerators()) {
        auto* p_field = dest->add_fields();
        if (enumerator->getDeclName()) p_field->set_name(enumerator->getNameAsString());
        SetVersioned(p_field->mutable_access(), access);
        if (const auto* comment = getASTContext().getRawCommentForAnyRedecl(enumerator)) {
            SetVersionedString(p_field->mutable_documentation(), comment->getRawText(getASTContext().getSourceManager()));
        }

        // Every synthesized field shares the enum's underlying integer type.
        auto* version = p_field->mutable_type_ref()->add_versions();
        version->add_source_versions(Config::GetConfig().Version());
        auto* type_ref = version->mutable_value()->mutable_type_ref();
        SetVersionedString(type_ref->mutable_type_name(), type_name);
        type_ref->set_is_builtin_or_template(true);

        p_field->set_is_anon_enum_value(true);
        SetVersionedBool(p_field->mutable_is_mutable(), false);
        SetVersionedBool(p_field->mutable_is_bitfield(), false);
        SetVersioned(p_field->mutable_storage_class(), ParserTypes::VAR_STORAGE_CLASS_STATIC);
        SetVersioned(p_field->mutable_constant_evaluation_kind(), ParserTypes::CONSTANT_EVALUATION_CONSTEXPR);

        // Dependent enumerators retain their initializer expression until their values are known.
        if (const auto* initializer = enumerator->getInitExpr(); initializer && initializer->isValueDependent()) {
            std::string out;
            llvm::raw_string_ostream os{out};
            initializer->printPretty(os, nullptr, getASTContext().getPrintingPolicy());
            SetVersionedString(p_field->mutable_default_value(), out);
        }
        else {
            SetVersionedString(p_field->mutable_default_value(), llvm::toString(enumerator->getInitVal(), 10));
        }
    }
}

// enums don't have params or templates, so we just hash the fqn; exclude the underlying type since that's version
// sensitive
UEMeta::Hash UEMeta::EnumDeclWrapper::computeDeclId(std::string_view fqn) const {
    boost::hash2::xxh3_128 hasher;
    boost::hash2::hash_append(hasher, boost::hash2::endian::little, fqn);
    return Hash{hasher};
}

// typedef enums are treated as normal enums;
// if the name is attached to a variable, it's identity is linked to the variable, and this decl doesn't have an identity
// if the name is 'free standing' (meaning, not attached to a 'declarator' like a variable), its enumerators will
// be synthesized as variables/static fields in the surrounding scope, and this decl doesn't have an identity
bool UEMeta::EnumDeclWrapper::computeHasIdentity() const {
    return decl->hasNameForLinkage();
}

std::string UEMeta::EnumDeclWrapper::computeFQN() const {
    if (const clang::QualType type = getASTContext().getCanonicalTagType(decl); !type.isNull()) {
        return clang::TypeName::getFullyQualifiedName(type, getASTContext(), getASTContext().getPrintingPolicy(), true);
    }
    return "";
}
