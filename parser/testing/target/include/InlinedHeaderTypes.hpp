#pragma once

namespace UEMeta::Testing::Types {
    struct BeforeInlinedDependencyType {};
} // namespace UEMeta::Testing::Types

#include "InlinedDependencyTypes.hpp"

namespace UEMeta::Testing::Types {
    struct AfterInlinedDependencyType {};
} // namespace UEMeta::Testing::Types
