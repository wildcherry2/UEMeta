#include "UEMeta/clang/DeclUtility.hpp"

namespace {
    uint64_t next_decl_occurrence = 0;
} // namespace

uint64_t UEMeta::allocateDeclOccurrence() {
    return next_decl_occurrence++;
}

void UEMeta::resetDeclOccurrences() {
    next_decl_occurrence = 0;
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
