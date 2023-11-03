#include "child.hpp"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::b::c::child_class::child_class(int some_value, int some_ohter_value, int some_another_value)
    :parent_class(some_value, some_ohter_value, some_another_value)
{

}

ifc_fn iref<a::b::c::child_class > a::b::c::child_class::_get(void* ptr)
{
    return static_cast<child_class*>(ptr);
}
