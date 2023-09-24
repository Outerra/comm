
#pragma once

#include <comm/intergen/ifc.h>
#include "data.hpp"

using namespace std;

/*ifc{
// ***
// block placed before the generated client classes
// ***
}ifc*/

//ifc{ xt::client2
// ***
// block placed before the generated client class, specific interface
// ***
//}ifc

/*ifc{+
// ***
// block placed after the generated client classes
// ***
}ifc*/


/*ifc{ client
struct flags {
    int a;
    float b;

    friend coid::metastream& operator || (coid::metastream& m, flags& d) {
        return m.compound_type(d, [&]() {
            m.member("a", d.a);
            m.member("b", d.b);
        });
    }
};
}ifc*/

//ifc{ xt::client2
struct component;
//}ifc

namespace ab {

/*ifc{
// ***
// block placed inside namespace
// ***
}ifc*/

namespace cd {

/*ifc{ client+
// ***
// block placed inside namespace after client class, specific interface
// ***
}ifc*/




class host : public policy_intrusive_base
{
    coid::charstr _tmp;
    coid::versionid _eid;

public:

    ifc_class_var(client, "ifc/", _ifc);

    /// @brief interface creator
    /// @return host class instance
    ifc_fn static iref<ab::cd::host> creator() {
        return new host;
    }

    ifc_fn void set_def(const flags& flg = {.a = 1, .b = 2})
    {
    }

    ///Setter
    ifc_fn void set(const coid::token& par)
    {
        _tmp = par;
    }

    ///Getter
    ifc_fn int get(ifc_out coid::charstr& par)
    {
        par = _tmp;
        return 0;
    }

    //ifc{
    typedef int sometype;
    //}ifc

    ///Using a custom type
    ifc_fn sometype custom() {
        return 0;
    }

    ifc_fnx(!) const int* c_only_method(int k) {
        return 0;
    }

    ifc_fnx(!) void set_array(const float ar[3]) {}

    ifc_fn virtual bool overridable() { return false; }

    ifc_fnx(!) void callback(void (*cbk)(int, const coid::token&)) {
        cbk(1, "blah"_T);
    }

    ifc_event void echo(int k);

    ifc_eventx(!) void callforth(void (*cbk)(int, const coid::token&));

    ifc_eventx(=0) int required(int x);

    ifc_fnx(!) void memfn_callback(coid::callback<void(int, void*)>&& fn) {
        //invoke passed callback (intergen interface member function)
        //(_ifc->iface<intergen_interface>()->*(fn))(1, nullptr);

        fn(_ifc->iface<intergen_interface>(), 1, nullptr);
    }

    ifc_fnx(@unload) static bool unload(const coid::token& client, const coid::token& module, coid::binstring* bstr) {
        return true;
    }

    ////

    ifc_class(xt::client2, "ifc/");

    ifc_fn void test() {}

    /// @return pointer to a data interface type (ifc_struct)
    ifc_fnx(ifc_struct=component_ifc) component* get();
};

} // namespace ab
} // namespace cd
