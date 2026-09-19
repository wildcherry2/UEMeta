#include "UEMeta/clang/wrappers/DeclWrapper.hpp"

BS::thread_pool<> UEMeta::Detail::DeclWrapperStatics::serialization_pool;
inline static uint64_t value = 0;

uint64_t UEMeta::Detail::DeclWrapperStatics::allocateDeclOccurrence() {
    return value++;
}

void UEMeta::Detail::DeclWrapperStatics::awaitPendingSerializations() { serialization_pool.wait(); }

#ifdef UEM_TESTING
void UEMeta::Detail::DeclWrapperStatics::resetDeclOccurrences() {
    value = 0;
}
#endif
