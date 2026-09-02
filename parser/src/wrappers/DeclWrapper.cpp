#include "UEMeta/wrappers/DeclWrapper.hpp"

void UEMeta::DeclWrapper::addForwardDeclaration() {
    forward_declarations.push_back(allocateDeclOccurrence());
}

uint64_t UEMeta::DeclWrapper::allocateDeclOccurrence() {
    static uint64_t value = 0;
    return value++;
}