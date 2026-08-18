#pragma once

namespace UEMeta::Testing::Types {
    enum class Stage {
        New,
        Parsed = 4,
        Serialized
    };

    enum NaturalUnscopedEmpty {};
    enum NaturalUnscopedSingle {
        NaturalUnscopedSingleItem
    };
    enum NaturalUnscopedPair {
        NaturalUnscopedPairFirst,
        NaturalUnscopedPairSecond
    };

    enum class NaturalClassEmpty {};
    enum class NaturalClassSingle {
        NaturalClassSingleItem
    };
    enum class NaturalClassPair {
        NaturalClassPairFirst,
        NaturalClassPairSecond
    };

    enum struct NaturalStructEmpty {};
    enum struct NaturalStructSingle {
        NaturalStructSingleItem
    };
    enum struct NaturalStructPair {
        NaturalStructPairFirst,
        NaturalStructPairSecond
    };

    enum NaturalUnscopedInt : int {};
    enum class NaturalClassInt : int {};
    enum struct NaturalStructInt : int {};

    enum AssignedUnscopedEmpty {};
    enum AssignedUnscopedSingle {
        AssignedUnscopedSingleItem = 17
    };
    enum AssignedUnscopedPair {
        AssignedUnscopedPairFirst = -11,
        AssignedUnscopedPairSecond = 42
    };

    enum class AssignedClassEmpty {};
    enum class AssignedClassSingle {
        AssignedClassSingleItem = 29
    };
    enum class AssignedClassPair {
        AssignedClassPairFirst = -7,
        AssignedClassPairSecond = 81
    };

    enum struct AssignedStructEmpty {};
    enum struct AssignedStructSingle {
        AssignedStructSingleItem = -33
    };
    enum struct AssignedStructPair {
        AssignedStructPairFirst = 14,
        AssignedStructPairSecond = 99
    };

    enum AssignedUnscopedInt : int {
        AssignedUnscopedIntItem = 101
    };
    enum class AssignedClassInt : int {
        AssignedClassIntItem = -101
    };
    enum struct AssignedStructInt : int {
        AssignedStructIntItem = 303
    };
}
