enum class Test {
    ALPHA,
    BETA = 2
};

namespace ns {
    enum class Test {
        ALPHA,
        BETA = 2
    };

    typedef enum {
        BRAVO,
        CHARLIE
    } Anon;

    enum {
        DELTA,
        ECHO = 9
    } AnotherAnon;
}