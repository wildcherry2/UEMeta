#pragma once

namespace UEMeta::Testing::Types {
    struct BeforeInlinedDependencyType {};
}

#include "InlinedDependencyTypes.hpp"

namespace UEMeta::Testing::Types {
    struct AfterInlinedDependencyType {};
}
