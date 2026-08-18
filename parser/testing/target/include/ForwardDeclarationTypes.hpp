#pragma once

namespace UEMeta::Testing::Types {
    class ForwardClass;
    struct ForwardStruct;
    union ForwardUnion;

    template<typename ClassType, typename ClassValue>
    class ForwardClassTemplate;

    template<typename ClassType>
    class ForwardClassTemplate<ClassType, int>;

    template<typename StructType, typename StructValue>
    struct ForwardStructTemplate;

    template<typename StructType>
    struct ForwardStructTemplate<StructType, int>;

    template<typename UnionType, typename UnionValue>
    union ForwardUnionTemplate;

    template<typename UnionType>
    union ForwardUnionTemplate<UnionType, int>;

    enum ForwardUnscopedEnum : int;
    enum class ForwardScopedEnum;

    template<typename ConcreteClassType>
    class ConcreteClassTemplate {};

    static_assert(sizeof(ConcreteClassTemplate<char>) > 0);
    extern template class ConcreteClassTemplate<short>;
    template class ConcreteClassTemplate<long>;

    template<typename ConcreteStructType>
    struct ConcreteStructTemplate {};

    static_assert(sizeof(ConcreteStructTemplate<float>) > 0);
    extern template struct ConcreteStructTemplate<double>;
    template struct ConcreteStructTemplate<long double>;

    template<typename ConcreteUnionType>
    union ConcreteUnionTemplate {};

    static_assert(sizeof(ConcreteUnionTemplate<signed char>) > 0);
    extern template union ConcreteUnionTemplate<unsigned int>;
    template union ConcreteUnionTemplate<unsigned long long>;
}
