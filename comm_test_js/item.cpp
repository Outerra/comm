#include "item.hpp"

iref<item> item::_get(void* ptr)
{
    return iref<item>(static_cast<item*>(ptr));
}

item* item::_create_dummy()
{
    return new item;
}
