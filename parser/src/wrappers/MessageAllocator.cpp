#include "UEMeta/wrappers/MessageAllocator.hpp"

uint64_t UEMeta::allocateDeclOccurrence() {
    static uint64_t value = 0;
    return value++;
}
