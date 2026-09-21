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

    //ifc{
    enum class stringo
    {
        jozo,
        fero,

        _count
    };
    //}ifc

    ifc_fn stringo stringified_enum_value() { return stringo::fero; }
};

inline coid::metastream& operator||(coid::metastream& m, item::stringo& v)
{
    using stringo = item::stringo;
    static stringo values[] = {stringo::jozo, stringo::fero};
    static const char* names[] = {"jozo", "fero", nullptr};
    static_assert(sizeof(names) / sizeof(const char*) - 1 == sizeof(values) / sizeof(stringo));  // launch_result_values and launch_result_names not synced
    static_assert(sizeof(values) / sizeof(stringo) == static_cast<uint32>(stringo::_count)); // some of launch_result_enum values missing in launch_result_values array

    return m.enum_class_type(v, values, names, stringo::jozo);
}

