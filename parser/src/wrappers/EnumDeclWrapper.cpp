#include "UEMeta/wrappers/EnumDeclWrapper.hpp"
#include "boost/hash2/xxh3.hpp"

bool UEMeta::EnumDeclWrapper::serialize(google::protobuf::Message* p_msg_base) const {
    auto* p_msg = dynamic_cast<ParserTypes::TLEnumDeclaration*>(p_msg_base);
    if (!p_msg) {
        return false;
    }

    

    return true;
}

void UEMeta::EnumDeclWrapper::onVisit(clang::ASTContext &context) {
    TypedDeclWrapper::onVisit(context);

    clang::QualType underlying = decl->getIntegerType();
    if (underlying.isNull()) underlying = decl->getPromotionType();
    if (!underlying.isNull()) {
        underlying_type = underlying.getAsString();
    }

    enum_scope = !decl->isScoped() ? ParserTypes::ENUM_SCOPE_UNSCOPED : decl->isScopedUsingClassTag() ? ParserTypes::ENUM_SCOPE_CLASS : ParserTypes::ENUM_SCOPE_STRUCT;
    enumerators.reserve(std::distance(decl->enumerators().begin(), decl->enumerators().end()));
    for (auto* enumerator : decl->enumerators()) {
        enumerators.emplace_back(enumerator, context);
    }
}

// enums don't have params or templates, so we just hash the fqn; exclude the underlying type since that's version
// sensitive
UEMeta::Hash UEMeta::EnumDeclWrapper::computeTypeId(std::string_view fqn, clang::ASTContext &ctx) const {
    boost::hash2::xxh3_128 hasher;
    hasher.update(fqn.data(), fqn.size());
    return Hash{hasher};
}

// typedef enums are treated as normal enums after desugaring;
// if the name is attached to a variable, it's identity is linked to the variable
// if the name is 'free standing' (meaning, not attached to a 'declarator' like a variable), its enumerators will
// be synthesized as variables/static fields in the surrounding scope
bool UEMeta::EnumDeclWrapper::hasIdentity() {
    return decl->getName().empty()
        && decl->getTypedefNameForAnonDecl() == nullptr // theoretically always true
        && (decl->isEmbeddedInDeclarator() || decl->isFreeStanding());
}

// since this isn't field/param/return/variable type, the canonical type (which resolves aliases/syntatic sugar) is
// basically just a type identity for this enum
clang::QualType UEMeta::EnumDeclWrapper::getQualType(clang::ASTContext &ctx) {
    const clang::QualType canonical = ctx.getCanonicalTagType(decl);
    if (!canonical.isNull()) {
        return canonical.getDesugaredType(ctx);
    }
    return canonical;
}

UEMeta::EnumDeclWrapper::EnumConstantWrapper::EnumConstantWrapper(const clang::EnumConstantDecl* decl, clang::ASTContext& ctx) {
    value = llvm::toString(decl->getInitVal(), 10);
    name = decl->getDeclName().isIdentifier() ? std::variant<llvm::StringRef, std::string>{decl->getName()}
            : std::variant<llvm::StringRef, std::string>(decl->getNameAsString());
    if (const clang::RawComment* comment = ctx.getRawCommentForDeclNoCache(decl)) {
        documentation = comment->getRawText(ctx.getSourceManager());
    }
}