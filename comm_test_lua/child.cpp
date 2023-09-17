#include "child.hpp"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::b::c::child_class::child_class(int value)
    :parent_class(value)
{

}

ifc_fn iref<a::b::c::child_class > a::b::c::child_class::_get(void* ptr)
{
    return static_cast<child_class*>(ptr);
}
