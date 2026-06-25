

namespace ns {
    enum class B {
        enumerator,
        second,
        third = 9
    };

    class A {
        using enum B;
    };


}