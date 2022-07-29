
#pragma once

#include <comm/intergen/ifc.h>

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

public:

    ifc_class_var(client, "ifc/", _ifc);

    /// @brief interface creator
    /// @return host class instance
    ifc_fn static iref<ab::cd::host> creator() {
        return new host;
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

    ifc_fn void set_array(const float ar[3]) {
    }

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


    ifc_class(xt::client2, "ifc/");

    ifc_fn void test() {}
};


struct data
{
    ifc_struct(data_ifc, "ifc/");

    ifc_fn static data* creator() {
        return new data;
    }

    ifc_fn void set_a(int b) {
        _b = b;
    }

    ifc_fn void set_b(const coid::token& a, ifc_out int* b) {
        _a = a;
        *b = _b;
    }

    int _b;
    coid::charstr _a;
};


} // namespace ab
} // namespace cd
