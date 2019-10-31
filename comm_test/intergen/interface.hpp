
#pragma once

#include <comm/intergen/ifc.h>

using namespace std;

namespace ab {
namespace cd {

class host : public policy_intrusive_base
{
    coid::charstr _tmp;

public:

    ifc_class_var(client, "ifc/", _ifc);

    ifc_fn static iref<ab::cd::host> creator() {
        return new host;
    }

    ifc_fn void set(const coid::token& par)
    {
        _tmp = par;
    }

    ifc_fn int get(ifc_out coid::charstr& par)
    {
        par = _tmp;
        return 0;
    }

    ifc_fn void callback(void (*cbk)(int, const coid::token&)) {
        cbk(1, "blah"_T);
    }

    ifc_event void echo(int k);

    ifc_event void callforth(void (*cbk)(int, const coid::token&));


    ifc_fn void memfn_callback(coid::callback<void(int, void*)>&& fn) {
        //invoke passed callback (intergen interface member function)
        //(_ifc->iface<intergen_interface>()->*(fn))(1, nullptr);

        fn(_ifc->iface<intergen_interface>(), 1, nullptr);
    }
};

} // namespace ab
} // namespace cd
