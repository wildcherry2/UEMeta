#pragma once

#include "AliasTypes.hpp"
#include "EnumTypes.hpp"

namespace UEMeta::Testing::Types {
    class BaseRecord {
    public:
        void func() {}
        virtual void vfunc(int a, bool b) {}

        int field = 0;
        Alpha bfield{};
        Stage stage = Stage::New;

        virtual ~BaseRecord() = default;
    };

    struct DerivedRecord : public BaseRecord {
        void vfunc(int a, bool b) override {}
        virtual int vfunc2() const { return 4; }

        int nfield = 1;

        void sfunc(char c) {}

        ~DerivedRecord() override = default;
    };

    struct DiamondRootRecordBase {
        virtual int RootVirtual() { return RootField; }

        int RootField = 1;
    };

    struct DiamondLeftRecordBase : virtual public DiamondRootRecordBase {
        virtual int LeftVirtual() { return LeftField; }

        int LeftField = 2;
    };

    struct DiamondRightRecordBase : virtual public DiamondRootRecordBase {
        virtual int RightVirtual() { return RightField; }

        int RightField = 3;
    };

    struct DiamondMethodCoverageRecord : public DiamondLeftRecordBase, protected DiamondRightRecordBase {
        int LeftVirtual() override { return 4; }
        int RightVirtual() override { return 5; }
        virtual int OwnVirtual() { return 6; }

        int DirectField = 4;
    };

    class MethodCoverageRecord {
    public:
        void PublicPlain() {}
        void PublicConst() const {}
        void PublicVolatile() volatile {}
        void PublicConstVolatile() const volatile {}
        virtual void PublicVirtual() {}
        virtual void PublicPureVirtual() = 0;
        void PublicDeleted() = delete;
        void PublicDeclaredOnly();

    private:
        void PrivatePlain() {}
        virtual void PrivateVirtual() {}
    };

    class FieldCoverageRecord {
    public:
        int PublicPlain;
        mutable const int* PublicMutablePointer = nullptr;
        unsigned PublicBitField : 3;
        unsigned PublicDefaultBitField : 5 = 7;

    private:
        long PrivatePlain = 19;
        mutable int PrivateMutable;
        short PrivateArray[2] = {};
    };

    // Incomplete tags are TLForwardDeclaration targets, so every valid TLRecordDeclaration case below is a complete
    // tag definition. Each family spells out its primary, implicit, explicit, extern-instantiation, and explicit-
    // instantiation-definition forms without test-generation macros.

    template <typename FirstType> class ClassOneParameterNoNestedNoBaseRecord {
    public:
        FirstType DependentField;
        int FixedField;
    };

    static_assert(sizeof(ClassOneParameterNoNestedNoBaseRecord<char>) > 0);

    template <> class ClassOneParameterNoNestedNoBaseRecord<short> {
    public:
        short DependentField;
        int FixedField;
    };

    static_assert(sizeof(ClassOneParameterNoNestedNoBaseRecord<short>) > 0);

    extern template class ClassOneParameterNoNestedNoBaseRecord<int>;

    template class ClassOneParameterNoNestedNoBaseRecord<long>;

    template <typename FirstType> class ClassOneParameterNoNestedSingleBaseRecord : public BaseRecord {};

    static_assert(sizeof(ClassOneParameterNoNestedSingleBaseRecord<char>) > 0);

    template <> class ClassOneParameterNoNestedSingleBaseRecord<short> : public BaseRecord {};

    static_assert(sizeof(ClassOneParameterNoNestedSingleBaseRecord<short>) > 0);

    extern template class ClassOneParameterNoNestedSingleBaseRecord<int>;

    template class ClassOneParameterNoNestedSingleBaseRecord<long>;

    template <typename FirstType>
    class ClassOneParameterNoNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                        protected DiamondRightRecordBase {};

    static_assert(sizeof(ClassOneParameterNoNestedDiamondBasesRecord<char>) > 0);

    template <>
    class ClassOneParameterNoNestedDiamondBasesRecord<short> : public DiamondLeftRecordBase,
                                                               protected DiamondRightRecordBase {};

    static_assert(sizeof(ClassOneParameterNoNestedDiamondBasesRecord<short>) > 0);

    extern template class ClassOneParameterNoNestedDiamondBasesRecord<int>;

    template class ClassOneParameterNoNestedDiamondBasesRecord<long>;

    template <typename FirstType> class ClassOneParameterOneNestedNoBaseRecord {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<char>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<char>::NestedDecl) > 0);

    template <> class ClassOneParameterOneNestedNoBaseRecord<short> {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<short>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<short>::NestedDecl) > 0);

    extern template class ClassOneParameterOneNestedNoBaseRecord<int>;
    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<int>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<int>::NestedDecl) > 0);

    template class ClassOneParameterOneNestedNoBaseRecord<long>;
    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<long>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedNoBaseRecord<long>::NestedDecl) > 0);

    template <typename FirstType> class ClassOneParameterOneNestedSingleBaseRecord : public BaseRecord {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<char>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<char>::NestedDecl) > 0);

    template <> class ClassOneParameterOneNestedSingleBaseRecord<short> : public BaseRecord {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<short>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<short>::NestedDecl) > 0);

    extern template class ClassOneParameterOneNestedSingleBaseRecord<int>;
    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<int>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<int>::NestedDecl) > 0);

    template class ClassOneParameterOneNestedSingleBaseRecord<long>;
    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<long>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedSingleBaseRecord<long>::NestedDecl) > 0);

    template <typename FirstType>
    class ClassOneParameterOneNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                         protected DiamondRightRecordBase {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<char>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<char>::NestedDecl) > 0);

    template <>
    class ClassOneParameterOneNestedDiamondBasesRecord<short> : public DiamondLeftRecordBase,
                                                                protected DiamondRightRecordBase {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<short>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<short>::NestedDecl) > 0);

    extern template class ClassOneParameterOneNestedDiamondBasesRecord<int>;
    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<int>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<int>::NestedDecl) > 0);

    template class ClassOneParameterOneNestedDiamondBasesRecord<long>;
    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<long>) > 0);
    static_assert(sizeof(ClassOneParameterOneNestedDiamondBasesRecord<long>::NestedDecl) > 0);

    template <typename FirstType> class ClassOneParameterDoublyNestedNoBaseRecord {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<char>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<char>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<char>::IntermediateDecl::AnotherDecl) > 0);

    template <> class ClassOneParameterDoublyNestedNoBaseRecord<short> {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<short>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<short>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<short>::IntermediateDecl::AnotherDecl) > 0);

    extern template class ClassOneParameterDoublyNestedNoBaseRecord<int>;
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<int>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<int>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<int>::IntermediateDecl::AnotherDecl) > 0);

    template class ClassOneParameterDoublyNestedNoBaseRecord<long>;
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<long>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<long>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedNoBaseRecord<long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType> class ClassOneParameterDoublyNestedSingleBaseRecord : public BaseRecord {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<char>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<char>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<char>::IntermediateDecl::AnotherDecl) > 0);

    template <> class ClassOneParameterDoublyNestedSingleBaseRecord<short> : public BaseRecord {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<short>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<short>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<short>::IntermediateDecl::AnotherDecl) > 0);

    extern template class ClassOneParameterDoublyNestedSingleBaseRecord<int>;
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<int>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<int>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<int>::IntermediateDecl::AnotherDecl) > 0);

    template class ClassOneParameterDoublyNestedSingleBaseRecord<long>;
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<long>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<long>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedSingleBaseRecord<long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType>
    class ClassOneParameterDoublyNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                            protected DiamondRightRecordBase {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<char>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<char>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<char>::IntermediateDecl::AnotherDecl) > 0);

    template <>
    class ClassOneParameterDoublyNestedDiamondBasesRecord<short> : public DiamondLeftRecordBase,
                                                                   protected DiamondRightRecordBase {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<short>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<short>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<short>::IntermediateDecl::AnotherDecl) > 0);

    extern template class ClassOneParameterDoublyNestedDiamondBasesRecord<int>;
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<int>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<int>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<int>::IntermediateDecl::AnotherDecl) > 0);

    template class ClassOneParameterDoublyNestedDiamondBasesRecord<long>;
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<long>) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<long>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassOneParameterDoublyNestedDiamondBasesRecord<long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType, typename SecondType> class ClassTwoParametersNoNestedNoBaseRecord {};

    static_assert(sizeof(ClassTwoParametersNoNestedNoBaseRecord<char, short>) > 0);

    template <> class ClassTwoParametersNoNestedNoBaseRecord<short, int> {};

    static_assert(sizeof(ClassTwoParametersNoNestedNoBaseRecord<short, int>) > 0);

    extern template class ClassTwoParametersNoNestedNoBaseRecord<int, long>;

    template class ClassTwoParametersNoNestedNoBaseRecord<long, long long>;

    template <typename FirstType, typename SecondType>
    class ClassTwoParametersNoNestedSingleBaseRecord : public BaseRecord {};

    static_assert(sizeof(ClassTwoParametersNoNestedSingleBaseRecord<char, short>) > 0);

    template <> class ClassTwoParametersNoNestedSingleBaseRecord<short, int> : public BaseRecord {};

    static_assert(sizeof(ClassTwoParametersNoNestedSingleBaseRecord<short, int>) > 0);

    extern template class ClassTwoParametersNoNestedSingleBaseRecord<int, long>;

    template class ClassTwoParametersNoNestedSingleBaseRecord<long, long long>;

    template <typename FirstType, typename SecondType>
    class ClassTwoParametersNoNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                         protected DiamondRightRecordBase {};

    static_assert(sizeof(ClassTwoParametersNoNestedDiamondBasesRecord<char, short>) > 0);

    template <>
    class ClassTwoParametersNoNestedDiamondBasesRecord<short, int> : public DiamondLeftRecordBase,
                                                                     protected DiamondRightRecordBase {};

    static_assert(sizeof(ClassTwoParametersNoNestedDiamondBasesRecord<short, int>) > 0);

    extern template class ClassTwoParametersNoNestedDiamondBasesRecord<int, long>;

    template class ClassTwoParametersNoNestedDiamondBasesRecord<long, long long>;

    template <typename FirstType, typename SecondType> class ClassTwoParametersOneNestedNoBaseRecord {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<char, short>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<char, short>::NestedDecl) > 0);

    template <> class ClassTwoParametersOneNestedNoBaseRecord<short, int> {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<short, int>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<short, int>::NestedDecl) > 0);

    extern template class ClassTwoParametersOneNestedNoBaseRecord<int, long>;
    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<int, long>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<int, long>::NestedDecl) > 0);

    template class ClassTwoParametersOneNestedNoBaseRecord<long, long long>;
    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<long, long long>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedNoBaseRecord<long, long long>::NestedDecl) > 0);

    template <typename FirstType, typename SecondType>
    class ClassTwoParametersOneNestedSingleBaseRecord : public BaseRecord {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<char, short>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<char, short>::NestedDecl) > 0);

    template <> class ClassTwoParametersOneNestedSingleBaseRecord<short, int> : public BaseRecord {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<short, int>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<short, int>::NestedDecl) > 0);

    extern template class ClassTwoParametersOneNestedSingleBaseRecord<int, long>;
    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<int, long>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<int, long>::NestedDecl) > 0);

    template class ClassTwoParametersOneNestedSingleBaseRecord<long, long long>;
    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<long, long long>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedSingleBaseRecord<long, long long>::NestedDecl) > 0);

    template <typename FirstType, typename SecondType>
    class ClassTwoParametersOneNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                          protected DiamondRightRecordBase {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<char, short>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<char, short>::NestedDecl) > 0);

    template <>
    class ClassTwoParametersOneNestedDiamondBasesRecord<short, int> : public DiamondLeftRecordBase,
                                                                      protected DiamondRightRecordBase {
    public:
        struct NestedDecl {};
    };

    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<short, int>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<short, int>::NestedDecl) > 0);

    extern template class ClassTwoParametersOneNestedDiamondBasesRecord<int, long>;
    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<int, long>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<int, long>::NestedDecl) > 0);

    template class ClassTwoParametersOneNestedDiamondBasesRecord<long, long long>;
    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<long, long long>) > 0);
    static_assert(sizeof(ClassTwoParametersOneNestedDiamondBasesRecord<long, long long>::NestedDecl) > 0);

    template <typename FirstType, typename SecondType> class ClassTwoParametersDoublyNestedNoBaseRecord {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<char, short>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<char, short>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<char, short>::IntermediateDecl::AnotherDecl) > 0);

    template <> class ClassTwoParametersDoublyNestedNoBaseRecord<short, int> {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<short, int>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<short, int>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<short, int>::IntermediateDecl::AnotherDecl) > 0);

    extern template class ClassTwoParametersDoublyNestedNoBaseRecord<int, long>;
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<int, long>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<int, long>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<int, long>::IntermediateDecl::AnotherDecl) > 0);

    template class ClassTwoParametersDoublyNestedNoBaseRecord<long, long long>;
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<long, long long>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<long, long long>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedNoBaseRecord<long, long long>::IntermediateDecl::AnotherDecl) >
                  0);

    template <typename FirstType, typename SecondType>
    class ClassTwoParametersDoublyNestedSingleBaseRecord : public BaseRecord {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<char, short>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<char, short>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<char, short>::IntermediateDecl::AnotherDecl) >
                  0);

    template <> class ClassTwoParametersDoublyNestedSingleBaseRecord<short, int> : public BaseRecord {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<short, int>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<short, int>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<short, int>::IntermediateDecl::AnotherDecl) >
                  0);

    extern template class ClassTwoParametersDoublyNestedSingleBaseRecord<int, long>;
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<int, long>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<int, long>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<int, long>::IntermediateDecl::AnotherDecl) > 0);

    template class ClassTwoParametersDoublyNestedSingleBaseRecord<long, long long>;
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<long, long long>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<long, long long>::IntermediateDecl) > 0);
    static_assert(
        sizeof(ClassTwoParametersDoublyNestedSingleBaseRecord<long, long long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType, typename SecondType>
    class ClassTwoParametersDoublyNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                             protected DiamondRightRecordBase {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<char, short>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<char, short>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<char, short>::IntermediateDecl::AnotherDecl) >
                  0);

    template <>
    class ClassTwoParametersDoublyNestedDiamondBasesRecord<short, int> : public DiamondLeftRecordBase,
                                                                         protected DiamondRightRecordBase {
    public:
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<short, int>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<short, int>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<short, int>::IntermediateDecl::AnotherDecl) >
                  0);

    extern template class ClassTwoParametersDoublyNestedDiamondBasesRecord<int, long>;
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<int, long>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<int, long>::IntermediateDecl) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<int, long>::IntermediateDecl::AnotherDecl) >
                  0);

    template class ClassTwoParametersDoublyNestedDiamondBasesRecord<long, long long>;
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<long, long long>) > 0);
    static_assert(sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<long, long long>::IntermediateDecl) > 0);
    static_assert(
        sizeof(ClassTwoParametersDoublyNestedDiamondBasesRecord<long, long long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType> struct StructOneParameterNoNestedNoBaseRecord {};

    static_assert(sizeof(StructOneParameterNoNestedNoBaseRecord<char>) > 0);

    template <> struct StructOneParameterNoNestedNoBaseRecord<short> {};

    static_assert(sizeof(StructOneParameterNoNestedNoBaseRecord<short>) > 0);

    extern template struct StructOneParameterNoNestedNoBaseRecord<int>;

    template struct StructOneParameterNoNestedNoBaseRecord<long>;

    template <typename FirstType> struct StructOneParameterNoNestedSingleBaseRecord : public BaseRecord {};

    static_assert(sizeof(StructOneParameterNoNestedSingleBaseRecord<char>) > 0);

    template <> struct StructOneParameterNoNestedSingleBaseRecord<short> : public BaseRecord {};

    static_assert(sizeof(StructOneParameterNoNestedSingleBaseRecord<short>) > 0);

    extern template struct StructOneParameterNoNestedSingleBaseRecord<int>;

    template struct StructOneParameterNoNestedSingleBaseRecord<long>;

    template <typename FirstType>
    struct StructOneParameterNoNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                          protected DiamondRightRecordBase {};

    static_assert(sizeof(StructOneParameterNoNestedDiamondBasesRecord<char>) > 0);

    template <>
    struct StructOneParameterNoNestedDiamondBasesRecord<short> : public DiamondLeftRecordBase,
                                                                 protected DiamondRightRecordBase {};

    static_assert(sizeof(StructOneParameterNoNestedDiamondBasesRecord<short>) > 0);

    extern template struct StructOneParameterNoNestedDiamondBasesRecord<int>;

    template struct StructOneParameterNoNestedDiamondBasesRecord<long>;

    template <typename FirstType> struct StructOneParameterOneNestedNoBaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<char>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<char>::NestedDecl) > 0);

    template <> struct StructOneParameterOneNestedNoBaseRecord<short> {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<short>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<short>::NestedDecl) > 0);

    extern template struct StructOneParameterOneNestedNoBaseRecord<int>;
    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<int>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<int>::NestedDecl) > 0);

    template struct StructOneParameterOneNestedNoBaseRecord<long>;
    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<long>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedNoBaseRecord<long>::NestedDecl) > 0);

    template <typename FirstType> struct StructOneParameterOneNestedSingleBaseRecord : public BaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<char>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<char>::NestedDecl) > 0);

    template <> struct StructOneParameterOneNestedSingleBaseRecord<short> : public BaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<short>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<short>::NestedDecl) > 0);

    extern template struct StructOneParameterOneNestedSingleBaseRecord<int>;
    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<int>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<int>::NestedDecl) > 0);

    template struct StructOneParameterOneNestedSingleBaseRecord<long>;
    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<long>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedSingleBaseRecord<long>::NestedDecl) > 0);

    template <typename FirstType>
    struct StructOneParameterOneNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                           protected DiamondRightRecordBase {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<char>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<char>::NestedDecl) > 0);

    template <>
    struct StructOneParameterOneNestedDiamondBasesRecord<short> : public DiamondLeftRecordBase,
                                                                  protected DiamondRightRecordBase {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<short>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<short>::NestedDecl) > 0);

    extern template struct StructOneParameterOneNestedDiamondBasesRecord<int>;
    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<int>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<int>::NestedDecl) > 0);

    template struct StructOneParameterOneNestedDiamondBasesRecord<long>;
    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<long>) > 0);
    static_assert(sizeof(StructOneParameterOneNestedDiamondBasesRecord<long>::NestedDecl) > 0);

    template <typename FirstType> struct StructOneParameterDoublyNestedNoBaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<char>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<char>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<char>::IntermediateDecl::AnotherDecl) > 0);

    template <> struct StructOneParameterDoublyNestedNoBaseRecord<short> {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<short>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<short>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<short>::IntermediateDecl::AnotherDecl) > 0);

    extern template struct StructOneParameterDoublyNestedNoBaseRecord<int>;
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<int>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<int>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<int>::IntermediateDecl::AnotherDecl) > 0);

    template struct StructOneParameterDoublyNestedNoBaseRecord<long>;
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<long>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<long>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedNoBaseRecord<long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType> struct StructOneParameterDoublyNestedSingleBaseRecord : public BaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<char>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<char>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<char>::IntermediateDecl::AnotherDecl) > 0);

    template <> struct StructOneParameterDoublyNestedSingleBaseRecord<short> : public BaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<short>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<short>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<short>::IntermediateDecl::AnotherDecl) > 0);

    extern template struct StructOneParameterDoublyNestedSingleBaseRecord<int>;
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<int>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<int>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<int>::IntermediateDecl::AnotherDecl) > 0);

    template struct StructOneParameterDoublyNestedSingleBaseRecord<long>;
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<long>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<long>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedSingleBaseRecord<long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType>
    struct StructOneParameterDoublyNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                              protected DiamondRightRecordBase {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<char>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<char>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<char>::IntermediateDecl::AnotherDecl) > 0);

    template <>
    struct StructOneParameterDoublyNestedDiamondBasesRecord<short> : public DiamondLeftRecordBase,
                                                                     protected DiamondRightRecordBase {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<short>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<short>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<short>::IntermediateDecl::AnotherDecl) > 0);

    extern template struct StructOneParameterDoublyNestedDiamondBasesRecord<int>;
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<int>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<int>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<int>::IntermediateDecl::AnotherDecl) > 0);

    template struct StructOneParameterDoublyNestedDiamondBasesRecord<long>;
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<long>) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<long>::IntermediateDecl) > 0);
    static_assert(sizeof(StructOneParameterDoublyNestedDiamondBasesRecord<long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType, typename SecondType> struct StructTwoParametersNoNestedNoBaseRecord {};

    static_assert(sizeof(StructTwoParametersNoNestedNoBaseRecord<char, short>) > 0);

    template <> struct StructTwoParametersNoNestedNoBaseRecord<short, int> {};

    static_assert(sizeof(StructTwoParametersNoNestedNoBaseRecord<short, int>) > 0);

    extern template struct StructTwoParametersNoNestedNoBaseRecord<int, long>;

    template struct StructTwoParametersNoNestedNoBaseRecord<long, long long>;

    template <typename FirstType, typename SecondType>
    struct StructTwoParametersNoNestedSingleBaseRecord : public BaseRecord {};

    static_assert(sizeof(StructTwoParametersNoNestedSingleBaseRecord<char, short>) > 0);

    template <> struct StructTwoParametersNoNestedSingleBaseRecord<short, int> : public BaseRecord {};

    static_assert(sizeof(StructTwoParametersNoNestedSingleBaseRecord<short, int>) > 0);

    extern template struct StructTwoParametersNoNestedSingleBaseRecord<int, long>;

    template struct StructTwoParametersNoNestedSingleBaseRecord<long, long long>;

    template <typename FirstType, typename SecondType>
    struct StructTwoParametersNoNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                           protected DiamondRightRecordBase {};

    static_assert(sizeof(StructTwoParametersNoNestedDiamondBasesRecord<char, short>) > 0);

    template <>
    struct StructTwoParametersNoNestedDiamondBasesRecord<short, int> : public DiamondLeftRecordBase,
                                                                       protected DiamondRightRecordBase {};

    static_assert(sizeof(StructTwoParametersNoNestedDiamondBasesRecord<short, int>) > 0);

    extern template struct StructTwoParametersNoNestedDiamondBasesRecord<int, long>;

    template struct StructTwoParametersNoNestedDiamondBasesRecord<long, long long>;

    template <typename FirstType, typename SecondType> struct StructTwoParametersOneNestedNoBaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<char, short>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<char, short>::NestedDecl) > 0);

    template <> struct StructTwoParametersOneNestedNoBaseRecord<short, int> {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<short, int>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<short, int>::NestedDecl) > 0);

    extern template struct StructTwoParametersOneNestedNoBaseRecord<int, long>;
    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<int, long>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<int, long>::NestedDecl) > 0);

    template struct StructTwoParametersOneNestedNoBaseRecord<long, long long>;
    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<long, long long>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedNoBaseRecord<long, long long>::NestedDecl) > 0);

    template <typename FirstType, typename SecondType>
    struct StructTwoParametersOneNestedSingleBaseRecord : public BaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<char, short>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<char, short>::NestedDecl) > 0);

    template <> struct StructTwoParametersOneNestedSingleBaseRecord<short, int> : public BaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<short, int>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<short, int>::NestedDecl) > 0);

    extern template struct StructTwoParametersOneNestedSingleBaseRecord<int, long>;
    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<int, long>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<int, long>::NestedDecl) > 0);

    template struct StructTwoParametersOneNestedSingleBaseRecord<long, long long>;
    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<long, long long>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedSingleBaseRecord<long, long long>::NestedDecl) > 0);

    template <typename FirstType, typename SecondType>
    struct StructTwoParametersOneNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                            protected DiamondRightRecordBase {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<char, short>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<char, short>::NestedDecl) > 0);

    template <>
    struct StructTwoParametersOneNestedDiamondBasesRecord<short, int> : public DiamondLeftRecordBase,
                                                                        protected DiamondRightRecordBase {
        struct NestedDecl {};
    };

    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<short, int>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<short, int>::NestedDecl) > 0);

    extern template struct StructTwoParametersOneNestedDiamondBasesRecord<int, long>;
    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<int, long>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<int, long>::NestedDecl) > 0);

    template struct StructTwoParametersOneNestedDiamondBasesRecord<long, long long>;
    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<long, long long>) > 0);
    static_assert(sizeof(StructTwoParametersOneNestedDiamondBasesRecord<long, long long>::NestedDecl) > 0);

    template <typename FirstType, typename SecondType> struct StructTwoParametersDoublyNestedNoBaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<char, short>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<char, short>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<char, short>::IntermediateDecl::AnotherDecl) > 0);

    template <> struct StructTwoParametersDoublyNestedNoBaseRecord<short, int> {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<short, int>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<short, int>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<short, int>::IntermediateDecl::AnotherDecl) > 0);

    extern template struct StructTwoParametersDoublyNestedNoBaseRecord<int, long>;
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<int, long>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<int, long>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<int, long>::IntermediateDecl::AnotherDecl) > 0);

    template struct StructTwoParametersDoublyNestedNoBaseRecord<long, long long>;
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<long, long long>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<long, long long>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedNoBaseRecord<long, long long>::IntermediateDecl::AnotherDecl) >
                  0);

    template <typename FirstType, typename SecondType>
    struct StructTwoParametersDoublyNestedSingleBaseRecord : public BaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<char, short>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<char, short>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<char, short>::IntermediateDecl::AnotherDecl) >
                  0);

    template <> struct StructTwoParametersDoublyNestedSingleBaseRecord<short, int> : public BaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<short, int>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<short, int>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<short, int>::IntermediateDecl::AnotherDecl) >
                  0);

    extern template struct StructTwoParametersDoublyNestedSingleBaseRecord<int, long>;
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<int, long>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<int, long>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<int, long>::IntermediateDecl::AnotherDecl) >
                  0);

    template struct StructTwoParametersDoublyNestedSingleBaseRecord<long, long long>;
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<long, long long>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<long, long long>::IntermediateDecl) > 0);
    static_assert(
        sizeof(StructTwoParametersDoublyNestedSingleBaseRecord<long, long long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType, typename SecondType>
    struct StructTwoParametersDoublyNestedDiamondBasesRecord : public DiamondLeftRecordBase,
                                                               protected DiamondRightRecordBase {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<char, short>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<char, short>::IntermediateDecl) > 0);
    static_assert(
        sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<char, short>::IntermediateDecl::AnotherDecl) > 0);

    template <>
    struct StructTwoParametersDoublyNestedDiamondBasesRecord<short, int> : public DiamondLeftRecordBase,
                                                                           protected DiamondRightRecordBase {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<short, int>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<short, int>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<short, int>::IntermediateDecl::AnotherDecl) >
                  0);

    extern template struct StructTwoParametersDoublyNestedDiamondBasesRecord<int, long>;
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<int, long>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<int, long>::IntermediateDecl) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<int, long>::IntermediateDecl::AnotherDecl) >
                  0);

    template struct StructTwoParametersDoublyNestedDiamondBasesRecord<long, long long>;
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<long, long long>) > 0);
    static_assert(sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<long, long long>::IntermediateDecl) > 0);
    static_assert(
        sizeof(StructTwoParametersDoublyNestedDiamondBasesRecord<long, long long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType> union UnionOneParameterNoNestedNoBaseRecord {};

    static_assert(sizeof(UnionOneParameterNoNestedNoBaseRecord<char>) > 0);

    template <> union UnionOneParameterNoNestedNoBaseRecord<short> {};

    static_assert(sizeof(UnionOneParameterNoNestedNoBaseRecord<short>) > 0);

    extern template union UnionOneParameterNoNestedNoBaseRecord<int>;

    template union UnionOneParameterNoNestedNoBaseRecord<long>;

    template <typename FirstType> union UnionOneParameterOneNestedNoBaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<char>) > 0);
    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<char>::NestedDecl) > 0);

    template <> union UnionOneParameterOneNestedNoBaseRecord<short> {
        struct NestedDecl {};
    };

    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<short>) > 0);
    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<short>::NestedDecl) > 0);

    extern template union UnionOneParameterOneNestedNoBaseRecord<int>;
    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<int>) > 0);
    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<int>::NestedDecl) > 0);

    template union UnionOneParameterOneNestedNoBaseRecord<long>;
    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<long>) > 0);
    static_assert(sizeof(UnionOneParameterOneNestedNoBaseRecord<long>::NestedDecl) > 0);

    template <typename FirstType> union UnionOneParameterDoublyNestedNoBaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<char>) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<char>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<char>::IntermediateDecl::AnotherDecl) > 0);

    template <> union UnionOneParameterDoublyNestedNoBaseRecord<short> {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<short>) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<short>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<short>::IntermediateDecl::AnotherDecl) > 0);

    extern template union UnionOneParameterDoublyNestedNoBaseRecord<int>;
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<int>) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<int>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<int>::IntermediateDecl::AnotherDecl) > 0);

    template union UnionOneParameterDoublyNestedNoBaseRecord<long>;
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<long>) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<long>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionOneParameterDoublyNestedNoBaseRecord<long>::IntermediateDecl::AnotherDecl) > 0);

    template <typename FirstType, typename SecondType> union UnionTwoParametersNoNestedNoBaseRecord {};

    static_assert(sizeof(UnionTwoParametersNoNestedNoBaseRecord<char, short>) > 0);

    template <> union UnionTwoParametersNoNestedNoBaseRecord<short, int> {};

    static_assert(sizeof(UnionTwoParametersNoNestedNoBaseRecord<short, int>) > 0);

    extern template union UnionTwoParametersNoNestedNoBaseRecord<int, long>;

    template union UnionTwoParametersNoNestedNoBaseRecord<long, long long>;

    template <typename FirstType, typename SecondType> union UnionTwoParametersOneNestedNoBaseRecord {
        struct NestedDecl {};
    };

    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<char, short>) > 0);
    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<char, short>::NestedDecl) > 0);

    template <> union UnionTwoParametersOneNestedNoBaseRecord<short, int> {
        struct NestedDecl {};
    };

    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<short, int>) > 0);
    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<short, int>::NestedDecl) > 0);

    extern template union UnionTwoParametersOneNestedNoBaseRecord<int, long>;
    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<int, long>) > 0);
    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<int, long>::NestedDecl) > 0);

    template union UnionTwoParametersOneNestedNoBaseRecord<long, long long>;
    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<long, long long>) > 0);
    static_assert(sizeof(UnionTwoParametersOneNestedNoBaseRecord<long, long long>::NestedDecl) > 0);

    template <typename FirstType, typename SecondType> union UnionTwoParametersDoublyNestedNoBaseRecord {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<char, short>) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<char, short>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<char, short>::IntermediateDecl::AnotherDecl) > 0);

    template <> union UnionTwoParametersDoublyNestedNoBaseRecord<short, int> {
        struct IntermediateDecl {
            struct AnotherDecl {};
        };
    };

    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<short, int>) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<short, int>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<short, int>::IntermediateDecl::AnotherDecl) > 0);

    extern template union UnionTwoParametersDoublyNestedNoBaseRecord<int, long>;
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<int, long>) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<int, long>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<int, long>::IntermediateDecl::AnotherDecl) > 0);

    template union UnionTwoParametersDoublyNestedNoBaseRecord<long, long long>;
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<long, long long>) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<long, long long>::IntermediateDecl) > 0);
    static_assert(sizeof(UnionTwoParametersDoublyNestedNoBaseRecord<long, long long>::IntermediateDecl::AnotherDecl) >
                  0);

    struct TypeInfoTemplatedBaseRecord : ClassOneParameterNoNestedNoBaseRecord<char> {};

    template<typename FirstType>
    class DependentVirtualLayoutRecord {
    public:
        virtual void Invoke() {}
        FirstType Obj;
    };

    template<typename FirstType>
    struct DependentFieldAlignmentRecord {
        alignas(alignof(FirstType)) int Value;
    };

    template<typename FirstType>
    struct alignas(alignof(FirstType)) DependentRecordAlignmentRecord {
        int Value;
    };

    template<typename FirstType>
    struct DependentTemplateBaseRecord {};

    template<typename FirstType>
    struct DependentTemplateDerivedRecord : DependentTemplateBaseRecord<FirstType> {};

    template<typename FirstType>
    struct DependentTypeParameterBaseRecord : FirstType {};

    template<typename FirstType>
    struct DependentQualifiedBaseRecord : FirstType::BaseType {};

    template<typename FirstType, typename SecondType>
    struct DependentQualifiedTemplateBaseRecord : FirstType::template BaseType<SecondType> {};

    template<typename FirstType>
    struct DependentExternalQualifiedBaseRecord : AliasTemplate<FirstType>::Base {};

    template<typename... BaseTypes>
    struct DependentPackBasesRecord : BaseTypes... {
        void Accept(BaseTypes&&... values) {}
    };

    template<typename ConversionType>
    struct CanonicalConversionRecord {
        operator ConversionType() const;
    };

} // namespace UEMeta::Testing::Types

namespace B {
    template<typename T>
    class A {
    public:
        T func(int b, T& val);
        T func(double b, T* val);
    };

    template<typename T, class X, int c>
    class Mixed {};
}
