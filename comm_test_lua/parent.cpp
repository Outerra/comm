#include "parent.hpp"
#include "data.hpp"

#include <comm/global.h>
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::b::c::d::data* a::b::parent_class::get_data()
{
    return a::b::c::d::data::create();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::b::c::d::data* a::b::parent_class::get_data_w_params(int v0, int v1, int v2)
{
    return a::b::c::d::data::create_w_params(v0, v1, v2);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::b::parent_class::get_default()
{
    _instance.create(new parent_class(0,1,2));
    return _instance;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::b::parent_class::get_value(int value)
{
    _instance.create(new parent_class(value, 0, 0));
    return _instance;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::b::parent_class::_get(void* ptr)
{
    return static_cast<parent_class*>(ptr);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

int a::b::parent_class::return_some_value_parent()
{
    return _some_value;
}

ifc_fn void a::b::parent_class::return_some_value_parent_multiret_0(ifc_out int& out_arg0)
{
    out_arg0 = _some_value;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

int a::b::parent_class::return_some_value_parent_multiret_1(int& out_arg1)
{
    out_arg1 = _some_ohter_value;
    return _some_value;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

int a::b::parent_class::return_some_value_parent_multiret_2(int& out_arg1, int& out_arg2)
{
    out_arg1 = _some_ohter_value;
    out_arg2 = _some_another_value;
    return _some_value;

}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::b::parent_class::parent_class(int some_value, int some_ohter_value, int some_another_value)
    : _some_value(some_value)
    , _some_ohter_value(some_ohter_value)
    , _some_another_value(some_another_value)
{}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::b::parent_class::get_instance()
{
    return _instance;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
