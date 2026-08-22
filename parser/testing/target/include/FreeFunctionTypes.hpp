#pragma once

#include "AliasTypes.hpp"

namespace UEMeta::Testing::Types {
    // Every declaration is written explicitly so each serialized function can
    // be matched directly to its storage, evaluation, definition, parameter,
    // return-type, and template-specialization combination.

    // Non-template functions: Unspecified storage
    auto NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersAuto();
    void NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersVoid();
    int NoTemplateUnspecifiedNoneWithoutDefinitionZeroParametersInt();
    auto NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterAuto(int First);
    void NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterVoid(int First);
    int NoTemplateUnspecifiedNoneWithoutDefinitionOneParameterInt(int First);
    auto NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    void NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    int NoTemplateUnspecifiedNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    auto NoTemplateUnspecifiedNoneWithDefinitionZeroParametersAuto() { return 0; }
    void NoTemplateUnspecifiedNoneWithDefinitionZeroParametersVoid() {}
    int NoTemplateUnspecifiedNoneWithDefinitionZeroParametersInt() { return 0; }
    auto NoTemplateUnspecifiedNoneWithDefinitionOneParameterAuto(int First) { return 0; }
    void NoTemplateUnspecifiedNoneWithDefinitionOneParameterVoid(int First) {}
    int NoTemplateUnspecifiedNoneWithDefinitionOneParameterInt(int First) { return 0; }
    auto NoTemplateUnspecifiedNoneWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    void NoTemplateUnspecifiedNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    int NoTemplateUnspecifiedNoneWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    constexpr auto NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersAuto();
    constexpr void NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersVoid();
    constexpr int NoTemplateUnspecifiedConstexprWithoutDefinitionZeroParametersInt();
    constexpr auto NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterAuto(int First);
    constexpr void NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterVoid(int First);
    constexpr int NoTemplateUnspecifiedConstexprWithoutDefinitionOneParameterInt(int First);
    constexpr auto NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    constexpr void NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    constexpr int NoTemplateUnspecifiedConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    constexpr auto NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersAuto() { return 0; }
    constexpr void NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersVoid() {}
    constexpr int NoTemplateUnspecifiedConstexprWithDefinitionZeroParametersInt() { return 0; }
    constexpr auto NoTemplateUnspecifiedConstexprWithDefinitionOneParameterAuto(int First) { return 0; }
    constexpr void NoTemplateUnspecifiedConstexprWithDefinitionOneParameterVoid(int First) {}
    constexpr int NoTemplateUnspecifiedConstexprWithDefinitionOneParameterInt(int First) { return 0; }
    constexpr auto NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    constexpr void NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    constexpr int NoTemplateUnspecifiedConstexprWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    consteval auto NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersAuto();
    consteval void NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersVoid();
    consteval int NoTemplateUnspecifiedConstevalWithoutDefinitionZeroParametersInt();
    consteval auto NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterAuto(int First);
    consteval void NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterVoid(int First);
    consteval int NoTemplateUnspecifiedConstevalWithoutDefinitionOneParameterInt(int First);
    consteval auto NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    consteval void NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    consteval int NoTemplateUnspecifiedConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    consteval auto NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersAuto() { return 0; }
    consteval void NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersVoid() {}
    consteval int NoTemplateUnspecifiedConstevalWithDefinitionZeroParametersInt() { return 0; }
    consteval auto NoTemplateUnspecifiedConstevalWithDefinitionOneParameterAuto(int First) { return 0; }
    consteval void NoTemplateUnspecifiedConstevalWithDefinitionOneParameterVoid(int First) {}
    consteval int NoTemplateUnspecifiedConstevalWithDefinitionOneParameterInt(int First) { return 0; }
    consteval auto NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    consteval void NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    consteval int NoTemplateUnspecifiedConstevalWithDefinitionTwoParametersInt(int First, long Second) { return 0; }

    // Non-template functions: Extern storage
    extern auto NoTemplateExternNoneWithoutDefinitionZeroParametersAuto();
    extern void NoTemplateExternNoneWithoutDefinitionZeroParametersVoid();
    extern int NoTemplateExternNoneWithoutDefinitionZeroParametersInt();
    extern auto NoTemplateExternNoneWithoutDefinitionOneParameterAuto(int First);
    extern void NoTemplateExternNoneWithoutDefinitionOneParameterVoid(int First);
    extern int NoTemplateExternNoneWithoutDefinitionOneParameterInt(int First);
    extern auto NoTemplateExternNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    extern void NoTemplateExternNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    extern int NoTemplateExternNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    extern auto NoTemplateExternNoneWithDefinitionZeroParametersAuto() { return 0; }
    extern void NoTemplateExternNoneWithDefinitionZeroParametersVoid() {}
    extern int NoTemplateExternNoneWithDefinitionZeroParametersInt() { return 0; }
    extern auto NoTemplateExternNoneWithDefinitionOneParameterAuto(int First) { return 0; }
    extern void NoTemplateExternNoneWithDefinitionOneParameterVoid(int First) {}
    extern int NoTemplateExternNoneWithDefinitionOneParameterInt(int First) { return 0; }
    extern auto NoTemplateExternNoneWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    extern void NoTemplateExternNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    extern int NoTemplateExternNoneWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    extern constexpr auto NoTemplateExternConstexprWithoutDefinitionZeroParametersAuto();
    extern constexpr void NoTemplateExternConstexprWithoutDefinitionZeroParametersVoid();
    extern constexpr int NoTemplateExternConstexprWithoutDefinitionZeroParametersInt();
    extern constexpr auto NoTemplateExternConstexprWithoutDefinitionOneParameterAuto(int First);
    extern constexpr void NoTemplateExternConstexprWithoutDefinitionOneParameterVoid(int First);
    extern constexpr int NoTemplateExternConstexprWithoutDefinitionOneParameterInt(int First);
    extern constexpr auto NoTemplateExternConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    extern constexpr void NoTemplateExternConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    extern constexpr int NoTemplateExternConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    extern constexpr auto NoTemplateExternConstexprWithDefinitionZeroParametersAuto() { return 0; }
    extern constexpr void NoTemplateExternConstexprWithDefinitionZeroParametersVoid() {}
    extern constexpr int NoTemplateExternConstexprWithDefinitionZeroParametersInt() { return 0; }
    extern constexpr auto NoTemplateExternConstexprWithDefinitionOneParameterAuto(int First) { return 0; }
    extern constexpr void NoTemplateExternConstexprWithDefinitionOneParameterVoid(int First) {}
    extern constexpr int NoTemplateExternConstexprWithDefinitionOneParameterInt(int First) { return 0; }
    extern constexpr auto NoTemplateExternConstexprWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    extern constexpr void NoTemplateExternConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    extern constexpr int NoTemplateExternConstexprWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    extern consteval auto NoTemplateExternConstevalWithoutDefinitionZeroParametersAuto();
    extern consteval void NoTemplateExternConstevalWithoutDefinitionZeroParametersVoid();
    extern consteval int NoTemplateExternConstevalWithoutDefinitionZeroParametersInt();
    extern consteval auto NoTemplateExternConstevalWithoutDefinitionOneParameterAuto(int First);
    extern consteval void NoTemplateExternConstevalWithoutDefinitionOneParameterVoid(int First);
    extern consteval int NoTemplateExternConstevalWithoutDefinitionOneParameterInt(int First);
    extern consteval auto NoTemplateExternConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    extern consteval void NoTemplateExternConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    extern consteval int NoTemplateExternConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    extern consteval auto NoTemplateExternConstevalWithDefinitionZeroParametersAuto() { return 0; }
    extern consteval void NoTemplateExternConstevalWithDefinitionZeroParametersVoid() {}
    extern consteval int NoTemplateExternConstevalWithDefinitionZeroParametersInt() { return 0; }
    extern consteval auto NoTemplateExternConstevalWithDefinitionOneParameterAuto(int First) { return 0; }
    extern consteval void NoTemplateExternConstevalWithDefinitionOneParameterVoid(int First) {}
    extern consteval int NoTemplateExternConstevalWithDefinitionOneParameterInt(int First) { return 0; }
    extern consteval auto NoTemplateExternConstevalWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    extern consteval void NoTemplateExternConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    extern consteval int NoTemplateExternConstevalWithDefinitionTwoParametersInt(int First, long Second) { return 0; }

    // Non-template functions: ExternC storage
    extern "C" {
    extern auto NoTemplateExternCNoneWithoutDefinitionZeroParametersAuto();
    extern void NoTemplateExternCNoneWithoutDefinitionZeroParametersVoid();
    extern int NoTemplateExternCNoneWithoutDefinitionZeroParametersInt();
    extern auto NoTemplateExternCNoneWithoutDefinitionOneParameterAuto(int First);
    extern void NoTemplateExternCNoneWithoutDefinitionOneParameterVoid(int First);
    extern int NoTemplateExternCNoneWithoutDefinitionOneParameterInt(int First);
    extern auto NoTemplateExternCNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    extern void NoTemplateExternCNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    extern int NoTemplateExternCNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    extern auto NoTemplateExternCNoneWithDefinitionZeroParametersAuto() { return 0; }
    extern void NoTemplateExternCNoneWithDefinitionZeroParametersVoid() {}
    extern int NoTemplateExternCNoneWithDefinitionZeroParametersInt() { return 0; }
    extern auto NoTemplateExternCNoneWithDefinitionOneParameterAuto(int First) { return 0; }
    extern void NoTemplateExternCNoneWithDefinitionOneParameterVoid(int First) {}
    extern int NoTemplateExternCNoneWithDefinitionOneParameterInt(int First) { return 0; }
    extern auto NoTemplateExternCNoneWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    extern void NoTemplateExternCNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    extern int NoTemplateExternCNoneWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    extern constexpr auto NoTemplateExternCConstexprWithoutDefinitionZeroParametersAuto();
    extern constexpr void NoTemplateExternCConstexprWithoutDefinitionZeroParametersVoid();
    extern constexpr int NoTemplateExternCConstexprWithoutDefinitionZeroParametersInt();
    extern constexpr auto NoTemplateExternCConstexprWithoutDefinitionOneParameterAuto(int First);
    extern constexpr void NoTemplateExternCConstexprWithoutDefinitionOneParameterVoid(int First);
    extern constexpr int NoTemplateExternCConstexprWithoutDefinitionOneParameterInt(int First);
    extern constexpr auto NoTemplateExternCConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    extern constexpr void NoTemplateExternCConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    extern constexpr int NoTemplateExternCConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    extern constexpr auto NoTemplateExternCConstexprWithDefinitionZeroParametersAuto() { return 0; }
    extern constexpr void NoTemplateExternCConstexprWithDefinitionZeroParametersVoid() {}
    extern constexpr int NoTemplateExternCConstexprWithDefinitionZeroParametersInt() { return 0; }
    extern constexpr auto NoTemplateExternCConstexprWithDefinitionOneParameterAuto(int First) { return 0; }
    extern constexpr void NoTemplateExternCConstexprWithDefinitionOneParameterVoid(int First) {}
    extern constexpr int NoTemplateExternCConstexprWithDefinitionOneParameterInt(int First) { return 0; }
    extern constexpr auto NoTemplateExternCConstexprWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    extern constexpr void NoTemplateExternCConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    extern constexpr int NoTemplateExternCConstexprWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    extern consteval auto NoTemplateExternCConstevalWithoutDefinitionZeroParametersAuto();
    extern consteval void NoTemplateExternCConstevalWithoutDefinitionZeroParametersVoid();
    extern consteval int NoTemplateExternCConstevalWithoutDefinitionZeroParametersInt();
    extern consteval auto NoTemplateExternCConstevalWithoutDefinitionOneParameterAuto(int First);
    extern consteval void NoTemplateExternCConstevalWithoutDefinitionOneParameterVoid(int First);
    extern consteval int NoTemplateExternCConstevalWithoutDefinitionOneParameterInt(int First);
    extern consteval auto NoTemplateExternCConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    extern consteval void NoTemplateExternCConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    extern consteval int NoTemplateExternCConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    extern consteval auto NoTemplateExternCConstevalWithDefinitionZeroParametersAuto() { return 0; }
    extern consteval void NoTemplateExternCConstevalWithDefinitionZeroParametersVoid() {}
    extern consteval int NoTemplateExternCConstevalWithDefinitionZeroParametersInt() { return 0; }
    extern consteval auto NoTemplateExternCConstevalWithDefinitionOneParameterAuto(int First) { return 0; }
    extern consteval void NoTemplateExternCConstevalWithDefinitionOneParameterVoid(int First) {}
    extern consteval int NoTemplateExternCConstevalWithDefinitionOneParameterInt(int First) { return 0; }
    extern consteval auto NoTemplateExternCConstevalWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    extern consteval void NoTemplateExternCConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    extern consteval int NoTemplateExternCConstevalWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    }

    // Non-template functions: Static storage
    static auto NoTemplateStaticNoneWithoutDefinitionZeroParametersAuto();
    static void NoTemplateStaticNoneWithoutDefinitionZeroParametersVoid();
    static int NoTemplateStaticNoneWithoutDefinitionZeroParametersInt();
    static auto NoTemplateStaticNoneWithoutDefinitionOneParameterAuto(int First);
    static void NoTemplateStaticNoneWithoutDefinitionOneParameterVoid(int First);
    static int NoTemplateStaticNoneWithoutDefinitionOneParameterInt(int First);
    static auto NoTemplateStaticNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    static void NoTemplateStaticNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    static int NoTemplateStaticNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    static auto NoTemplateStaticNoneWithDefinitionZeroParametersAuto() { return 0; }
    static void NoTemplateStaticNoneWithDefinitionZeroParametersVoid() {}
    static int NoTemplateStaticNoneWithDefinitionZeroParametersInt() { return 0; }
    static auto NoTemplateStaticNoneWithDefinitionOneParameterAuto(int First) { return 0; }
    static void NoTemplateStaticNoneWithDefinitionOneParameterVoid(int First) {}
    static int NoTemplateStaticNoneWithDefinitionOneParameterInt(int First) { return 0; }
    static auto NoTemplateStaticNoneWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    static void NoTemplateStaticNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    static int NoTemplateStaticNoneWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    static constexpr auto NoTemplateStaticConstexprWithoutDefinitionZeroParametersAuto();
    static constexpr void NoTemplateStaticConstexprWithoutDefinitionZeroParametersVoid();
    static constexpr int NoTemplateStaticConstexprWithoutDefinitionZeroParametersInt();
    static constexpr auto NoTemplateStaticConstexprWithoutDefinitionOneParameterAuto(int First);
    static constexpr void NoTemplateStaticConstexprWithoutDefinitionOneParameterVoid(int First);
    static constexpr int NoTemplateStaticConstexprWithoutDefinitionOneParameterInt(int First);
    static constexpr auto NoTemplateStaticConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    static constexpr void NoTemplateStaticConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    static constexpr int NoTemplateStaticConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    static constexpr auto NoTemplateStaticConstexprWithDefinitionZeroParametersAuto() { return 0; }
    static constexpr void NoTemplateStaticConstexprWithDefinitionZeroParametersVoid() {}
    static constexpr int NoTemplateStaticConstexprWithDefinitionZeroParametersInt() { return 0; }
    static constexpr auto NoTemplateStaticConstexprWithDefinitionOneParameterAuto(int First) { return 0; }
    static constexpr void NoTemplateStaticConstexprWithDefinitionOneParameterVoid(int First) {}
    static constexpr int NoTemplateStaticConstexprWithDefinitionOneParameterInt(int First) { return 0; }
    static constexpr auto NoTemplateStaticConstexprWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    static constexpr void NoTemplateStaticConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    static constexpr int NoTemplateStaticConstexprWithDefinitionTwoParametersInt(int First, long Second) { return 0; }
    static consteval auto NoTemplateStaticConstevalWithoutDefinitionZeroParametersAuto();
    static consteval void NoTemplateStaticConstevalWithoutDefinitionZeroParametersVoid();
    static consteval int NoTemplateStaticConstevalWithoutDefinitionZeroParametersInt();
    static consteval auto NoTemplateStaticConstevalWithoutDefinitionOneParameterAuto(int First);
    static consteval void NoTemplateStaticConstevalWithoutDefinitionOneParameterVoid(int First);
    static consteval int NoTemplateStaticConstevalWithoutDefinitionOneParameterInt(int First);
    static consteval auto NoTemplateStaticConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    static consteval void NoTemplateStaticConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    static consteval int NoTemplateStaticConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    static consteval auto NoTemplateStaticConstevalWithDefinitionZeroParametersAuto() { return 0; }
    static consteval void NoTemplateStaticConstevalWithDefinitionZeroParametersVoid() {}
    static consteval int NoTemplateStaticConstevalWithDefinitionZeroParametersInt() { return 0; }
    static consteval auto NoTemplateStaticConstevalWithDefinitionOneParameterAuto(int First) { return 0; }
    static consteval void NoTemplateStaticConstevalWithDefinitionOneParameterVoid(int First) {}
    static consteval int NoTemplateStaticConstevalWithDefinitionOneParameterInt(int First) { return 0; }
    static consteval auto NoTemplateStaticConstevalWithDefinitionTwoParametersAuto(int First, long Second) { return 0; }
    static consteval void NoTemplateStaticConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    static consteval int NoTemplateStaticConstevalWithDefinitionTwoParametersInt(int First, long Second) { return 0; }

    // Template argument roles:
    //   one parameter: implicit=char, explicit=short, declaration=int, definition=long
    //   two parameters: implicit=<char, short>, explicit=<short, int>,
    //                   declaration=<int, long>, definition=<long, long long>

    // One template parameter(s), Unspecified storage,
    // None evaluation, WithoutDefinition
    template <typename FirstType> auto TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto();
    template <> auto TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto<short>();
    extern template auto TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersAuto<int>();

    template <typename FirstType> void TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid();
    template <> void TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid<short>();
    extern template void TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersVoid<int>();

    template <typename FirstType> int TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt();
    template <> int TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt<short>();
    extern template int TemplateOneUnspecifiedNoneWithoutDefinitionZeroParametersInt<int>();

    template <typename FirstType> auto TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto(int First);
    template <> auto TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto<short>(int First);
    extern template auto TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterAuto<int>(int First);

    template <typename FirstType> void TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid(int First);
    template <> void TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid<short>(int First);
    extern template void TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterVoid<int>(int First);

    template <typename FirstType> int TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt(int First);
    template <> int TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt<short>(int First);
    extern template int TemplateOneUnspecifiedNoneWithoutDefinitionOneParameterInt<int>(int First);

    template <typename FirstType>
    auto TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <> auto TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto<short>(int First, long Second);
    extern template auto TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersAuto<int>(int First, long Second);

    template <typename FirstType>
    void TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <> void TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid<short>(int First, long Second);
    extern template void TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersVoid<int>(int First, long Second);

    template <typename FirstType>
    int TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    template <> int TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt<short>(int First, long Second);
    extern template int TemplateOneUnspecifiedNoneWithoutDefinitionTwoParametersInt<int>(int First, long Second);

    // One template parameter(s), Unspecified storage,
    // None evaluation, WithDefinition
    template <typename FirstType> auto TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto() { return 0; }
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAutoImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto<char>;
    template <> auto TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto<short>() { return 0; }
    extern template auto TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto<int>();
    template auto TemplateOneUnspecifiedNoneWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> void TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid() {}
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoidImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid<char>;
    template <> void TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid<short>() {}
    extern template void TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid<int>();
    template void TemplateOneUnspecifiedNoneWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> int TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt() { return 0; }
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionZeroParametersIntImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt<char>;
    template <> int TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt<short>() { return 0; }
    extern template int TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt<int>();
    template int TemplateOneUnspecifiedNoneWithDefinitionZeroParametersInt<long>();

    template <typename FirstType> auto TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto(int First) { return 0; }
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionOneParameterAutoImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto<char>;
    template <> auto TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto<short>(int First) { return 0; }
    extern template auto TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto<int>(int First);
    template auto TemplateOneUnspecifiedNoneWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType> void TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid(int First) {}
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoidImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid<char>;
    template <> void TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid<short>(int First) {}
    extern template void TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid<int>(int First);
    template void TemplateOneUnspecifiedNoneWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType> int TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt(int First) { return 0; }
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionOneParameterIntImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt<char>;
    template <> int TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt<short>(int First) { return 0; }
    extern template int TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt<int>(int First);
    template int TemplateOneUnspecifiedNoneWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    auto TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAutoImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto<char>;
    template <> auto TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto<short>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto<int>(int First, long Second);
    template auto TemplateOneUnspecifiedNoneWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    void TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoidImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid<char>;
    template <> void TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    extern template void TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid<int>(int First, long Second);
    template void TemplateOneUnspecifiedNoneWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType> int TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    inline auto* TemplateOneUnspecifiedNoneWithDefinitionTwoParametersIntImplicitAnchor =
        &TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt<char>;
    template <> int TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt<short>(int First, long Second) {
        return 0;
    }
    extern template int TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt<int>(int First, long Second);
    template int TemplateOneUnspecifiedNoneWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Unspecified storage,
    // Constexpr evaluation, WithoutDefinition
    template <typename FirstType> constexpr auto TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto();
    template <> constexpr auto TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto<short>();
    extern template auto TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersAuto<int>();

    template <typename FirstType> constexpr void TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid();
    template <> constexpr void TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid<short>();
    extern template void TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersVoid<int>();

    template <typename FirstType> constexpr int TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt();
    template <> constexpr int TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt<short>();
    extern template int TemplateOneUnspecifiedConstexprWithoutDefinitionZeroParametersInt<int>();

    template <typename FirstType>
    constexpr auto TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto(int First);
    template <> constexpr auto TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto<short>(int First);
    extern template auto TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterAuto<int>(int First);

    template <typename FirstType>
    constexpr void TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid(int First);
    template <> constexpr void TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid<short>(int First);
    extern template void TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterVoid<int>(int First);

    template <typename FirstType>
    constexpr int TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt(int First);
    template <> constexpr int TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt<short>(int First);
    extern template int TemplateOneUnspecifiedConstexprWithoutDefinitionOneParameterInt<int>(int First);

    template <typename FirstType>
    constexpr auto TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    constexpr auto TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto<short>(int First, long Second);
    extern template auto TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersAuto<int>(int First, long Second);

    template <typename FirstType>
    constexpr void TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    constexpr void TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid<short>(int First, long Second);
    extern template void TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersVoid<int>(int First, long Second);

    template <typename FirstType>
    constexpr int TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    constexpr int TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt<short>(int First, long Second);
    extern template int TemplateOneUnspecifiedConstexprWithoutDefinitionTwoParametersInt<int>(int First, long Second);

    // One template parameter(s), Unspecified storage,
    // Constexpr evaluation, WithDefinition
    template <typename FirstType> constexpr auto TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto<char>() == 0);
    template <> constexpr auto TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto<short>() { return 0; }
    extern template auto TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto<int>();
    template auto TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> constexpr void TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid<char>(), true));
    template <> constexpr void TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid<short>() {}
    extern template void TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid<int>();
    template void TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> constexpr int TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt<char>() == 0);
    template <> constexpr int TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt<short>() { return 0; }
    extern template int TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt<int>();
    template int TemplateOneUnspecifiedConstexprWithDefinitionZeroParametersInt<long>();

    template <typename FirstType>
    constexpr auto TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto<char>(0) == 0);
    template <> constexpr auto TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto<short>(int First) {
        return 0;
    }
    extern template auto TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto<int>(int First);
    template auto TemplateOneUnspecifiedConstexprWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType>
    constexpr void TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid<char>(0), true));
    template <> constexpr void TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid<short>(int First) {}
    extern template void TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid<int>(int First);
    template void TemplateOneUnspecifiedConstexprWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType>
    constexpr int TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt<char>(0) == 0);
    template <> constexpr int TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt<short>(int First) {
        return 0;
    }
    extern template int TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt<int>(int First);
    template int TemplateOneUnspecifiedConstexprWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    constexpr auto TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto<char>(0, 0) == 0);
    template <>
    constexpr auto TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto<short>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto<int>(int First, long Second);
    template auto TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    constexpr void TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid<char>(0, 0), true));
    template <>
    constexpr void TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    extern template void TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid<int>(int First, long Second);
    template void TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    constexpr int TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt<char>(0, 0) == 0);
    template <>
    constexpr int TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt<short>(int First, long Second) {
        return 0;
    }
    extern template int TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt<int>(int First, long Second);
    template int TemplateOneUnspecifiedConstexprWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Unspecified storage,
    // Consteval evaluation, WithoutDefinition
    template <typename FirstType> consteval auto TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto();
    template <> consteval auto TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto<short>();
    extern template auto TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersAuto<int>();

    template <typename FirstType> consteval void TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid();
    template <> consteval void TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid<short>();
    extern template void TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersVoid<int>();

    template <typename FirstType> consteval int TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt();
    template <> consteval int TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt<short>();
    extern template int TemplateOneUnspecifiedConstevalWithoutDefinitionZeroParametersInt<int>();

    template <typename FirstType>
    consteval auto TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto(int First);
    template <> consteval auto TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto<short>(int First);
    extern template auto TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterAuto<int>(int First);

    template <typename FirstType>
    consteval void TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid(int First);
    template <> consteval void TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid<short>(int First);
    extern template void TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterVoid<int>(int First);

    template <typename FirstType>
    consteval int TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt(int First);
    template <> consteval int TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt<short>(int First);
    extern template int TemplateOneUnspecifiedConstevalWithoutDefinitionOneParameterInt<int>(int First);

    template <typename FirstType>
    consteval auto TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    consteval auto TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto<short>(int First, long Second);
    extern template auto TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersAuto<int>(int First, long Second);

    template <typename FirstType>
    consteval void TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    consteval void TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid<short>(int First, long Second);
    extern template void TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersVoid<int>(int First, long Second);

    template <typename FirstType>
    consteval int TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    consteval int TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt<short>(int First, long Second);
    extern template int TemplateOneUnspecifiedConstevalWithoutDefinitionTwoParametersInt<int>(int First, long Second);

    // One template parameter(s), Unspecified storage,
    // Consteval evaluation, WithDefinition
    template <typename FirstType> consteval auto TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto<char>() == 0);
    template <> consteval auto TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto<short>() { return 0; }
    extern template auto TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto<int>();
    template auto TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> consteval void TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid<char>(), true));
    template <> consteval void TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid<short>() {}
    extern template void TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid<int>();
    template void TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> consteval int TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt<char>() == 0);
    template <> consteval int TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt<short>() { return 0; }
    extern template int TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt<int>();
    template int TemplateOneUnspecifiedConstevalWithDefinitionZeroParametersInt<long>();

    template <typename FirstType>
    consteval auto TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto<char>(0) == 0);
    template <> consteval auto TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto<short>(int First) {
        return 0;
    }
    extern template auto TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto<int>(int First);
    template auto TemplateOneUnspecifiedConstevalWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType>
    consteval void TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid<char>(0), true));
    template <> consteval void TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid<short>(int First) {}
    extern template void TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid<int>(int First);
    template void TemplateOneUnspecifiedConstevalWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType>
    consteval int TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt<char>(0) == 0);
    template <> consteval int TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt<short>(int First) {
        return 0;
    }
    extern template int TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt<int>(int First);
    template int TemplateOneUnspecifiedConstevalWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    consteval auto TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto<char>(0, 0) == 0);
    template <>
    consteval auto TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto<short>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto<int>(int First, long Second);
    template auto TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    consteval void TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid<char>(0, 0), true));
    template <>
    consteval void TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    extern template void TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid<int>(int First, long Second);
    template void TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    consteval int TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt<char>(0, 0) == 0);
    template <>
    consteval int TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt<short>(int First, long Second) {
        return 0;
    }
    extern template int TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt<int>(int First, long Second);
    template int TemplateOneUnspecifiedConstevalWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Extern storage,
    // None evaluation, WithoutDefinition
    template <typename FirstType> extern auto TemplateOneExternNoneWithoutDefinitionZeroParametersAuto();
    template <> auto TemplateOneExternNoneWithoutDefinitionZeroParametersAuto<short>();
    extern template auto TemplateOneExternNoneWithoutDefinitionZeroParametersAuto<int>();

    template <typename FirstType> extern void TemplateOneExternNoneWithoutDefinitionZeroParametersVoid();
    template <> void TemplateOneExternNoneWithoutDefinitionZeroParametersVoid<short>();
    extern template void TemplateOneExternNoneWithoutDefinitionZeroParametersVoid<int>();

    template <typename FirstType> extern int TemplateOneExternNoneWithoutDefinitionZeroParametersInt();
    template <> int TemplateOneExternNoneWithoutDefinitionZeroParametersInt<short>();
    extern template int TemplateOneExternNoneWithoutDefinitionZeroParametersInt<int>();

    template <typename FirstType> extern auto TemplateOneExternNoneWithoutDefinitionOneParameterAuto(int First);
    template <> auto TemplateOneExternNoneWithoutDefinitionOneParameterAuto<short>(int First);
    extern template auto TemplateOneExternNoneWithoutDefinitionOneParameterAuto<int>(int First);

    template <typename FirstType> extern void TemplateOneExternNoneWithoutDefinitionOneParameterVoid(int First);
    template <> void TemplateOneExternNoneWithoutDefinitionOneParameterVoid<short>(int First);
    extern template void TemplateOneExternNoneWithoutDefinitionOneParameterVoid<int>(int First);

    template <typename FirstType> extern int TemplateOneExternNoneWithoutDefinitionOneParameterInt(int First);
    template <> int TemplateOneExternNoneWithoutDefinitionOneParameterInt<short>(int First);
    extern template int TemplateOneExternNoneWithoutDefinitionOneParameterInt<int>(int First);

    template <typename FirstType>
    extern auto TemplateOneExternNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <> auto TemplateOneExternNoneWithoutDefinitionTwoParametersAuto<short>(int First, long Second);
    extern template auto TemplateOneExternNoneWithoutDefinitionTwoParametersAuto<int>(int First, long Second);

    template <typename FirstType>
    extern void TemplateOneExternNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <> void TemplateOneExternNoneWithoutDefinitionTwoParametersVoid<short>(int First, long Second);
    extern template void TemplateOneExternNoneWithoutDefinitionTwoParametersVoid<int>(int First, long Second);

    template <typename FirstType>
    extern int TemplateOneExternNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    template <> int TemplateOneExternNoneWithoutDefinitionTwoParametersInt<short>(int First, long Second);
    extern template int TemplateOneExternNoneWithoutDefinitionTwoParametersInt<int>(int First, long Second);

    // One template parameter(s), Extern storage,
    // None evaluation, WithDefinition
    template <typename FirstType> extern auto TemplateOneExternNoneWithDefinitionZeroParametersAuto() { return 0; }
    inline auto* TemplateOneExternNoneWithDefinitionZeroParametersAutoImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionZeroParametersAuto<char>;
    template <> auto TemplateOneExternNoneWithDefinitionZeroParametersAuto<short>() { return 0; }
    extern template auto TemplateOneExternNoneWithDefinitionZeroParametersAuto<int>();
    template auto TemplateOneExternNoneWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> extern void TemplateOneExternNoneWithDefinitionZeroParametersVoid() {}
    inline auto* TemplateOneExternNoneWithDefinitionZeroParametersVoidImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionZeroParametersVoid<char>;
    template <> void TemplateOneExternNoneWithDefinitionZeroParametersVoid<short>() {}
    extern template void TemplateOneExternNoneWithDefinitionZeroParametersVoid<int>();
    template void TemplateOneExternNoneWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> extern int TemplateOneExternNoneWithDefinitionZeroParametersInt() { return 0; }
    inline auto* TemplateOneExternNoneWithDefinitionZeroParametersIntImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionZeroParametersInt<char>;
    template <> int TemplateOneExternNoneWithDefinitionZeroParametersInt<short>() { return 0; }
    extern template int TemplateOneExternNoneWithDefinitionZeroParametersInt<int>();
    template int TemplateOneExternNoneWithDefinitionZeroParametersInt<long>();

    template <typename FirstType> extern auto TemplateOneExternNoneWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    inline auto* TemplateOneExternNoneWithDefinitionOneParameterAutoImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionOneParameterAuto<char>;
    template <> auto TemplateOneExternNoneWithDefinitionOneParameterAuto<short>(int First) { return 0; }
    extern template auto TemplateOneExternNoneWithDefinitionOneParameterAuto<int>(int First);
    template auto TemplateOneExternNoneWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType> extern void TemplateOneExternNoneWithDefinitionOneParameterVoid(int First) {}
    inline auto* TemplateOneExternNoneWithDefinitionOneParameterVoidImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionOneParameterVoid<char>;
    template <> void TemplateOneExternNoneWithDefinitionOneParameterVoid<short>(int First) {}
    extern template void TemplateOneExternNoneWithDefinitionOneParameterVoid<int>(int First);
    template void TemplateOneExternNoneWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType> extern int TemplateOneExternNoneWithDefinitionOneParameterInt(int First) { return 0; }
    inline auto* TemplateOneExternNoneWithDefinitionOneParameterIntImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionOneParameterInt<char>;
    template <> int TemplateOneExternNoneWithDefinitionOneParameterInt<short>(int First) { return 0; }
    extern template int TemplateOneExternNoneWithDefinitionOneParameterInt<int>(int First);
    template int TemplateOneExternNoneWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    extern auto TemplateOneExternNoneWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    inline auto* TemplateOneExternNoneWithDefinitionTwoParametersAutoImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionTwoParametersAuto<char>;
    template <> auto TemplateOneExternNoneWithDefinitionTwoParametersAuto<short>(int First, long Second) { return 0; }
    extern template auto TemplateOneExternNoneWithDefinitionTwoParametersAuto<int>(int First, long Second);
    template auto TemplateOneExternNoneWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    extern void TemplateOneExternNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    inline auto* TemplateOneExternNoneWithDefinitionTwoParametersVoidImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionTwoParametersVoid<char>;
    template <> void TemplateOneExternNoneWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    extern template void TemplateOneExternNoneWithDefinitionTwoParametersVoid<int>(int First, long Second);
    template void TemplateOneExternNoneWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    extern int TemplateOneExternNoneWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    inline auto* TemplateOneExternNoneWithDefinitionTwoParametersIntImplicitAnchor =
        &TemplateOneExternNoneWithDefinitionTwoParametersInt<char>;
    template <> int TemplateOneExternNoneWithDefinitionTwoParametersInt<short>(int First, long Second) { return 0; }
    extern template int TemplateOneExternNoneWithDefinitionTwoParametersInt<int>(int First, long Second);
    template int TemplateOneExternNoneWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Extern storage,
    // Constexpr evaluation, WithoutDefinition
    template <typename FirstType> extern constexpr auto TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto();
    template <> constexpr auto TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto<short>();
    extern template auto TemplateOneExternConstexprWithoutDefinitionZeroParametersAuto<int>();

    template <typename FirstType> extern constexpr void TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid();
    template <> constexpr void TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid<short>();
    extern template void TemplateOneExternConstexprWithoutDefinitionZeroParametersVoid<int>();

    template <typename FirstType> extern constexpr int TemplateOneExternConstexprWithoutDefinitionZeroParametersInt();
    template <> constexpr int TemplateOneExternConstexprWithoutDefinitionZeroParametersInt<short>();
    extern template int TemplateOneExternConstexprWithoutDefinitionZeroParametersInt<int>();

    template <typename FirstType>
    extern constexpr auto TemplateOneExternConstexprWithoutDefinitionOneParameterAuto(int First);
    template <> constexpr auto TemplateOneExternConstexprWithoutDefinitionOneParameterAuto<short>(int First);
    extern template auto TemplateOneExternConstexprWithoutDefinitionOneParameterAuto<int>(int First);

    template <typename FirstType>
    extern constexpr void TemplateOneExternConstexprWithoutDefinitionOneParameterVoid(int First);
    template <> constexpr void TemplateOneExternConstexprWithoutDefinitionOneParameterVoid<short>(int First);
    extern template void TemplateOneExternConstexprWithoutDefinitionOneParameterVoid<int>(int First);

    template <typename FirstType>
    extern constexpr int TemplateOneExternConstexprWithoutDefinitionOneParameterInt(int First);
    template <> constexpr int TemplateOneExternConstexprWithoutDefinitionOneParameterInt<short>(int First);
    extern template int TemplateOneExternConstexprWithoutDefinitionOneParameterInt<int>(int First);

    template <typename FirstType>
    extern constexpr auto TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    constexpr auto TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto<short>(int First, long Second);
    extern template auto TemplateOneExternConstexprWithoutDefinitionTwoParametersAuto<int>(int First, long Second);

    template <typename FirstType>
    extern constexpr void TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    constexpr void TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid<short>(int First, long Second);
    extern template void TemplateOneExternConstexprWithoutDefinitionTwoParametersVoid<int>(int First, long Second);

    template <typename FirstType>
    extern constexpr int TemplateOneExternConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    constexpr int TemplateOneExternConstexprWithoutDefinitionTwoParametersInt<short>(int First, long Second);
    extern template int TemplateOneExternConstexprWithoutDefinitionTwoParametersInt<int>(int First, long Second);

    // One template parameter(s), Extern storage,
    // Constexpr evaluation, WithDefinition
    template <typename FirstType> extern constexpr auto TemplateOneExternConstexprWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateOneExternConstexprWithDefinitionZeroParametersAuto<char>() == 0);
    template <> constexpr auto TemplateOneExternConstexprWithDefinitionZeroParametersAuto<short>() { return 0; }
    extern template auto TemplateOneExternConstexprWithDefinitionZeroParametersAuto<int>();
    template auto TemplateOneExternConstexprWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> extern constexpr void TemplateOneExternConstexprWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateOneExternConstexprWithDefinitionZeroParametersVoid<char>(), true));
    template <> constexpr void TemplateOneExternConstexprWithDefinitionZeroParametersVoid<short>() {}
    extern template void TemplateOneExternConstexprWithDefinitionZeroParametersVoid<int>();
    template void TemplateOneExternConstexprWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> extern constexpr int TemplateOneExternConstexprWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateOneExternConstexprWithDefinitionZeroParametersInt<char>() == 0);
    template <> constexpr int TemplateOneExternConstexprWithDefinitionZeroParametersInt<short>() { return 0; }
    extern template int TemplateOneExternConstexprWithDefinitionZeroParametersInt<int>();
    template int TemplateOneExternConstexprWithDefinitionZeroParametersInt<long>();

    template <typename FirstType>
    extern constexpr auto TemplateOneExternConstexprWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateOneExternConstexprWithDefinitionOneParameterAuto<char>(0) == 0);
    template <> constexpr auto TemplateOneExternConstexprWithDefinitionOneParameterAuto<short>(int First) { return 0; }
    extern template auto TemplateOneExternConstexprWithDefinitionOneParameterAuto<int>(int First);
    template auto TemplateOneExternConstexprWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType>
    extern constexpr void TemplateOneExternConstexprWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateOneExternConstexprWithDefinitionOneParameterVoid<char>(0), true));
    template <> constexpr void TemplateOneExternConstexprWithDefinitionOneParameterVoid<short>(int First) {}
    extern template void TemplateOneExternConstexprWithDefinitionOneParameterVoid<int>(int First);
    template void TemplateOneExternConstexprWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType>
    extern constexpr int TemplateOneExternConstexprWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateOneExternConstexprWithDefinitionOneParameterInt<char>(0) == 0);
    template <> constexpr int TemplateOneExternConstexprWithDefinitionOneParameterInt<short>(int First) { return 0; }
    extern template int TemplateOneExternConstexprWithDefinitionOneParameterInt<int>(int First);
    template int TemplateOneExternConstexprWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    extern constexpr auto TemplateOneExternConstexprWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneExternConstexprWithDefinitionTwoParametersAuto<char>(0, 0) == 0);
    template <>
    constexpr auto TemplateOneExternConstexprWithDefinitionTwoParametersAuto<short>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateOneExternConstexprWithDefinitionTwoParametersAuto<int>(int First, long Second);
    template auto TemplateOneExternConstexprWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    extern constexpr void TemplateOneExternConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateOneExternConstexprWithDefinitionTwoParametersVoid<char>(0, 0), true));
    template <>
    constexpr void TemplateOneExternConstexprWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    extern template void TemplateOneExternConstexprWithDefinitionTwoParametersVoid<int>(int First, long Second);
    template void TemplateOneExternConstexprWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    extern constexpr int TemplateOneExternConstexprWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneExternConstexprWithDefinitionTwoParametersInt<char>(0, 0) == 0);
    template <> constexpr int TemplateOneExternConstexprWithDefinitionTwoParametersInt<short>(int First, long Second) {
        return 0;
    }
    extern template int TemplateOneExternConstexprWithDefinitionTwoParametersInt<int>(int First, long Second);
    template int TemplateOneExternConstexprWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Extern storage,
    // Consteval evaluation, WithoutDefinition
    template <typename FirstType> extern consteval auto TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto();
    template <> consteval auto TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto<short>();
    extern template auto TemplateOneExternConstevalWithoutDefinitionZeroParametersAuto<int>();

    template <typename FirstType> extern consteval void TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid();
    template <> consteval void TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid<short>();
    extern template void TemplateOneExternConstevalWithoutDefinitionZeroParametersVoid<int>();

    template <typename FirstType> extern consteval int TemplateOneExternConstevalWithoutDefinitionZeroParametersInt();
    template <> consteval int TemplateOneExternConstevalWithoutDefinitionZeroParametersInt<short>();
    extern template int TemplateOneExternConstevalWithoutDefinitionZeroParametersInt<int>();

    template <typename FirstType>
    extern consteval auto TemplateOneExternConstevalWithoutDefinitionOneParameterAuto(int First);
    template <> consteval auto TemplateOneExternConstevalWithoutDefinitionOneParameterAuto<short>(int First);
    extern template auto TemplateOneExternConstevalWithoutDefinitionOneParameterAuto<int>(int First);

    template <typename FirstType>
    extern consteval void TemplateOneExternConstevalWithoutDefinitionOneParameterVoid(int First);
    template <> consteval void TemplateOneExternConstevalWithoutDefinitionOneParameterVoid<short>(int First);
    extern template void TemplateOneExternConstevalWithoutDefinitionOneParameterVoid<int>(int First);

    template <typename FirstType>
    extern consteval int TemplateOneExternConstevalWithoutDefinitionOneParameterInt(int First);
    template <> consteval int TemplateOneExternConstevalWithoutDefinitionOneParameterInt<short>(int First);
    extern template int TemplateOneExternConstevalWithoutDefinitionOneParameterInt<int>(int First);

    template <typename FirstType>
    extern consteval auto TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    consteval auto TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto<short>(int First, long Second);
    extern template auto TemplateOneExternConstevalWithoutDefinitionTwoParametersAuto<int>(int First, long Second);

    template <typename FirstType>
    extern consteval void TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    consteval void TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid<short>(int First, long Second);
    extern template void TemplateOneExternConstevalWithoutDefinitionTwoParametersVoid<int>(int First, long Second);

    template <typename FirstType>
    extern consteval int TemplateOneExternConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    consteval int TemplateOneExternConstevalWithoutDefinitionTwoParametersInt<short>(int First, long Second);
    extern template int TemplateOneExternConstevalWithoutDefinitionTwoParametersInt<int>(int First, long Second);

    // One template parameter(s), Extern storage,
    // Consteval evaluation, WithDefinition
    template <typename FirstType> extern consteval auto TemplateOneExternConstevalWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateOneExternConstevalWithDefinitionZeroParametersAuto<char>() == 0);
    template <> consteval auto TemplateOneExternConstevalWithDefinitionZeroParametersAuto<short>() { return 0; }
    extern template auto TemplateOneExternConstevalWithDefinitionZeroParametersAuto<int>();
    template auto TemplateOneExternConstevalWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> extern consteval void TemplateOneExternConstevalWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateOneExternConstevalWithDefinitionZeroParametersVoid<char>(), true));
    template <> consteval void TemplateOneExternConstevalWithDefinitionZeroParametersVoid<short>() {}
    extern template void TemplateOneExternConstevalWithDefinitionZeroParametersVoid<int>();
    template void TemplateOneExternConstevalWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> extern consteval int TemplateOneExternConstevalWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateOneExternConstevalWithDefinitionZeroParametersInt<char>() == 0);
    template <> consteval int TemplateOneExternConstevalWithDefinitionZeroParametersInt<short>() { return 0; }
    extern template int TemplateOneExternConstevalWithDefinitionZeroParametersInt<int>();
    template int TemplateOneExternConstevalWithDefinitionZeroParametersInt<long>();

    template <typename FirstType>
    extern consteval auto TemplateOneExternConstevalWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateOneExternConstevalWithDefinitionOneParameterAuto<char>(0) == 0);
    template <> consteval auto TemplateOneExternConstevalWithDefinitionOneParameterAuto<short>(int First) { return 0; }
    extern template auto TemplateOneExternConstevalWithDefinitionOneParameterAuto<int>(int First);
    template auto TemplateOneExternConstevalWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType>
    extern consteval void TemplateOneExternConstevalWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateOneExternConstevalWithDefinitionOneParameterVoid<char>(0), true));
    template <> consteval void TemplateOneExternConstevalWithDefinitionOneParameterVoid<short>(int First) {}
    extern template void TemplateOneExternConstevalWithDefinitionOneParameterVoid<int>(int First);
    template void TemplateOneExternConstevalWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType>
    extern consteval int TemplateOneExternConstevalWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateOneExternConstevalWithDefinitionOneParameterInt<char>(0) == 0);
    template <> consteval int TemplateOneExternConstevalWithDefinitionOneParameterInt<short>(int First) { return 0; }
    extern template int TemplateOneExternConstevalWithDefinitionOneParameterInt<int>(int First);
    template int TemplateOneExternConstevalWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    extern consteval auto TemplateOneExternConstevalWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneExternConstevalWithDefinitionTwoParametersAuto<char>(0, 0) == 0);
    template <>
    consteval auto TemplateOneExternConstevalWithDefinitionTwoParametersAuto<short>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateOneExternConstevalWithDefinitionTwoParametersAuto<int>(int First, long Second);
    template auto TemplateOneExternConstevalWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    extern consteval void TemplateOneExternConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateOneExternConstevalWithDefinitionTwoParametersVoid<char>(0, 0), true));
    template <>
    consteval void TemplateOneExternConstevalWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    extern template void TemplateOneExternConstevalWithDefinitionTwoParametersVoid<int>(int First, long Second);
    template void TemplateOneExternConstevalWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    extern consteval int TemplateOneExternConstevalWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneExternConstevalWithDefinitionTwoParametersInt<char>(0, 0) == 0);
    template <> consteval int TemplateOneExternConstevalWithDefinitionTwoParametersInt<short>(int First, long Second) {
        return 0;
    }
    extern template int TemplateOneExternConstevalWithDefinitionTwoParametersInt<int>(int First, long Second);
    template int TemplateOneExternConstevalWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Static storage,
    // None evaluation, WithoutDefinition
    template <typename FirstType> static auto TemplateOneStaticNoneWithoutDefinitionZeroParametersAuto();
    template <> auto TemplateOneStaticNoneWithoutDefinitionZeroParametersAuto<short>();

    template <typename FirstType> static void TemplateOneStaticNoneWithoutDefinitionZeroParametersVoid();
    template <> void TemplateOneStaticNoneWithoutDefinitionZeroParametersVoid<short>();

    template <typename FirstType> static int TemplateOneStaticNoneWithoutDefinitionZeroParametersInt();
    template <> int TemplateOneStaticNoneWithoutDefinitionZeroParametersInt<short>();

    template <typename FirstType> static auto TemplateOneStaticNoneWithoutDefinitionOneParameterAuto(int First);
    template <> auto TemplateOneStaticNoneWithoutDefinitionOneParameterAuto<short>(int First);

    template <typename FirstType> static void TemplateOneStaticNoneWithoutDefinitionOneParameterVoid(int First);
    template <> void TemplateOneStaticNoneWithoutDefinitionOneParameterVoid<short>(int First);

    template <typename FirstType> static int TemplateOneStaticNoneWithoutDefinitionOneParameterInt(int First);
    template <> int TemplateOneStaticNoneWithoutDefinitionOneParameterInt<short>(int First);

    template <typename FirstType>
    static auto TemplateOneStaticNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <> auto TemplateOneStaticNoneWithoutDefinitionTwoParametersAuto<short>(int First, long Second);

    template <typename FirstType>
    static void TemplateOneStaticNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <> void TemplateOneStaticNoneWithoutDefinitionTwoParametersVoid<short>(int First, long Second);

    template <typename FirstType>
    static int TemplateOneStaticNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    template <> int TemplateOneStaticNoneWithoutDefinitionTwoParametersInt<short>(int First, long Second);

    // One template parameter(s), Static storage,
    // None evaluation, WithDefinition
    template <typename FirstType> static auto TemplateOneStaticNoneWithDefinitionZeroParametersAuto() { return 0; }
    inline auto* TemplateOneStaticNoneWithDefinitionZeroParametersAutoImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionZeroParametersAuto<char>;
    template <> auto TemplateOneStaticNoneWithDefinitionZeroParametersAuto<short>() { return 0; }
    template auto TemplateOneStaticNoneWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> static void TemplateOneStaticNoneWithDefinitionZeroParametersVoid() {}
    inline auto* TemplateOneStaticNoneWithDefinitionZeroParametersVoidImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionZeroParametersVoid<char>;
    template <> void TemplateOneStaticNoneWithDefinitionZeroParametersVoid<short>() {}
    template void TemplateOneStaticNoneWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> static int TemplateOneStaticNoneWithDefinitionZeroParametersInt() { return 0; }
    inline auto* TemplateOneStaticNoneWithDefinitionZeroParametersIntImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionZeroParametersInt<char>;
    template <> int TemplateOneStaticNoneWithDefinitionZeroParametersInt<short>() { return 0; }
    template int TemplateOneStaticNoneWithDefinitionZeroParametersInt<long>();

    template <typename FirstType> static auto TemplateOneStaticNoneWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    inline auto* TemplateOneStaticNoneWithDefinitionOneParameterAutoImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionOneParameterAuto<char>;
    template <> auto TemplateOneStaticNoneWithDefinitionOneParameterAuto<short>(int First) { return 0; }
    template auto TemplateOneStaticNoneWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType> static void TemplateOneStaticNoneWithDefinitionOneParameterVoid(int First) {}
    inline auto* TemplateOneStaticNoneWithDefinitionOneParameterVoidImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionOneParameterVoid<char>;
    template <> void TemplateOneStaticNoneWithDefinitionOneParameterVoid<short>(int First) {}
    template void TemplateOneStaticNoneWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType> static int TemplateOneStaticNoneWithDefinitionOneParameterInt(int First) { return 0; }
    inline auto* TemplateOneStaticNoneWithDefinitionOneParameterIntImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionOneParameterInt<char>;
    template <> int TemplateOneStaticNoneWithDefinitionOneParameterInt<short>(int First) { return 0; }
    template int TemplateOneStaticNoneWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    static auto TemplateOneStaticNoneWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    inline auto* TemplateOneStaticNoneWithDefinitionTwoParametersAutoImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionTwoParametersAuto<char>;
    template <> auto TemplateOneStaticNoneWithDefinitionTwoParametersAuto<short>(int First, long Second) { return 0; }
    template auto TemplateOneStaticNoneWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    static void TemplateOneStaticNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    inline auto* TemplateOneStaticNoneWithDefinitionTwoParametersVoidImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionTwoParametersVoid<char>;
    template <> void TemplateOneStaticNoneWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    template void TemplateOneStaticNoneWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    static int TemplateOneStaticNoneWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    inline auto* TemplateOneStaticNoneWithDefinitionTwoParametersIntImplicitAnchor =
        &TemplateOneStaticNoneWithDefinitionTwoParametersInt<char>;
    template <> int TemplateOneStaticNoneWithDefinitionTwoParametersInt<short>(int First, long Second) { return 0; }
    template int TemplateOneStaticNoneWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Static storage,
    // Constexpr evaluation, WithoutDefinition
    template <typename FirstType> static constexpr auto TemplateOneStaticConstexprWithoutDefinitionZeroParametersAuto();
    template <> constexpr auto TemplateOneStaticConstexprWithoutDefinitionZeroParametersAuto<short>();

    template <typename FirstType> static constexpr void TemplateOneStaticConstexprWithoutDefinitionZeroParametersVoid();
    template <> constexpr void TemplateOneStaticConstexprWithoutDefinitionZeroParametersVoid<short>();

    template <typename FirstType> static constexpr int TemplateOneStaticConstexprWithoutDefinitionZeroParametersInt();
    template <> constexpr int TemplateOneStaticConstexprWithoutDefinitionZeroParametersInt<short>();

    template <typename FirstType>
    static constexpr auto TemplateOneStaticConstexprWithoutDefinitionOneParameterAuto(int First);
    template <> constexpr auto TemplateOneStaticConstexprWithoutDefinitionOneParameterAuto<short>(int First);

    template <typename FirstType>
    static constexpr void TemplateOneStaticConstexprWithoutDefinitionOneParameterVoid(int First);
    template <> constexpr void TemplateOneStaticConstexprWithoutDefinitionOneParameterVoid<short>(int First);

    template <typename FirstType>
    static constexpr int TemplateOneStaticConstexprWithoutDefinitionOneParameterInt(int First);
    template <> constexpr int TemplateOneStaticConstexprWithoutDefinitionOneParameterInt<short>(int First);

    template <typename FirstType>
    static constexpr auto TemplateOneStaticConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    constexpr auto TemplateOneStaticConstexprWithoutDefinitionTwoParametersAuto<short>(int First, long Second);

    template <typename FirstType>
    static constexpr void TemplateOneStaticConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    constexpr void TemplateOneStaticConstexprWithoutDefinitionTwoParametersVoid<short>(int First, long Second);

    template <typename FirstType>
    static constexpr int TemplateOneStaticConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    constexpr int TemplateOneStaticConstexprWithoutDefinitionTwoParametersInt<short>(int First, long Second);

    // One template parameter(s), Static storage,
    // Constexpr evaluation, WithDefinition
    template <typename FirstType> static constexpr auto TemplateOneStaticConstexprWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateOneStaticConstexprWithDefinitionZeroParametersAuto<char>() == 0);
    template <> constexpr auto TemplateOneStaticConstexprWithDefinitionZeroParametersAuto<short>() { return 0; }
    template auto TemplateOneStaticConstexprWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> static constexpr void TemplateOneStaticConstexprWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateOneStaticConstexprWithDefinitionZeroParametersVoid<char>(), true));
    template <> constexpr void TemplateOneStaticConstexprWithDefinitionZeroParametersVoid<short>() {}
    template void TemplateOneStaticConstexprWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> static constexpr int TemplateOneStaticConstexprWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateOneStaticConstexprWithDefinitionZeroParametersInt<char>() == 0);
    template <> constexpr int TemplateOneStaticConstexprWithDefinitionZeroParametersInt<short>() { return 0; }
    template int TemplateOneStaticConstexprWithDefinitionZeroParametersInt<long>();

    template <typename FirstType>
    static constexpr auto TemplateOneStaticConstexprWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateOneStaticConstexprWithDefinitionOneParameterAuto<char>(0) == 0);
    template <> constexpr auto TemplateOneStaticConstexprWithDefinitionOneParameterAuto<short>(int First) { return 0; }
    template auto TemplateOneStaticConstexprWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType>
    static constexpr void TemplateOneStaticConstexprWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateOneStaticConstexprWithDefinitionOneParameterVoid<char>(0), true));
    template <> constexpr void TemplateOneStaticConstexprWithDefinitionOneParameterVoid<short>(int First) {}
    template void TemplateOneStaticConstexprWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType>
    static constexpr int TemplateOneStaticConstexprWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateOneStaticConstexprWithDefinitionOneParameterInt<char>(0) == 0);
    template <> constexpr int TemplateOneStaticConstexprWithDefinitionOneParameterInt<short>(int First) { return 0; }
    template int TemplateOneStaticConstexprWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    static constexpr auto TemplateOneStaticConstexprWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneStaticConstexprWithDefinitionTwoParametersAuto<char>(0, 0) == 0);
    template <>
    constexpr auto TemplateOneStaticConstexprWithDefinitionTwoParametersAuto<short>(int First, long Second) {
        return 0;
    }
    template auto TemplateOneStaticConstexprWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    static constexpr void TemplateOneStaticConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateOneStaticConstexprWithDefinitionTwoParametersVoid<char>(0, 0), true));
    template <>
    constexpr void TemplateOneStaticConstexprWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    template void TemplateOneStaticConstexprWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    static constexpr int TemplateOneStaticConstexprWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneStaticConstexprWithDefinitionTwoParametersInt<char>(0, 0) == 0);
    template <> constexpr int TemplateOneStaticConstexprWithDefinitionTwoParametersInt<short>(int First, long Second) {
        return 0;
    }
    template int TemplateOneStaticConstexprWithDefinitionTwoParametersInt<long>(int First, long Second);

    // One template parameter(s), Static storage,
    // Consteval evaluation, WithoutDefinition
    template <typename FirstType> static consteval auto TemplateOneStaticConstevalWithoutDefinitionZeroParametersAuto();
    template <> consteval auto TemplateOneStaticConstevalWithoutDefinitionZeroParametersAuto<short>();

    template <typename FirstType> static consteval void TemplateOneStaticConstevalWithoutDefinitionZeroParametersVoid();
    template <> consteval void TemplateOneStaticConstevalWithoutDefinitionZeroParametersVoid<short>();

    template <typename FirstType> static consteval int TemplateOneStaticConstevalWithoutDefinitionZeroParametersInt();
    template <> consteval int TemplateOneStaticConstevalWithoutDefinitionZeroParametersInt<short>();

    template <typename FirstType>
    static consteval auto TemplateOneStaticConstevalWithoutDefinitionOneParameterAuto(int First);
    template <> consteval auto TemplateOneStaticConstevalWithoutDefinitionOneParameterAuto<short>(int First);

    template <typename FirstType>
    static consteval void TemplateOneStaticConstevalWithoutDefinitionOneParameterVoid(int First);
    template <> consteval void TemplateOneStaticConstevalWithoutDefinitionOneParameterVoid<short>(int First);

    template <typename FirstType>
    static consteval int TemplateOneStaticConstevalWithoutDefinitionOneParameterInt(int First);
    template <> consteval int TemplateOneStaticConstevalWithoutDefinitionOneParameterInt<short>(int First);

    template <typename FirstType>
    static consteval auto TemplateOneStaticConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    consteval auto TemplateOneStaticConstevalWithoutDefinitionTwoParametersAuto<short>(int First, long Second);

    template <typename FirstType>
    static consteval void TemplateOneStaticConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    consteval void TemplateOneStaticConstevalWithoutDefinitionTwoParametersVoid<short>(int First, long Second);

    template <typename FirstType>
    static consteval int TemplateOneStaticConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    consteval int TemplateOneStaticConstevalWithoutDefinitionTwoParametersInt<short>(int First, long Second);

    // One template parameter(s), Static storage,
    // Consteval evaluation, WithDefinition
    template <typename FirstType> static consteval auto TemplateOneStaticConstevalWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateOneStaticConstevalWithDefinitionZeroParametersAuto<char>() == 0);
    template <> consteval auto TemplateOneStaticConstevalWithDefinitionZeroParametersAuto<short>() { return 0; }
    template auto TemplateOneStaticConstevalWithDefinitionZeroParametersAuto<long>();

    template <typename FirstType> static consteval void TemplateOneStaticConstevalWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateOneStaticConstevalWithDefinitionZeroParametersVoid<char>(), true));
    template <> consteval void TemplateOneStaticConstevalWithDefinitionZeroParametersVoid<short>() {}
    template void TemplateOneStaticConstevalWithDefinitionZeroParametersVoid<long>();

    template <typename FirstType> static consteval int TemplateOneStaticConstevalWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateOneStaticConstevalWithDefinitionZeroParametersInt<char>() == 0);
    template <> consteval int TemplateOneStaticConstevalWithDefinitionZeroParametersInt<short>() { return 0; }
    template int TemplateOneStaticConstevalWithDefinitionZeroParametersInt<long>();

    template <typename FirstType>
    static consteval auto TemplateOneStaticConstevalWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateOneStaticConstevalWithDefinitionOneParameterAuto<char>(0) == 0);
    template <> consteval auto TemplateOneStaticConstevalWithDefinitionOneParameterAuto<short>(int First) { return 0; }
    template auto TemplateOneStaticConstevalWithDefinitionOneParameterAuto<long>(int First);

    template <typename FirstType>
    static consteval void TemplateOneStaticConstevalWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateOneStaticConstevalWithDefinitionOneParameterVoid<char>(0), true));
    template <> consteval void TemplateOneStaticConstevalWithDefinitionOneParameterVoid<short>(int First) {}
    template void TemplateOneStaticConstevalWithDefinitionOneParameterVoid<long>(int First);

    template <typename FirstType>
    static consteval int TemplateOneStaticConstevalWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateOneStaticConstevalWithDefinitionOneParameterInt<char>(0) == 0);
    template <> consteval int TemplateOneStaticConstevalWithDefinitionOneParameterInt<short>(int First) { return 0; }
    template int TemplateOneStaticConstevalWithDefinitionOneParameterInt<long>(int First);

    template <typename FirstType>
    static consteval auto TemplateOneStaticConstevalWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneStaticConstevalWithDefinitionTwoParametersAuto<char>(0, 0) == 0);
    template <>
    consteval auto TemplateOneStaticConstevalWithDefinitionTwoParametersAuto<short>(int First, long Second) {
        return 0;
    }
    template auto TemplateOneStaticConstevalWithDefinitionTwoParametersAuto<long>(int First, long Second);

    template <typename FirstType>
    static consteval void TemplateOneStaticConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateOneStaticConstevalWithDefinitionTwoParametersVoid<char>(0, 0), true));
    template <>
    consteval void TemplateOneStaticConstevalWithDefinitionTwoParametersVoid<short>(int First, long Second) {}
    template void TemplateOneStaticConstevalWithDefinitionTwoParametersVoid<long>(int First, long Second);

    template <typename FirstType>
    static consteval int TemplateOneStaticConstevalWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateOneStaticConstevalWithDefinitionTwoParametersInt<char>(0, 0) == 0);
    template <> consteval int TemplateOneStaticConstevalWithDefinitionTwoParametersInt<short>(int First, long Second) {
        return 0;
    }
    template int TemplateOneStaticConstevalWithDefinitionTwoParametersInt<long>(int First, long Second);

    // Two template parameter(s), Unspecified storage,
    // None evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    auto TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto();
    template <> auto TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto<short, int>();
    extern template auto TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersAuto<int, long>();

    template <typename FirstType, typename SecondType>
    void TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid();
    template <> void TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid<short, int>();
    extern template void TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersVoid<int, long>();

    template <typename FirstType, typename SecondType>
    int TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt();
    template <> int TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt<short, int>();
    extern template int TemplateTwoUnspecifiedNoneWithoutDefinitionZeroParametersInt<int, long>();

    template <typename FirstType, typename SecondType>
    auto TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto(int First);
    template <> auto TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto<short, int>(int First);
    extern template auto TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterAuto<int, long>(int First);

    template <typename FirstType, typename SecondType>
    void TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid(int First);
    template <> void TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid<short, int>(int First);
    extern template void TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterVoid<int, long>(int First);

    template <typename FirstType, typename SecondType>
    int TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt(int First);
    template <> int TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt<short, int>(int First);
    extern template int TemplateTwoUnspecifiedNoneWithoutDefinitionOneParameterInt<int, long>(int First);

    template <typename FirstType, typename SecondType>
    auto TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <> auto TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto<short, int>(int First, long Second);
    extern template auto TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersAuto<int, long>(int First,
                                                                                                 long Second);

    template <typename FirstType, typename SecondType>
    void TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <> void TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid<short, int>(int First, long Second);
    extern template void TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersVoid<int, long>(int First,
                                                                                                 long Second);

    template <typename FirstType, typename SecondType>
    int TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    template <> int TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);
    extern template int TemplateTwoUnspecifiedNoneWithoutDefinitionTwoParametersInt<int, long>(int First, long Second);

    // Two template parameter(s), Unspecified storage,
    // None evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    auto TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto() {
        return 0;
    }
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAutoImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto<char, short>;
    template <> auto TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto<short, int>() { return 0; }
    extern template auto TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto<int, long>();
    template auto TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    void TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid() {}
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoidImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid<char, short>;
    template <> void TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid<short, int>() {}
    extern template void TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid<int, long>();
    template void TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType> int TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt() {
        return 0;
    }
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersIntImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt<char, short>;
    template <> int TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt<short, int>() { return 0; }
    extern template int TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt<int, long>();
    template int TemplateTwoUnspecifiedNoneWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    auto TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAutoImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto<char, short>;
    template <> auto TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto<short, int>(int First) { return 0; }
    extern template auto TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto<int, long>(int First);
    template auto TemplateTwoUnspecifiedNoneWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    void TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid(int First) {}
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoidImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid<char, short>;
    template <> void TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid<short, int>(int First) {}
    extern template void TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid<int, long>(int First);
    template void TemplateTwoUnspecifiedNoneWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    int TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionOneParameterIntImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt<char, short>;
    template <> int TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt<short, int>(int First) { return 0; }
    extern template int TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt<int, long>(int First);
    template int TemplateTwoUnspecifiedNoneWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    auto TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAutoImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto<char, short>;
    template <> auto TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto<int, long>(int First, long Second);
    template auto TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersAuto<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    void TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoidImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid<char, short>;
    template <> void TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    extern template void TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid<int, long>(int First, long Second);
    template void TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersVoid<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    int TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    inline auto* TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersIntImplicitAnchor =
        &TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt<char, short>;
    template <> int TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    extern template int TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt<int, long>(int First, long Second);
    template int TemplateTwoUnspecifiedNoneWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Unspecified storage,
    // Constexpr evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    constexpr auto TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto();
    template <> constexpr auto TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto<short, int>();
    extern template auto TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersAuto<int, long>();

    template <typename FirstType, typename SecondType>
    constexpr void TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid();
    template <> constexpr void TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid<short, int>();
    extern template void TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersVoid<int, long>();

    template <typename FirstType, typename SecondType>
    constexpr int TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt();
    template <> constexpr int TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt<short, int>();
    extern template int TemplateTwoUnspecifiedConstexprWithoutDefinitionZeroParametersInt<int, long>();

    template <typename FirstType, typename SecondType>
    constexpr auto TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto(int First);
    template <> constexpr auto TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto<short, int>(int First);
    extern template auto TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterAuto<int, long>(int First);

    template <typename FirstType, typename SecondType>
    constexpr void TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid(int First);
    template <> constexpr void TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid<short, int>(int First);
    extern template void TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterVoid<int, long>(int First);

    template <typename FirstType, typename SecondType>
    constexpr int TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt(int First);
    template <> constexpr int TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt<short, int>(int First);
    extern template int TemplateTwoUnspecifiedConstexprWithoutDefinitionOneParameterInt<int, long>(int First);

    template <typename FirstType, typename SecondType>
    constexpr auto TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    constexpr auto TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto<short, int>(int First,
                                                                                                 long Second);
    extern template auto TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersAuto<int, long>(int First,
                                                                                                      long Second);

    template <typename FirstType, typename SecondType>
    constexpr void TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    constexpr void TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid<short, int>(int First,
                                                                                                 long Second);
    extern template void TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersVoid<int, long>(int First,
                                                                                                      long Second);

    template <typename FirstType, typename SecondType>
    constexpr int TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    constexpr int TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);
    extern template int TemplateTwoUnspecifiedConstexprWithoutDefinitionTwoParametersInt<int, long>(int First,
                                                                                                    long Second);

    // Two template parameter(s), Unspecified storage,
    // Constexpr evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    constexpr auto TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto<char, short>() == 0);
    template <> constexpr auto TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto<short, int>() {
        return 0;
    }
    extern template auto TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto<int, long>();
    template auto TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    constexpr void TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid<char, short>(), true));
    template <> constexpr void TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid<short, int>() {}
    extern template void TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid<int, long>();
    template void TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    constexpr int TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt<char, short>() == 0);
    template <> constexpr int TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt<short, int>() { return 0; }
    extern template int TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt<int, long>();
    template int TemplateTwoUnspecifiedConstexprWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    constexpr auto TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto<char, short>(0) == 0);
    template <> constexpr auto TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto<short, int>(int First) {
        return 0;
    }
    extern template auto TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto<int, long>(int First);
    template auto TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    constexpr void TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid<char, short>(0), true));
    template <> constexpr void TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid<short, int>(int First) {}
    extern template void TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid<int, long>(int First);
    template void TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    constexpr int TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt<char, short>(0) == 0);
    template <> constexpr int TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt<short, int>(int First) {
        return 0;
    }
    extern template int TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt<int, long>(int First);
    template int TemplateTwoUnspecifiedConstexprWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    constexpr auto TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto<char, short>(0, 0) == 0);
    template <>
    constexpr auto TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto<int, long>(int First,
                                                                                                   long Second);
    template auto TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersAuto<long, long long>(int First,
                                                                                                  long Second);

    template <typename FirstType, typename SecondType>
    constexpr void TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid<char, short>(0, 0), true));
    template <>
    constexpr void TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    extern template void TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid<int, long>(int First,
                                                                                                   long Second);
    template void TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersVoid<long, long long>(int First,
                                                                                                  long Second);

    template <typename FirstType, typename SecondType>
    constexpr int TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt<char, short>(0, 0) == 0);
    template <>
    constexpr int TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    extern template int TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt<int, long>(int First,
                                                                                                 long Second);
    template int TemplateTwoUnspecifiedConstexprWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Unspecified storage,
    // Consteval evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    consteval auto TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto();
    template <> consteval auto TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto<short, int>();
    extern template auto TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersAuto<int, long>();

    template <typename FirstType, typename SecondType>
    consteval void TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid();
    template <> consteval void TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid<short, int>();
    extern template void TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersVoid<int, long>();

    template <typename FirstType, typename SecondType>
    consteval int TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt();
    template <> consteval int TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt<short, int>();
    extern template int TemplateTwoUnspecifiedConstevalWithoutDefinitionZeroParametersInt<int, long>();

    template <typename FirstType, typename SecondType>
    consteval auto TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto(int First);
    template <> consteval auto TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto<short, int>(int First);
    extern template auto TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterAuto<int, long>(int First);

    template <typename FirstType, typename SecondType>
    consteval void TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid(int First);
    template <> consteval void TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid<short, int>(int First);
    extern template void TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterVoid<int, long>(int First);

    template <typename FirstType, typename SecondType>
    consteval int TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt(int First);
    template <> consteval int TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt<short, int>(int First);
    extern template int TemplateTwoUnspecifiedConstevalWithoutDefinitionOneParameterInt<int, long>(int First);

    template <typename FirstType, typename SecondType>
    consteval auto TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    consteval auto TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto<short, int>(int First,
                                                                                                 long Second);
    extern template auto TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersAuto<int, long>(int First,
                                                                                                      long Second);

    template <typename FirstType, typename SecondType>
    consteval void TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    consteval void TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid<short, int>(int First,
                                                                                                 long Second);
    extern template void TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersVoid<int, long>(int First,
                                                                                                      long Second);

    template <typename FirstType, typename SecondType>
    consteval int TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    consteval int TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);
    extern template int TemplateTwoUnspecifiedConstevalWithoutDefinitionTwoParametersInt<int, long>(int First,
                                                                                                    long Second);

    // Two template parameter(s), Unspecified storage,
    // Consteval evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    consteval auto TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto<char, short>() == 0);
    template <> consteval auto TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto<short, int>() {
        return 0;
    }
    extern template auto TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto<int, long>();
    template auto TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    consteval void TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid<char, short>(), true));
    template <> consteval void TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid<short, int>() {}
    extern template void TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid<int, long>();
    template void TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    consteval int TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt<char, short>() == 0);
    template <> consteval int TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt<short, int>() { return 0; }
    extern template int TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt<int, long>();
    template int TemplateTwoUnspecifiedConstevalWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    consteval auto TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto<char, short>(0) == 0);
    template <> consteval auto TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto<short, int>(int First) {
        return 0;
    }
    extern template auto TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto<int, long>(int First);
    template auto TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    consteval void TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid<char, short>(0), true));
    template <> consteval void TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid<short, int>(int First) {}
    extern template void TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid<int, long>(int First);
    template void TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    consteval int TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt<char, short>(0) == 0);
    template <> consteval int TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt<short, int>(int First) {
        return 0;
    }
    extern template int TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt<int, long>(int First);
    template int TemplateTwoUnspecifiedConstevalWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    consteval auto TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto<char, short>(0, 0) == 0);
    template <>
    consteval auto TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto<int, long>(int First,
                                                                                                   long Second);
    template auto TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersAuto<long, long long>(int First,
                                                                                                  long Second);

    template <typename FirstType, typename SecondType>
    consteval void TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid<char, short>(0, 0), true));
    template <>
    consteval void TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    extern template void TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid<int, long>(int First,
                                                                                                   long Second);
    template void TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersVoid<long, long long>(int First,
                                                                                                  long Second);

    template <typename FirstType, typename SecondType>
    consteval int TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt<char, short>(0, 0) == 0);
    template <>
    consteval int TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    extern template int TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt<int, long>(int First,
                                                                                                 long Second);
    template int TemplateTwoUnspecifiedConstevalWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Extern storage,
    // None evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    extern auto TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto();
    template <> auto TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto<short, int>();
    extern template auto TemplateTwoExternNoneWithoutDefinitionZeroParametersAuto<int, long>();

    template <typename FirstType, typename SecondType>
    extern void TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid();
    template <> void TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid<short, int>();
    extern template void TemplateTwoExternNoneWithoutDefinitionZeroParametersVoid<int, long>();

    template <typename FirstType, typename SecondType>
    extern int TemplateTwoExternNoneWithoutDefinitionZeroParametersInt();
    template <> int TemplateTwoExternNoneWithoutDefinitionZeroParametersInt<short, int>();
    extern template int TemplateTwoExternNoneWithoutDefinitionZeroParametersInt<int, long>();

    template <typename FirstType, typename SecondType>
    extern auto TemplateTwoExternNoneWithoutDefinitionOneParameterAuto(int First);
    template <> auto TemplateTwoExternNoneWithoutDefinitionOneParameterAuto<short, int>(int First);
    extern template auto TemplateTwoExternNoneWithoutDefinitionOneParameterAuto<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern void TemplateTwoExternNoneWithoutDefinitionOneParameterVoid(int First);
    template <> void TemplateTwoExternNoneWithoutDefinitionOneParameterVoid<short, int>(int First);
    extern template void TemplateTwoExternNoneWithoutDefinitionOneParameterVoid<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern int TemplateTwoExternNoneWithoutDefinitionOneParameterInt(int First);
    template <> int TemplateTwoExternNoneWithoutDefinitionOneParameterInt<short, int>(int First);
    extern template int TemplateTwoExternNoneWithoutDefinitionOneParameterInt<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern auto TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <> auto TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto<short, int>(int First, long Second);
    extern template auto TemplateTwoExternNoneWithoutDefinitionTwoParametersAuto<int, long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern void TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <> void TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid<short, int>(int First, long Second);
    extern template void TemplateTwoExternNoneWithoutDefinitionTwoParametersVoid<int, long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern int TemplateTwoExternNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    template <> int TemplateTwoExternNoneWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);
    extern template int TemplateTwoExternNoneWithoutDefinitionTwoParametersInt<int, long>(int First, long Second);

    // Two template parameter(s), Extern storage,
    // None evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    extern auto TemplateTwoExternNoneWithDefinitionZeroParametersAuto() {
        return 0;
    }
    inline auto* TemplateTwoExternNoneWithDefinitionZeroParametersAutoImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionZeroParametersAuto<char, short>;
    template <> auto TemplateTwoExternNoneWithDefinitionZeroParametersAuto<short, int>() { return 0; }
    extern template auto TemplateTwoExternNoneWithDefinitionZeroParametersAuto<int, long>();
    template auto TemplateTwoExternNoneWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    extern void TemplateTwoExternNoneWithDefinitionZeroParametersVoid() {}
    inline auto* TemplateTwoExternNoneWithDefinitionZeroParametersVoidImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionZeroParametersVoid<char, short>;
    template <> void TemplateTwoExternNoneWithDefinitionZeroParametersVoid<short, int>() {}
    extern template void TemplateTwoExternNoneWithDefinitionZeroParametersVoid<int, long>();
    template void TemplateTwoExternNoneWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    extern int TemplateTwoExternNoneWithDefinitionZeroParametersInt() {
        return 0;
    }
    inline auto* TemplateTwoExternNoneWithDefinitionZeroParametersIntImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionZeroParametersInt<char, short>;
    template <> int TemplateTwoExternNoneWithDefinitionZeroParametersInt<short, int>() { return 0; }
    extern template int TemplateTwoExternNoneWithDefinitionZeroParametersInt<int, long>();
    template int TemplateTwoExternNoneWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    extern auto TemplateTwoExternNoneWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    inline auto* TemplateTwoExternNoneWithDefinitionOneParameterAutoImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionOneParameterAuto<char, short>;
    template <> auto TemplateTwoExternNoneWithDefinitionOneParameterAuto<short, int>(int First) { return 0; }
    extern template auto TemplateTwoExternNoneWithDefinitionOneParameterAuto<int, long>(int First);
    template auto TemplateTwoExternNoneWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern void TemplateTwoExternNoneWithDefinitionOneParameterVoid(int First) {}
    inline auto* TemplateTwoExternNoneWithDefinitionOneParameterVoidImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionOneParameterVoid<char, short>;
    template <> void TemplateTwoExternNoneWithDefinitionOneParameterVoid<short, int>(int First) {}
    extern template void TemplateTwoExternNoneWithDefinitionOneParameterVoid<int, long>(int First);
    template void TemplateTwoExternNoneWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern int TemplateTwoExternNoneWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    inline auto* TemplateTwoExternNoneWithDefinitionOneParameterIntImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionOneParameterInt<char, short>;
    template <> int TemplateTwoExternNoneWithDefinitionOneParameterInt<short, int>(int First) { return 0; }
    extern template int TemplateTwoExternNoneWithDefinitionOneParameterInt<int, long>(int First);
    template int TemplateTwoExternNoneWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern auto TemplateTwoExternNoneWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    inline auto* TemplateTwoExternNoneWithDefinitionTwoParametersAutoImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionTwoParametersAuto<char, short>;
    template <> auto TemplateTwoExternNoneWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateTwoExternNoneWithDefinitionTwoParametersAuto<int, long>(int First, long Second);
    template auto TemplateTwoExternNoneWithDefinitionTwoParametersAuto<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern void TemplateTwoExternNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    inline auto* TemplateTwoExternNoneWithDefinitionTwoParametersVoidImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionTwoParametersVoid<char, short>;
    template <> void TemplateTwoExternNoneWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    extern template void TemplateTwoExternNoneWithDefinitionTwoParametersVoid<int, long>(int First, long Second);
    template void TemplateTwoExternNoneWithDefinitionTwoParametersVoid<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern int TemplateTwoExternNoneWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    inline auto* TemplateTwoExternNoneWithDefinitionTwoParametersIntImplicitAnchor =
        &TemplateTwoExternNoneWithDefinitionTwoParametersInt<char, short>;
    template <> int TemplateTwoExternNoneWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    extern template int TemplateTwoExternNoneWithDefinitionTwoParametersInt<int, long>(int First, long Second);
    template int TemplateTwoExternNoneWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Extern storage,
    // Constexpr evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    extern constexpr auto TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto();
    template <> constexpr auto TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto<short, int>();
    extern template auto TemplateTwoExternConstexprWithoutDefinitionZeroParametersAuto<int, long>();

    template <typename FirstType, typename SecondType>
    extern constexpr void TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid();
    template <> constexpr void TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid<short, int>();
    extern template void TemplateTwoExternConstexprWithoutDefinitionZeroParametersVoid<int, long>();

    template <typename FirstType, typename SecondType>
    extern constexpr int TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt();
    template <> constexpr int TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt<short, int>();
    extern template int TemplateTwoExternConstexprWithoutDefinitionZeroParametersInt<int, long>();

    template <typename FirstType, typename SecondType>
    extern constexpr auto TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto(int First);
    template <> constexpr auto TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto<short, int>(int First);
    extern template auto TemplateTwoExternConstexprWithoutDefinitionOneParameterAuto<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern constexpr void TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid(int First);
    template <> constexpr void TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid<short, int>(int First);
    extern template void TemplateTwoExternConstexprWithoutDefinitionOneParameterVoid<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern constexpr int TemplateTwoExternConstexprWithoutDefinitionOneParameterInt(int First);
    template <> constexpr int TemplateTwoExternConstexprWithoutDefinitionOneParameterInt<short, int>(int First);
    extern template int TemplateTwoExternConstexprWithoutDefinitionOneParameterInt<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern constexpr auto TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    constexpr auto TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto<short, int>(int First, long Second);
    extern template auto TemplateTwoExternConstexprWithoutDefinitionTwoParametersAuto<int, long>(int First,
                                                                                                 long Second);

    template <typename FirstType, typename SecondType>
    extern constexpr void TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    constexpr void TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid<short, int>(int First, long Second);
    extern template void TemplateTwoExternConstexprWithoutDefinitionTwoParametersVoid<int, long>(int First,
                                                                                                 long Second);

    template <typename FirstType, typename SecondType>
    extern constexpr int TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    constexpr int TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);
    extern template int TemplateTwoExternConstexprWithoutDefinitionTwoParametersInt<int, long>(int First, long Second);

    // Two template parameter(s), Extern storage,
    // Constexpr evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    extern constexpr auto TemplateTwoExternConstexprWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateTwoExternConstexprWithDefinitionZeroParametersAuto<char, short>() == 0);
    template <> constexpr auto TemplateTwoExternConstexprWithDefinitionZeroParametersAuto<short, int>() { return 0; }
    extern template auto TemplateTwoExternConstexprWithDefinitionZeroParametersAuto<int, long>();
    template auto TemplateTwoExternConstexprWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    extern constexpr void TemplateTwoExternConstexprWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateTwoExternConstexprWithDefinitionZeroParametersVoid<char, short>(), true));
    template <> constexpr void TemplateTwoExternConstexprWithDefinitionZeroParametersVoid<short, int>() {}
    extern template void TemplateTwoExternConstexprWithDefinitionZeroParametersVoid<int, long>();
    template void TemplateTwoExternConstexprWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    extern constexpr int TemplateTwoExternConstexprWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateTwoExternConstexprWithDefinitionZeroParametersInt<char, short>() == 0);
    template <> constexpr int TemplateTwoExternConstexprWithDefinitionZeroParametersInt<short, int>() { return 0; }
    extern template int TemplateTwoExternConstexprWithDefinitionZeroParametersInt<int, long>();
    template int TemplateTwoExternConstexprWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    extern constexpr auto TemplateTwoExternConstexprWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateTwoExternConstexprWithDefinitionOneParameterAuto<char, short>(0) == 0);
    template <> constexpr auto TemplateTwoExternConstexprWithDefinitionOneParameterAuto<short, int>(int First) {
        return 0;
    }
    extern template auto TemplateTwoExternConstexprWithDefinitionOneParameterAuto<int, long>(int First);
    template auto TemplateTwoExternConstexprWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern constexpr void TemplateTwoExternConstexprWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateTwoExternConstexprWithDefinitionOneParameterVoid<char, short>(0), true));
    template <> constexpr void TemplateTwoExternConstexprWithDefinitionOneParameterVoid<short, int>(int First) {}
    extern template void TemplateTwoExternConstexprWithDefinitionOneParameterVoid<int, long>(int First);
    template void TemplateTwoExternConstexprWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern constexpr int TemplateTwoExternConstexprWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateTwoExternConstexprWithDefinitionOneParameterInt<char, short>(0) == 0);
    template <> constexpr int TemplateTwoExternConstexprWithDefinitionOneParameterInt<short, int>(int First) {
        return 0;
    }
    extern template int TemplateTwoExternConstexprWithDefinitionOneParameterInt<int, long>(int First);
    template int TemplateTwoExternConstexprWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern constexpr auto TemplateTwoExternConstexprWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoExternConstexprWithDefinitionTwoParametersAuto<char, short>(0, 0) == 0);
    template <>
    constexpr auto TemplateTwoExternConstexprWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateTwoExternConstexprWithDefinitionTwoParametersAuto<int, long>(int First, long Second);
    template auto TemplateTwoExternConstexprWithDefinitionTwoParametersAuto<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern constexpr void TemplateTwoExternConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateTwoExternConstexprWithDefinitionTwoParametersVoid<char, short>(0, 0), true));
    template <>
    constexpr void TemplateTwoExternConstexprWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    extern template void TemplateTwoExternConstexprWithDefinitionTwoParametersVoid<int, long>(int First, long Second);
    template void TemplateTwoExternConstexprWithDefinitionTwoParametersVoid<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern constexpr int TemplateTwoExternConstexprWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoExternConstexprWithDefinitionTwoParametersInt<char, short>(0, 0) == 0);
    template <>
    constexpr int TemplateTwoExternConstexprWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    extern template int TemplateTwoExternConstexprWithDefinitionTwoParametersInt<int, long>(int First, long Second);
    template int TemplateTwoExternConstexprWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Extern storage,
    // Consteval evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    extern consteval auto TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto();
    template <> consteval auto TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto<short, int>();
    extern template auto TemplateTwoExternConstevalWithoutDefinitionZeroParametersAuto<int, long>();

    template <typename FirstType, typename SecondType>
    extern consteval void TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid();
    template <> consteval void TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid<short, int>();
    extern template void TemplateTwoExternConstevalWithoutDefinitionZeroParametersVoid<int, long>();

    template <typename FirstType, typename SecondType>
    extern consteval int TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt();
    template <> consteval int TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt<short, int>();
    extern template int TemplateTwoExternConstevalWithoutDefinitionZeroParametersInt<int, long>();

    template <typename FirstType, typename SecondType>
    extern consteval auto TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto(int First);
    template <> consteval auto TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto<short, int>(int First);
    extern template auto TemplateTwoExternConstevalWithoutDefinitionOneParameterAuto<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern consteval void TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid(int First);
    template <> consteval void TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid<short, int>(int First);
    extern template void TemplateTwoExternConstevalWithoutDefinitionOneParameterVoid<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern consteval int TemplateTwoExternConstevalWithoutDefinitionOneParameterInt(int First);
    template <> consteval int TemplateTwoExternConstevalWithoutDefinitionOneParameterInt<short, int>(int First);
    extern template int TemplateTwoExternConstevalWithoutDefinitionOneParameterInt<int, long>(int First);

    template <typename FirstType, typename SecondType>
    extern consteval auto TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    consteval auto TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto<short, int>(int First, long Second);
    extern template auto TemplateTwoExternConstevalWithoutDefinitionTwoParametersAuto<int, long>(int First,
                                                                                                 long Second);

    template <typename FirstType, typename SecondType>
    extern consteval void TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    consteval void TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid<short, int>(int First, long Second);
    extern template void TemplateTwoExternConstevalWithoutDefinitionTwoParametersVoid<int, long>(int First,
                                                                                                 long Second);

    template <typename FirstType, typename SecondType>
    extern consteval int TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    consteval int TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);
    extern template int TemplateTwoExternConstevalWithoutDefinitionTwoParametersInt<int, long>(int First, long Second);

    // Two template parameter(s), Extern storage,
    // Consteval evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    extern consteval auto TemplateTwoExternConstevalWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateTwoExternConstevalWithDefinitionZeroParametersAuto<char, short>() == 0);
    template <> consteval auto TemplateTwoExternConstevalWithDefinitionZeroParametersAuto<short, int>() { return 0; }
    extern template auto TemplateTwoExternConstevalWithDefinitionZeroParametersAuto<int, long>();
    template auto TemplateTwoExternConstevalWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    extern consteval void TemplateTwoExternConstevalWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateTwoExternConstevalWithDefinitionZeroParametersVoid<char, short>(), true));
    template <> consteval void TemplateTwoExternConstevalWithDefinitionZeroParametersVoid<short, int>() {}
    extern template void TemplateTwoExternConstevalWithDefinitionZeroParametersVoid<int, long>();
    template void TemplateTwoExternConstevalWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    extern consteval int TemplateTwoExternConstevalWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateTwoExternConstevalWithDefinitionZeroParametersInt<char, short>() == 0);
    template <> consteval int TemplateTwoExternConstevalWithDefinitionZeroParametersInt<short, int>() { return 0; }
    extern template int TemplateTwoExternConstevalWithDefinitionZeroParametersInt<int, long>();
    template int TemplateTwoExternConstevalWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    extern consteval auto TemplateTwoExternConstevalWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateTwoExternConstevalWithDefinitionOneParameterAuto<char, short>(0) == 0);
    template <> consteval auto TemplateTwoExternConstevalWithDefinitionOneParameterAuto<short, int>(int First) {
        return 0;
    }
    extern template auto TemplateTwoExternConstevalWithDefinitionOneParameterAuto<int, long>(int First);
    template auto TemplateTwoExternConstevalWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern consteval void TemplateTwoExternConstevalWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateTwoExternConstevalWithDefinitionOneParameterVoid<char, short>(0), true));
    template <> consteval void TemplateTwoExternConstevalWithDefinitionOneParameterVoid<short, int>(int First) {}
    extern template void TemplateTwoExternConstevalWithDefinitionOneParameterVoid<int, long>(int First);
    template void TemplateTwoExternConstevalWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern consteval int TemplateTwoExternConstevalWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateTwoExternConstevalWithDefinitionOneParameterInt<char, short>(0) == 0);
    template <> consteval int TemplateTwoExternConstevalWithDefinitionOneParameterInt<short, int>(int First) {
        return 0;
    }
    extern template int TemplateTwoExternConstevalWithDefinitionOneParameterInt<int, long>(int First);
    template int TemplateTwoExternConstevalWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    extern consteval auto TemplateTwoExternConstevalWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoExternConstevalWithDefinitionTwoParametersAuto<char, short>(0, 0) == 0);
    template <>
    consteval auto TemplateTwoExternConstevalWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    extern template auto TemplateTwoExternConstevalWithDefinitionTwoParametersAuto<int, long>(int First, long Second);
    template auto TemplateTwoExternConstevalWithDefinitionTwoParametersAuto<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern consteval void TemplateTwoExternConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateTwoExternConstevalWithDefinitionTwoParametersVoid<char, short>(0, 0), true));
    template <>
    consteval void TemplateTwoExternConstevalWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    extern template void TemplateTwoExternConstevalWithDefinitionTwoParametersVoid<int, long>(int First, long Second);
    template void TemplateTwoExternConstevalWithDefinitionTwoParametersVoid<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    extern consteval int TemplateTwoExternConstevalWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoExternConstevalWithDefinitionTwoParametersInt<char, short>(0, 0) == 0);
    template <>
    consteval int TemplateTwoExternConstevalWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    extern template int TemplateTwoExternConstevalWithDefinitionTwoParametersInt<int, long>(int First, long Second);
    template int TemplateTwoExternConstevalWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Static storage,
    // None evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    static auto TemplateTwoStaticNoneWithoutDefinitionZeroParametersAuto();
    template <> auto TemplateTwoStaticNoneWithoutDefinitionZeroParametersAuto<short, int>();

    template <typename FirstType, typename SecondType>
    static void TemplateTwoStaticNoneWithoutDefinitionZeroParametersVoid();
    template <> void TemplateTwoStaticNoneWithoutDefinitionZeroParametersVoid<short, int>();

    template <typename FirstType, typename SecondType>
    static int TemplateTwoStaticNoneWithoutDefinitionZeroParametersInt();
    template <> int TemplateTwoStaticNoneWithoutDefinitionZeroParametersInt<short, int>();

    template <typename FirstType, typename SecondType>
    static auto TemplateTwoStaticNoneWithoutDefinitionOneParameterAuto(int First);
    template <> auto TemplateTwoStaticNoneWithoutDefinitionOneParameterAuto<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static void TemplateTwoStaticNoneWithoutDefinitionOneParameterVoid(int First);
    template <> void TemplateTwoStaticNoneWithoutDefinitionOneParameterVoid<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static int TemplateTwoStaticNoneWithoutDefinitionOneParameterInt(int First);
    template <> int TemplateTwoStaticNoneWithoutDefinitionOneParameterInt<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static auto TemplateTwoStaticNoneWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <> auto TemplateTwoStaticNoneWithoutDefinitionTwoParametersAuto<short, int>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static void TemplateTwoStaticNoneWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <> void TemplateTwoStaticNoneWithoutDefinitionTwoParametersVoid<short, int>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static int TemplateTwoStaticNoneWithoutDefinitionTwoParametersInt(int First, long Second);
    template <> int TemplateTwoStaticNoneWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);

    // Two template parameter(s), Static storage,
    // None evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    static auto TemplateTwoStaticNoneWithDefinitionZeroParametersAuto() {
        return 0;
    }
    inline auto* TemplateTwoStaticNoneWithDefinitionZeroParametersAutoImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionZeroParametersAuto<char, short>;
    template <> auto TemplateTwoStaticNoneWithDefinitionZeroParametersAuto<short, int>() { return 0; }
    template auto TemplateTwoStaticNoneWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    static void TemplateTwoStaticNoneWithDefinitionZeroParametersVoid() {}
    inline auto* TemplateTwoStaticNoneWithDefinitionZeroParametersVoidImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionZeroParametersVoid<char, short>;
    template <> void TemplateTwoStaticNoneWithDefinitionZeroParametersVoid<short, int>() {}
    template void TemplateTwoStaticNoneWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    static int TemplateTwoStaticNoneWithDefinitionZeroParametersInt() {
        return 0;
    }
    inline auto* TemplateTwoStaticNoneWithDefinitionZeroParametersIntImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionZeroParametersInt<char, short>;
    template <> int TemplateTwoStaticNoneWithDefinitionZeroParametersInt<short, int>() { return 0; }
    template int TemplateTwoStaticNoneWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    static auto TemplateTwoStaticNoneWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    inline auto* TemplateTwoStaticNoneWithDefinitionOneParameterAutoImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionOneParameterAuto<char, short>;
    template <> auto TemplateTwoStaticNoneWithDefinitionOneParameterAuto<short, int>(int First) { return 0; }
    template auto TemplateTwoStaticNoneWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static void TemplateTwoStaticNoneWithDefinitionOneParameterVoid(int First) {}
    inline auto* TemplateTwoStaticNoneWithDefinitionOneParameterVoidImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionOneParameterVoid<char, short>;
    template <> void TemplateTwoStaticNoneWithDefinitionOneParameterVoid<short, int>(int First) {}
    template void TemplateTwoStaticNoneWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static int TemplateTwoStaticNoneWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    inline auto* TemplateTwoStaticNoneWithDefinitionOneParameterIntImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionOneParameterInt<char, short>;
    template <> int TemplateTwoStaticNoneWithDefinitionOneParameterInt<short, int>(int First) { return 0; }
    template int TemplateTwoStaticNoneWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static auto TemplateTwoStaticNoneWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    inline auto* TemplateTwoStaticNoneWithDefinitionTwoParametersAutoImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionTwoParametersAuto<char, short>;
    template <> auto TemplateTwoStaticNoneWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    template auto TemplateTwoStaticNoneWithDefinitionTwoParametersAuto<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static void TemplateTwoStaticNoneWithDefinitionTwoParametersVoid(int First, long Second) {}
    inline auto* TemplateTwoStaticNoneWithDefinitionTwoParametersVoidImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionTwoParametersVoid<char, short>;
    template <> void TemplateTwoStaticNoneWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    template void TemplateTwoStaticNoneWithDefinitionTwoParametersVoid<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static int TemplateTwoStaticNoneWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    inline auto* TemplateTwoStaticNoneWithDefinitionTwoParametersIntImplicitAnchor =
        &TemplateTwoStaticNoneWithDefinitionTwoParametersInt<char, short>;
    template <> int TemplateTwoStaticNoneWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    template int TemplateTwoStaticNoneWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Static storage,
    // Constexpr evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    static constexpr auto TemplateTwoStaticConstexprWithoutDefinitionZeroParametersAuto();
    template <> constexpr auto TemplateTwoStaticConstexprWithoutDefinitionZeroParametersAuto<short, int>();

    template <typename FirstType, typename SecondType>
    static constexpr void TemplateTwoStaticConstexprWithoutDefinitionZeroParametersVoid();
    template <> constexpr void TemplateTwoStaticConstexprWithoutDefinitionZeroParametersVoid<short, int>();

    template <typename FirstType, typename SecondType>
    static constexpr int TemplateTwoStaticConstexprWithoutDefinitionZeroParametersInt();
    template <> constexpr int TemplateTwoStaticConstexprWithoutDefinitionZeroParametersInt<short, int>();

    template <typename FirstType, typename SecondType>
    static constexpr auto TemplateTwoStaticConstexprWithoutDefinitionOneParameterAuto(int First);
    template <> constexpr auto TemplateTwoStaticConstexprWithoutDefinitionOneParameterAuto<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static constexpr void TemplateTwoStaticConstexprWithoutDefinitionOneParameterVoid(int First);
    template <> constexpr void TemplateTwoStaticConstexprWithoutDefinitionOneParameterVoid<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static constexpr int TemplateTwoStaticConstexprWithoutDefinitionOneParameterInt(int First);
    template <> constexpr int TemplateTwoStaticConstexprWithoutDefinitionOneParameterInt<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static constexpr auto TemplateTwoStaticConstexprWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    constexpr auto TemplateTwoStaticConstexprWithoutDefinitionTwoParametersAuto<short, int>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static constexpr void TemplateTwoStaticConstexprWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    constexpr void TemplateTwoStaticConstexprWithoutDefinitionTwoParametersVoid<short, int>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static constexpr int TemplateTwoStaticConstexprWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    constexpr int TemplateTwoStaticConstexprWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);

    // Two template parameter(s), Static storage,
    // Constexpr evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    static constexpr auto TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto<char, short>() == 0);
    template <> constexpr auto TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto<short, int>() { return 0; }
    template auto TemplateTwoStaticConstexprWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    static constexpr void TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid<char, short>(), true));
    template <> constexpr void TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid<short, int>() {}
    template void TemplateTwoStaticConstexprWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    static constexpr int TemplateTwoStaticConstexprWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateTwoStaticConstexprWithDefinitionZeroParametersInt<char, short>() == 0);
    template <> constexpr int TemplateTwoStaticConstexprWithDefinitionZeroParametersInt<short, int>() { return 0; }
    template int TemplateTwoStaticConstexprWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    static constexpr auto TemplateTwoStaticConstexprWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstexprWithDefinitionOneParameterAuto<char, short>(0) == 0);
    template <> constexpr auto TemplateTwoStaticConstexprWithDefinitionOneParameterAuto<short, int>(int First) {
        return 0;
    }
    template auto TemplateTwoStaticConstexprWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static constexpr void TemplateTwoStaticConstexprWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateTwoStaticConstexprWithDefinitionOneParameterVoid<char, short>(0), true));
    template <> constexpr void TemplateTwoStaticConstexprWithDefinitionOneParameterVoid<short, int>(int First) {}
    template void TemplateTwoStaticConstexprWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static constexpr int TemplateTwoStaticConstexprWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstexprWithDefinitionOneParameterInt<char, short>(0) == 0);
    template <> constexpr int TemplateTwoStaticConstexprWithDefinitionOneParameterInt<short, int>(int First) {
        return 0;
    }
    template int TemplateTwoStaticConstexprWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static constexpr auto TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto<char, short>(0, 0) == 0);
    template <>
    constexpr auto TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    template auto TemplateTwoStaticConstexprWithDefinitionTwoParametersAuto<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static constexpr void TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid<char, short>(0, 0), true));
    template <>
    constexpr void TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    template void TemplateTwoStaticConstexprWithDefinitionTwoParametersVoid<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static constexpr int TemplateTwoStaticConstexprWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstexprWithDefinitionTwoParametersInt<char, short>(0, 0) == 0);
    template <>
    constexpr int TemplateTwoStaticConstexprWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    template int TemplateTwoStaticConstexprWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    // Two template parameter(s), Static storage,
    // Consteval evaluation, WithoutDefinition
    template <typename FirstType, typename SecondType>
    static consteval auto TemplateTwoStaticConstevalWithoutDefinitionZeroParametersAuto();
    template <> consteval auto TemplateTwoStaticConstevalWithoutDefinitionZeroParametersAuto<short, int>();

    template <typename FirstType, typename SecondType>
    static consteval void TemplateTwoStaticConstevalWithoutDefinitionZeroParametersVoid();
    template <> consteval void TemplateTwoStaticConstevalWithoutDefinitionZeroParametersVoid<short, int>();

    template <typename FirstType, typename SecondType>
    static consteval int TemplateTwoStaticConstevalWithoutDefinitionZeroParametersInt();
    template <> consteval int TemplateTwoStaticConstevalWithoutDefinitionZeroParametersInt<short, int>();

    template <typename FirstType, typename SecondType>
    static consteval auto TemplateTwoStaticConstevalWithoutDefinitionOneParameterAuto(int First);
    template <> consteval auto TemplateTwoStaticConstevalWithoutDefinitionOneParameterAuto<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static consteval void TemplateTwoStaticConstevalWithoutDefinitionOneParameterVoid(int First);
    template <> consteval void TemplateTwoStaticConstevalWithoutDefinitionOneParameterVoid<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static consteval int TemplateTwoStaticConstevalWithoutDefinitionOneParameterInt(int First);
    template <> consteval int TemplateTwoStaticConstevalWithoutDefinitionOneParameterInt<short, int>(int First);

    template <typename FirstType, typename SecondType>
    static consteval auto TemplateTwoStaticConstevalWithoutDefinitionTwoParametersAuto(int First, long Second);
    template <>
    consteval auto TemplateTwoStaticConstevalWithoutDefinitionTwoParametersAuto<short, int>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static consteval void TemplateTwoStaticConstevalWithoutDefinitionTwoParametersVoid(int First, long Second);
    template <>
    consteval void TemplateTwoStaticConstevalWithoutDefinitionTwoParametersVoid<short, int>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static consteval int TemplateTwoStaticConstevalWithoutDefinitionTwoParametersInt(int First, long Second);
    template <>
    consteval int TemplateTwoStaticConstevalWithoutDefinitionTwoParametersInt<short, int>(int First, long Second);

    // Two template parameter(s), Static storage,
    // Consteval evaluation, WithDefinition
    template <typename FirstType, typename SecondType>
    static consteval auto TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto() {
        return 0;
    }
    static_assert(TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto<char, short>() == 0);
    template <> consteval auto TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto<short, int>() { return 0; }
    template auto TemplateTwoStaticConstevalWithDefinitionZeroParametersAuto<long, long long>();

    template <typename FirstType, typename SecondType>
    static consteval void TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid() {}
    static_assert((TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid<char, short>(), true));
    template <> consteval void TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid<short, int>() {}
    template void TemplateTwoStaticConstevalWithDefinitionZeroParametersVoid<long, long long>();

    template <typename FirstType, typename SecondType>
    static consteval int TemplateTwoStaticConstevalWithDefinitionZeroParametersInt() {
        return 0;
    }
    static_assert(TemplateTwoStaticConstevalWithDefinitionZeroParametersInt<char, short>() == 0);
    template <> consteval int TemplateTwoStaticConstevalWithDefinitionZeroParametersInt<short, int>() { return 0; }
    template int TemplateTwoStaticConstevalWithDefinitionZeroParametersInt<long, long long>();

    template <typename FirstType, typename SecondType>
    static consteval auto TemplateTwoStaticConstevalWithDefinitionOneParameterAuto(int First) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstevalWithDefinitionOneParameterAuto<char, short>(0) == 0);
    template <> consteval auto TemplateTwoStaticConstevalWithDefinitionOneParameterAuto<short, int>(int First) {
        return 0;
    }
    template auto TemplateTwoStaticConstevalWithDefinitionOneParameterAuto<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static consteval void TemplateTwoStaticConstevalWithDefinitionOneParameterVoid(int First) {}
    static_assert((TemplateTwoStaticConstevalWithDefinitionOneParameterVoid<char, short>(0), true));
    template <> consteval void TemplateTwoStaticConstevalWithDefinitionOneParameterVoid<short, int>(int First) {}
    template void TemplateTwoStaticConstevalWithDefinitionOneParameterVoid<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static consteval int TemplateTwoStaticConstevalWithDefinitionOneParameterInt(int First) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstevalWithDefinitionOneParameterInt<char, short>(0) == 0);
    template <> consteval int TemplateTwoStaticConstevalWithDefinitionOneParameterInt<short, int>(int First) {
        return 0;
    }
    template int TemplateTwoStaticConstevalWithDefinitionOneParameterInt<long, long long>(int First);

    template <typename FirstType, typename SecondType>
    static consteval auto TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto<char, short>(0, 0) == 0);
    template <>
    consteval auto TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto<short, int>(int First, long Second) {
        return 0;
    }
    template auto TemplateTwoStaticConstevalWithDefinitionTwoParametersAuto<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static consteval void TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid(int First, long Second) {}
    static_assert((TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid<char, short>(0, 0), true));
    template <>
    consteval void TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid<short, int>(int First, long Second) {}
    template void TemplateTwoStaticConstevalWithDefinitionTwoParametersVoid<long, long long>(int First, long Second);

    template <typename FirstType, typename SecondType>
    static consteval int TemplateTwoStaticConstevalWithDefinitionTwoParametersInt(int First, long Second) {
        return 0;
    }
    static_assert(TemplateTwoStaticConstevalWithDefinitionTwoParametersInt<char, short>(0, 0) == 0);
    template <>
    consteval int TemplateTwoStaticConstevalWithDefinitionTwoParametersInt<short, int>(int First, long Second) {
        return 0;
    }
    template int TemplateTwoStaticConstevalWithDefinitionTwoParametersInt<long, long long>(int First, long Second);

    const Beta* TypeInfoDeclaredFunction(const Beta *const (&Value)[2]);

    template<typename ValueType>
    ValueType* TypeInfoDependentFunction(ValueType* Value);

    Beta (*TypeInfoParenArrayReturnFunction())[2];

    // Explicit declarations above serialize to 1407 TLFreeFunction records.
    // The 54 inline variables only force ordinary implicit instantiation.
} // namespace UEMeta::Testing::Types
