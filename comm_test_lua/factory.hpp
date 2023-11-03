#pragma once
#include <comm/ref.h>
#include <comm/intergen/ifc.h>
#include <comm/dynarray.h>

/*ifc{ 
#include "parent_class_ifc.h"
#include "child_class_ifc.h"
}ifc*/


namespace a
{

namespace b 
{
class parent_class;
class parent_class_ifc;

namespace c 
{
class child_class;
class child_class_ifc;
}

}

class factory : public policy_intrusive_base
{
public: // methods only
    factory() = default;
    factory(const factory&) = delete;
    factory& operator=(const factory&) = delete;
    static factory& get_instance();
    static bool initialize();
    static void shutdown();
    void reset();

    iref<a::b::parent_class> create_parent_item(int v0, int v1, int v2);
    iref<a::b::c::child_class> create_child_item(int v0, int v1, int v2);

    uint get_items_count() const;
    iref<a::b::parent_class> get_item(uint index) const;
    iref<a::b::parent_class> get_last_item() const;

    ifc_class(a::factory_ifc, "ifc");
    ifc_fnx(get_instance) static iref<factory> get_instance_ifc();

    ifc_fnx(create_parent_item) iref<a::b::parent_class_ifc> create_parent_item_ifc(int value);
    ifc_fnx(create_child_item) iref<a::b::c::child_class_ifc> create_child_item_ifc(int value);

protected: // methods only
protected: // members only
    static inline iref<factory> _instance;
    coid::dynarray32<iref<a::b::parent_class>> _items;
};
}; // end of namespace a
