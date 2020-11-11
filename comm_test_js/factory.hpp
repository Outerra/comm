#pragma once
#include <comm/ref.h>
#include <comm/intergen/ifc.h>
#include <comm/dynarray.h>

/*ifc{
#include "item_interface.h"
}ifc*/

class item_interface;

class item;

class factory : public policy_intrusive_base
{
public:
    ifc_class_var(factory_interface, "ifc", _client);
    ifc_fn static iref<factory> get();
    ifc_fn iref<item_interface> create_item();
    ifc_event int do_something();
    void initialize();
    int run();


    coid::dynarray<iref<item>> _items;
};

