#include "UEMeta/clang/wrappers/DeclWrapper.hpp"

BS::thread_pool<> UEMeta::Detail::DeclWrapperStatics::serialization_pool;

uint64_t UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence() {
    static uint64_t value = 0;
    return value++;
}

void UEMeta::Detail::DeclWrapperStatics::awaitPendingSerializations() { serialization_pool.wait(); }
