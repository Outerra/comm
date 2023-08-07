
#ifndef __INTERGEN_TEST__HPP__
#define __INTERGEN_TEST__HPP__

#include "../ifc.h"
#include "../../str.h"

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

class empty_thing : public policy_intrusive_base
{
public:

    ifc_class(ifc1::ifc2::emptyface, "ifc");

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

    ifc_class_var(ifc1::ifc2::thingface, "ifc", _ifc);

    ifc_fnx(get) static iref<n1::n2::thing> get_thing() {
        return new thing;
    }

    /*ifc_fn*/  void createScenario(const coid::charstr &name);

    ifc_fnx(~) void destroy()
    {}

    /// @brief some method " test escaping
    /// @param a some argument " test escaping
    ifc_fn int hallo( int a, const coid::token& b, ifc_out coid::charstr& c ) {
        return 0;
    }

    ifc_fn void noargs() {}

    ifc_fn coid::charstr fallo( bool b, const char* str ) { return str; }

    ifc_fn double operator()(const char* key) const;
    ifc_fn void operator()(const char* key, double value);

    ifc_event void boo( const char* key );

    ifc_event const char* body() ifc_default_body("return \"string\";");

    ifc_event coid::charstr strbody() override ifc_default_body("return \"value\";");


    //extension interface in the same class

    ifc_class_extend(ns::ifc_ext : ifc1::ifc2::thingface, "ifc");

    ifc_fn void dummy();
};


class inherit_external : public policy_intrusive_base
{
public:

    //extension interface in a different class
    ifc_class(ns::ifc_ext_ext : ifc1::ifc2::thingface, "ifc", "");

    ifc_fn void dummy();
};


}
}

#endif //__INTERGEN_TEST__HPP__
