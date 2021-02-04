#include "item.hpp"

iref<item> item::_get(void* ptr)
{
    return static_cast<item*>(ptr);
}
