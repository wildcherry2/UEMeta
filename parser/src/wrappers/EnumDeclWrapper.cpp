#include "UEMeta/wrappers/EnumDeclWrapper.hpp"

#include "boost/hash2/hash_append_fwd.hpp"
#include "boost/hash2/xxh3.hpp"
#include "clang/AST/QualTypeNames.h"
#include "UEMeta/wrappers/MessageAllocator.hpp"

UEMeta::EnumDeclWrapper::SerializeResult UEMeta::EnumDeclWrapper::serialize() const {
    // if it has a stable identity or depends on a declarator, serialize with global thread-local message allocation
    // and return it
    if (computeHasIdentity() || decl->isEmbeddedInDeclarator()) {
        const auto out_msg = google::protobuf::Arena::Create<ParserTypes::TLEnumDeclaration>(arena.get());
        {
            const std::string fqn = computeFQN();
            const Hash decl_id = computeDeclId(fqn);
            putMetadata(out_msg->mutable_metadata(), true, fqn, decl_id);
        }
        {
            clang::QualType underlying = decl->getIntegerType();
            if (underlying.isNull()) underlying = decl->getPromotionType();
            if (!underlying.isNull()) {
                const auto underlying_type = underlying.getAsString();
                if (underlying_type.empty()) {
                    throw std::runtime_error("Underlying type of enumerator is unknown!");
                }
                SetVersionedString(out_msg->mutable_underlying_type(), underlying_type);
            }
            else {
                throw std::runtime_error("Underlying type of enumerator is unknown!");
            }
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

    // TODO reuse VarDeclWrapper machinery
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