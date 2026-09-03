#include "UEMeta/wrappers/DeclWrapper.hpp"

UEMeta::Hash::Hash(boost::hash2::xxh3_128& hasher) {
    boost::hash2::digest<16> result = hasher.result();
    const auto* values = reinterpret_cast<uint64_t*>(result.data());
    a = values[0];
    b = values[1];
}

void UEMeta::DeclWrapper::addForwardDeclaration() {
    forward_declarations.push_back(allocateDeclOccurrence());
}

uint64_t UEMeta::DeclWrapper::allocateDeclOccurrence() {
    static uint64_t value = 0;
    return value++;
}