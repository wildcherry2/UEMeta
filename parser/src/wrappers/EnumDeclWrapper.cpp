#include "UEMeta/wrappers/EnumDeclWrapper.hpp"
#include "boost/hash2/xxh3.hpp"
#include "UEMeta/wrappers/MessageAllocator.hpp"

bool UEMeta::EnumDeclWrapper::serialize(const std::filesystem::path& out_dir) const {
    return true;
}

std::vector<google::protobuf::Message*> UEMeta::EnumDeclWrapper::serialize() const {
    std::vector<google::protobuf::Message*> result;
    const clang::EnumDecl* decl = Decl();

    // if it has a stable identity or depends on a declarator, serialize with global thread-local message allocation
    // and return it
    if (hasIdentity() || decl->isEmbeddedInDeclarator()) {
        ParserTypes::TLEnumDeclaration* p_msg = MessageAllocator::GetEnum();
        addToMetadata(p_msg->mutable_metadata());
        SetVersionedString(p_msg->mutable_underlying_type(), underlying_type);
        p_msg->set_scope(enum_scope);
        for (auto& enumerator : enumerators) {
            auto* p_enumerator = p_msg->add_enumerators();
            if (auto* str_name = std::get_if<std::string>(&enumerator.name)) {
                p_enumerator->set_name(*str_name);
            }
            else if (auto* ref_name = std::get_if<llvm::StringRef>(&enumerator.name)) {
                p_enumerator->set_name(ref_name->str());
            }
            SetVersionedString(p_enumerator->mutable_documentation(), enumerator.documentation);
            SetVersionedString(p_enumerator->mutable_value(), enumerator.value);
        }
        result.push_back(p_msg);
        return result;
    }

    // if it doesn't have a stable identity, then the enumerators will become owned by the nearest enclosing
    // non-anonymous scope as static constexpr globally accessible variables
    result.reserve(enumerators.size());
    std::string scope_fqn;
    {
        const clang::Decl* decl_context = clang::Decl::castFromDeclContext(decl->getNonTransparentContext());
        clang::NestedNameSpecifier scope_nns = clang::TypeName::getFullyQualifiedDeclaredContext(
            decl->getASTContext(), decl_context, true);
        if (!scope_nns) {
            throw std::runtime_error("Failed to get scope of anonymous enumerators!");
        }
        llvm::raw_string_ostream os(scope_fqn);
        scope_nns.print(os, decl->getASTContext().getPrintingPolicy());
        if (scope_fqn.empty()) {
            throw std::runtime_error("Failed to print scope of anonymous enumerators!");
        }
    }

    // TODO reuse VarWrapper machinery
}

void UEMeta::EnumDeclWrapper::onVisit(clang::ASTContext& context) {
    DeclWrapper::onVisit(context);
    const clang::EnumDecl* decl = Decl();
    clang::QualType underlying = decl->getIntegerType();
    if (underlying.isNull()) underlying = decl->getPromotionType();
    if (!underlying.isNull()) {
        underlying_type = underlying.getAsString();
    }
    if (underlying_type.empty()) {
        throw std::runtime_error("Underlying type of enumerator is unknown!");
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

// typedef enums are treated as normal enums;
// if the name is attached to a variable, it's identity is linked to the variable, and this decl doesn't have an identity
// if the name is 'free standing' (meaning, not attached to a 'declarator' like a variable), its enumerators will
// be synthesized as variables/static fields in the surrounding scope, and this decl doesn't have an identity
bool UEMeta::EnumDeclWrapper::hasIdentity() const {
    return Decl()->hasNameForLinkage();
}

// since this isn't field/param/return/variable type, the canonical type (which resolves aliases/syntatic sugar) is
// basically just a type identity for this enum
clang::QualType UEMeta::EnumDeclWrapper::getQualType(clang::ASTContext &ctx) {
    const clang::QualType canonical = ctx.getCanonicalTagType(Decl());
    if (!canonical.isNull()) {
        return canonical.getDesugaredType(ctx);
    }
    return canonical;
}

void UEMeta::EnumDeclWrapper::addToContentHash(boost::hash2::xxh3_128& hash) const {
    DeclWrapper::addToContentHash(hash);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, underlying_type);
    boost::hash2::hash_append(hash, boost::hash2::endian::little, enum_scope);
    // technically, it's possible for enumerators to change order but manually assign values such that the
    // enum as a whole is structurally the same, but if nothing really changed, it won't make a difference
    // later in the pipeline
    for (const auto& enumerator : enumerators) {
        boost::hash2::hash_append(hash, boost::hash2::endian::little, enumerator.name);
        boost::hash2::hash_append(hash, boost::hash2::endian::little, enumerator.documentation);
        boost::hash2::hash_append(hash, boost::hash2::endian::little, enumerator.value);
    }
}

UEMeta::EnumDeclWrapper::EnumConstantWrapper::EnumConstantWrapper(const clang::EnumConstantDecl* decl, clang::ASTContext& ctx) {
    value = llvm::toString(decl->getInitVal(), 10);
    name = decl->getDeclName().isIdentifier() ? std::variant<llvm::StringRef, std::string>{decl->getName()}
            : std::variant<llvm::StringRef, std::string>(decl->getNameAsString());
    if (const clang::RawComment* comment = ctx.getRawCommentForDeclNoCache(decl)) {
        documentation = comment->getRawText(ctx.getSourceManager());
    }
}
