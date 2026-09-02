#include "UEMeta/wrappers/EnumDeclWrapper.hpp"

void UEMeta::EnumDeclWrapper::serialize(const std::filesystem::path &to_dir) {
}

void UEMeta::EnumDeclWrapper::onVisit(clang::ASTContext &context) {
    TypedDeclWrapper::onVisit(context);
}

uint64_t UEMeta::EnumDeclWrapper::computeContentHash(clang::ASTContext &ctx) {

}

// enums don't have params or templates, so we just hash the fqn; exclude the underlying type since that's version
// sensitive
uint64_t UEMeta::EnumDeclWrapper::computeTypeId(std::string_view fqn, clang::ASTContext &ctx) {
    return std::hash<std::string_view>{}(fqn);
}

// typedef enums are treated as normal enums; truly anonymous enums aren't serialized as enums: enumerators are either serialized
// as global static constants (if the anon enum is global) or static fields (if the enum is within a record), with flags
// set in either case to indicate such, if the enum isn't attached to a variable. If it is attached to a variable, then
// the variable declaration carries the enum and we treat this as 'anonymous' for our purposes.
bool UEMeta::EnumDeclWrapper::isAnonymous() {
    return decl->getName().empty()
        && decl->getTypedefNameForAnonDecl() == nullptr // since we desuguar (canonicalize), this should theoretically always be true
        && decl->isEmbeddedInDeclarator()
        && !decl->isFreeStanding();
}

// since this isn't field/param/return/variable type, the canonical type (which resolves aliases/syntatic sugar) is
// basically just a type identity for this enum
clang::QualType UEMeta::EnumDeclWrapper::getQualType(clang::ASTContext &ctx) {
    return ctx.getCanonicalTagType(decl);
}
