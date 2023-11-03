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

    ifc_fn static data* create()
    {
        coid::versionid id = entman::allocate();
        return entman::push_default<a::b::c::d::data>(id);
    }

    ifc_fn static data* create_w_params(int v0, int v1, int v2) 
    {
        coid::versionid id = entman::allocate();
        return entman::emplace<a::b::c::d::data>(id, v0, v1, v2);
    }

    ifc_fn void set_int(int i) 
    {
        _v0 = i;
    }

    ifc_fn int get_int() const
    {
        return _v0;
    }

    ifc_fn void get_int_multiret_0(ifc_out int& out_arg0) const
    {
        out_arg0 = _v0;
    }

    ifc_fn int get_int_multiret_1(ifc_out int& out_arg1) const
    {
        out_arg1 = _v1;
        return _v0;
    }
    
    ifc_fn int get_int_multiret_2(ifc_out int& out_arg1, ifc_out int& out_arg2) const
    {
        out_arg1 = _v1;
        out_arg2 = _v2;

        return _v0;
    }

    data(int v0, int v1, int v2)
        : _v0(v0)
        , _v1(v1)
        , _v2(v2)
    {};

    data() = default;

    int _v0 = 0;
    int _v1 = 0;
    int _v2 = 0;
};
}; // end of namespace d 
}; // end of namespace c 
}; // end of namespace b 
}; // end of namespace a