#pragma once
#include <comm/ref.h>
#include <comm/intergen/ifc.h>
#include <comm/dynarray.h>
#include <comm/binstream/stdstream.h>

class item : public policy_intrusive_base
{
public:
    ifc_class_var(item_interface, "ifc", _client);
    ifc_fn static iref<item> _get(void* ptr);

    ifc_event int return_something() ifc_default_body(return 0;);

    ifc_fn int enumerate(int a, const coid::callback<void(int, const coid::token&)>& fn)
    {
        for (int k = 0; k < 8; ++k)
            fn(a + k, "jozo");
        return 0;
    }

    ifc_fnx(!) void memfn_callback(coid::callback<void(int, void*)>&& fn) {
        fn.invoke_with_this(_client->iface<intergen_interface>(), 1, nullptr);
    }

    ifc_fn void print(int k)
    {
        static coid::stdoutstream out;
        out << k << '\n';
    }

    //ifc{
    enum class enumo {
        zero,           //< blah
        one,            //< ooo
    };
    //}ifc

    ifc_fn void enum_param(enumo o) {}

};

