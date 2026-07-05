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
}
