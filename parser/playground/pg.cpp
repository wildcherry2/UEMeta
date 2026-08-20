namespace st {
    class Beta {};
}

using Alpha = st::Beta;

class C {
public:
    void func() {}
    virtual void vfunc(int a, bool b) {}
    int field = 0;
    Alpha bfield{};
    virtual ~C() = default;
};

struct A : public C {
    void vfunc(int a, bool b) override {}
    virtual int vfunc2() { return 4; }
    int nfield = 1;
    void sfunc(char c) {}
    ~A() override = default;
};

