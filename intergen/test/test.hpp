
#ifndef __INTERGEN_TEST__HPP__
#define __INTERGEN_TEST__HPP__

#include "../ifc.h"
#include "../../str.h"

//ifc{
struct test;

namespace bt {
    struct base;
}
//}ifc

namespace xx {
    class yy;
}

struct test {
    int k;
    struct {
        int dummy;
    };
};

namespace n1 {
namespace n2 {

class zz;

class virtual_thing : public policy_intrusive_base
{
public:

    IFC_CLASS_VIRTUAL(basei, "ifc");

    ifc_fn void xooo();
};


class empty_thing : public policy_intrusive_base
{
public:

    IFC_CLASS(ifc1::ifc2::emptyface, "ifc");

    ifc_fnx(get) static iref<n1::n2::empty_thing> _get_thing() {
        return new empty_thing;
    }

    ifc_fnx(get2) static iref<n1::n2::empty_thing> _get2(void* p) {
        return static_cast<empty_thing*>(p);
    }
};



class base_thing : public policy_intrusive_base
{
protected:
    virtual coid::charstr strbody() { return "base"; }
};



class thing : public base_thing
{
public:

    IFC_CLASS_VAR(ifc1::ifc2::thingface : virtual basei, "ifc", _ifc);

    ifc_fnx(get) static iref<n1::n2::thing> get_thing() {
        return new thing;
    }

    /*ifc_fn*/  void createScenario(const coid::charstr& name);

    ifc_fnx(~) void destroy()
    {}

    /// @brief some method " test escaping
    /// @param a some argument " test escaping
    ifc_fn int hallo(int a, const coid::token& b, ifc_out coid::charstr& c) {
        return 0;
    }

    ifc_fn void noargs() {}

    ifc_fnx(!) ref<thing> noscript();

    ifc_fn coid::charstr fallo(bool b, const char* str) { return str; }

    ifc_fn void loo(bool a, int b);

    ifc_fn double operator()(const char* key) const;
    ifc_fn void operator()(const char* key, double value);

    ifc_fn void inout(ifc_inout test*& par);

    ifc_fn struct test* dataret();

    ifc_fnx(!) void nested(const coid::dynarray<bt::base>& stuff);

    ifc_event void boo(const char* key, int some);

    ifc_event const char* body() ifc_default_body("return \"string\";");

    ifc_event coid::charstr strbody() override ifc_default_body("return \"value\";");


    //extension interface in the same class

    IFC_CLASS(ns::ifc_int : ifc1::ifc2::thingface, "ifc");

    ifc_fn void dummy();
};

/// @brief
class inherit_external : public thing
{
public:

    //extension interface in a different class
    IFC_CLASS(ns::ifc_ext : ifc1::ifc2::thingface, "ifc", "");

    ifc_fn void mummy();

    //override parent method
    ifc_fn void loo(bool a, bool b);



    ////

    //extension of extension
    IFC_CLASS(ns::ifc_ext2 : ns::ifc_ext, "ifc");

    ifc_fn void some1();

    //override parent method
    ifc_fnx(loo) void loo2(bool a, bool b);

    ifc_fn void some2();

    //override base ifc method, change to event
    ifc_event bool noo();
};

}
}

#endif //__INTERGEN_TEST__HPP__
