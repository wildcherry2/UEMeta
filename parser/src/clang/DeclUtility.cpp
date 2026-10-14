#include "UEMeta/clang/DeclUtility.hpp"

#include "UEMeta/clang/Exception.hpp"
#include "clang/AST/ASTContext.h"
#include "clang/AST/QualTypeNames.h"

namespace {
    uint64_t next_decl_occurrence = 0;
} // namespace

uint64_t UEMeta::allocateDeclOccurrence() {
    return next_decl_occurrence++;
}

void UEMeta::resetDeclOccurrences() {
    next_decl_occurrence = 0;
}

void UEMeta::putContextFQN(llvm::raw_string_ostream& out_stream, const clang::Decl* for_decl) {
    if (!for_decl || !for_decl->getDeclContext()) {
        throw DeclException(for_decl, "DeclContext not found!");
    }
    clang::ASTContext& context = for_decl->getASTContext();
    // Clang resolves the supplied declaration's context itself. Passing
    // that context would skip a scope (or the translation unit entirely).
    const clang::NestedNameSpecifier scope_nns = clang::TypeName::getFullyQualifiedDeclaredContext(context, for_decl, true);
    if (!scope_nns) {
        throw DeclException(for_decl, "Failed to get scope of anonymous enumerators!");
    }
    // Print TypeName's rewritten qualifiers instead of recomputing declaration scopes.
    auto policy = context.getPrintingPolicy();
    policy.FullyQualifiedName = false;
    scope_nns.print(out_stream, policy);
}

UEMeta::Hash::Hash(boost::hash2::xxh3_128& hasher) {
    boost::hash2::digest<16> result = hasher.result();
    const auto*              values = reinterpret_cast<uint64_t*>(result.data());
    a                               = values[0];
    b                               = values[1];
}

UEMeta::Hash::Hash(const ParserTypes::Hash& hash) {
    a = hash.a();
    b = hash.b();
}

void UEMeta::Hash::putProtoHash(ParserTypes::Hash* hash) const {
    hash->set_a(a);
    hash->set_b(b);
}
