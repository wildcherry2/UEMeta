#include "AliasTypes.hpp"
#include "EnumTypes.hpp"
#include "ForwardDeclarationTypes.hpp"
#include "FunctionTypes.hpp"
#include "GlobalVariableTypes.cpp"
#include "RecordTypes.hpp"

namespace UEMeta::Testing::Types {
    int TouchAll() {
        DerivedRecord record{};
        record.vfunc(1, true);
        record.sfunc('x');
        return FreeFunction(record) + GlobalConstant;
    }
}
