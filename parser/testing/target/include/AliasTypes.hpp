#pragma once

namespace UEMeta::Testing::Types {
    class Beta {};

    using Alpha = Beta;

    template<typename AliasType>
    using TemplatedAlpha = AliasType;

    typedef Beta LegacyAlpha;
}
