#pragma once

namespace UEMeta::Testing::Types {
    class Beta {};

    using Alpha = Beta;

    template<typename AliasType>
    using TemplatedAlpha = AliasType;

    typedef Beta LegacyAlpha;

    template<typename ValueType>
    class AliasTemplate {};

    using WrappedAlpha = const Beta *const[2];
    using ReferenceAlpha = const Beta&;

    template<typename AliasType>
    using TemplatedPointerAlpha = const AliasType*;

    template<typename OwnerType>
    using DependentMemberAlpha = typename OwnerType::type;

    template<typename AliasType = const Beta*>
    using DefaultedTemplatedAlpha = AliasType;

    template<typename OwnerType, typename AliasType = typename OwnerType::type>
    using DependentDefaultAlpha = AliasType;

    template<int Size = 2>
    using SizedAlpha = Beta[Size];

    template<template<typename TemplateValue> typename TemplateType = AliasTemplate>
    using TemplateDefaultedAlpha = TemplateType<Beta>;

    using SpecializedAlpha = AliasTemplate<int>;
}
