#include "parent.hpp"
#include "data.hpp"

#include <comm/global.h>
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::b::c::d::data* a::b::parent_class::get_data()
{
    return a::b::c::d::data::create();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::b::parent_class::get_default()
{
    _instance.create(new parent_class(0));
    return _instance;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::b::parent_class::get_value(int value)
{
    _instance.create(new parent_class(value));
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

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::b::parent_class::parent_class(int some_value)
    : _some_value(some_value)
{}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::b::parent_class::get_instance()
{
    return _instance;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
