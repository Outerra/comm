#pragma once

#include <comm/intergen/ifc.h>
#include <comm/global.h>

namespace a
{
namespace b
{
namespace c
{
namespace d
{
struct data
{
    ifc_struct(a::b::c::d::data_ifc, "ifc");

    ifc_fn static data* create() {
        coid::versionid id = entman::allocate();
        return entman::push_default<a::b::c::d::data>(id);
    }

    ifc_fn void set_int(int i) {
        _int = i;
    }

    ifc_fn int get_int() const
    {
        return _int;
    }

    int _int = 1488;
};
}; // end of namespace d 
}; // end of namespace c 
}; // end of namespace b 
}; // end of namespace a