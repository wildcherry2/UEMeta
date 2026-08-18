#ifndef UEMETA_TESTING_GLOBAL_VARIABLE_TYPES_CPP
#define UEMETA_TESTING_GLOBAL_VARIABLE_TYPES_CPP

namespace UEMeta::Testing::Types {
    int PlainUnspecifiedNoneWithoutDefault;
    int PlainUnspecifiedNoneWithDefault = 17;
    constexpr int PlainUnspecifiedConstexprWithDefault = 17;
    extern int PlainExternNoneWithoutDefault;
    extern int PlainExternNoneWithDefault = 17;
    extern constexpr int PlainExternConstexprWithDefault = 17;
    extern "C" { extern int PlainExternCNoneWithoutDefault; }
    extern "C" { extern int PlainExternCNoneWithDefault = 17; }
    extern "C" { extern constexpr int PlainExternCConstexprWithDefault = 17; }
    static int PlainStaticNoneWithoutDefault;
    static int PlainStaticNoneWithDefault = 17;
    static constexpr int PlainStaticConstexprWithDefault = 17;
    thread_local int PlainThreadLocalNoneWithoutDefault;
    thread_local int PlainThreadLocalNoneWithDefault = 17;
    thread_local constexpr int PlainThreadLocalConstexprWithDefault = 17;

    int* PointerUnspecifiedNoneWithoutDefault;
    int* PointerUnspecifiedNoneWithDefault = nullptr;
    constexpr int* PointerUnspecifiedConstexprWithDefault = nullptr;
    extern int* PointerExternNoneWithoutDefault;
    extern int* PointerExternNoneWithDefault = nullptr;
    extern constexpr int* PointerExternConstexprWithDefault = nullptr;
    extern "C" { extern int* PointerExternCNoneWithoutDefault; }
    extern "C" { extern int* PointerExternCNoneWithDefault = nullptr; }
    extern "C" { extern constexpr int* PointerExternCConstexprWithDefault = nullptr; }
    static int* PointerStaticNoneWithoutDefault;
    static int* PointerStaticNoneWithDefault = nullptr;
    static constexpr int* PointerStaticConstexprWithDefault = nullptr;
    thread_local int* PointerThreadLocalNoneWithoutDefault;
    thread_local int* PointerThreadLocalNoneWithDefault = nullptr;
    thread_local constexpr int* PointerThreadLocalConstexprWithDefault = nullptr;

    int** DoublePointerUnspecifiedNoneWithoutDefault;
    int** DoublePointerUnspecifiedNoneWithDefault = nullptr;
    constexpr int** DoublePointerUnspecifiedConstexprWithDefault = nullptr;
    extern int** DoublePointerExternNoneWithoutDefault;
    extern int** DoublePointerExternNoneWithDefault = nullptr;
    extern constexpr int** DoublePointerExternConstexprWithDefault = nullptr;
    extern "C" { extern int** DoublePointerExternCNoneWithoutDefault; }
    extern "C" { extern int** DoublePointerExternCNoneWithDefault = nullptr; }
    extern "C" { extern constexpr int** DoublePointerExternCConstexprWithDefault = nullptr; }
    static int** DoublePointerStaticNoneWithoutDefault;
    static int** DoublePointerStaticNoneWithDefault = nullptr;
    static constexpr int** DoublePointerStaticConstexprWithDefault = nullptr;
    thread_local int** DoublePointerThreadLocalNoneWithoutDefault;
    thread_local int** DoublePointerThreadLocalNoneWithDefault = nullptr;
    thread_local constexpr int** DoublePointerThreadLocalConstexprWithDefault = nullptr;

    int ArrayUnspecifiedNoneWithoutDefault[2];
    int ArrayUnspecifiedNoneWithDefault[2] = {17};
    constexpr int ArrayUnspecifiedConstexprWithDefault[2] = {17};
    extern int ArrayExternNoneWithoutDefault[2];
    extern int ArrayExternNoneWithDefault[2] = {17};
    extern constexpr int ArrayExternConstexprWithDefault[2] = {17};
    extern "C" { extern int ArrayExternCNoneWithoutDefault[2]; }
    extern "C" { extern int ArrayExternCNoneWithDefault[2] = {17}; }
    extern "C" { extern constexpr int ArrayExternCConstexprWithDefault[2] = {17}; }
    static int ArrayStaticNoneWithoutDefault[2];
    static int ArrayStaticNoneWithDefault[2] = {17};
    static constexpr int ArrayStaticConstexprWithDefault[2] = {17};
    thread_local int ArrayThreadLocalNoneWithoutDefault[2];
    thread_local int ArrayThreadLocalNoneWithDefault[2] = {17};
    thread_local constexpr int ArrayThreadLocalConstexprWithDefault[2] = {17};

    int& ReferenceUnspecifiedNoneWithDefault = PlainExternNoneWithoutDefault;
    constexpr int& ReferenceUnspecifiedConstexprWithDefault = PlainExternNoneWithoutDefault;
    extern int& ReferenceExternNoneWithoutDefault;
    extern int& ReferenceExternNoneWithDefault = PlainExternNoneWithoutDefault;
    extern constexpr int& ReferenceExternConstexprWithDefault = PlainExternNoneWithoutDefault;
    extern "C" { extern int& ReferenceExternCNoneWithoutDefault; }
    extern "C" { extern int& ReferenceExternCNoneWithDefault = PlainExternNoneWithoutDefault; }
    extern "C" { extern constexpr int& ReferenceExternCConstexprWithDefault = PlainExternNoneWithoutDefault; }
    static int& ReferenceStaticNoneWithDefault = PlainExternNoneWithoutDefault;
    static constexpr int& ReferenceStaticConstexprWithDefault = PlainExternNoneWithoutDefault;
    thread_local int& ReferenceThreadLocalNoneWithDefault = PlainExternNoneWithoutDefault;
    thread_local constexpr int& ReferenceThreadLocalConstexprWithDefault = PlainExternNoneWithoutDefault;
}

#endif
