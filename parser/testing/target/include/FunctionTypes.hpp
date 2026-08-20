#pragma once

#include "RecordTypes.hpp"

namespace UEMeta::Testing::Types {
    inline int FreeFunction(const DerivedRecord& record) {
        return record.vfunc2();
    }

    inline constexpr int GlobalConstant = 42;
}
