#include "UEMeta/wrappers/Types.hpp"

UEMeta::Hash::Hash(boost::hash2::xxh3_128& hasher) {
    boost::hash2::digest<16> result = hasher.result();
    const auto* values = reinterpret_cast<uint64_t*>(result.data());
    a = values[0];
    b = values[1];
}

void UEMeta::Hash::putProtoHash(ParserTypes::Hash *hash) const {
    hash->set_a(a);
    hash->set_b(b);
}
