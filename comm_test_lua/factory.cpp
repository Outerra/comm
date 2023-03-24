#include "factory.hpp"
#include "parent.hpp"
#include "child.hpp"

#include "ifc/parent_class_ifc.h"
#include "ifc/child_class_ifc.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

a::factory& a::factory::get_instance()
{
    DASSERT(_instance);
    return *_instance;
}


bool a::factory::initialize()
{
    DASSERT_RET(_instance.is_empty(), false);
    _instance.create(new factory());

    return true;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void a::factory::shutdown()
{
    if (!_instance.is_empty())
    {
        DASSERT(_instance.refcount() == 1);
        _instance.release();
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void a::factory::reset()
{
    _items.reset();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::factory::get_item(uint index) const
{
    return _items[index];
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::factory::get_last_item() const
{
    iref<a::b::parent_class>* result = _items.last();
    return result ? *result : iref<a::b::parent_class>();
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

ifc_fnx(get_instance)iref<a::factory> a::factory::get_instance_ifc()
{
    return _instance;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class> a::factory::create_parent_item(int value)
{
    iref<a::b::parent_class>* item_iref_ptr = _items.push(new b::parent_class(value));
    return *item_iref_ptr;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::c::child_class> a::factory::create_child_item(int value)
{
    iref<a::b::c::child_class> result(new a::b::c::child_class(value));
    _items.push(result);
    return result;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

uint a::factory::get_items_count() const
{
    return _items.size();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::parent_class_ifc> a::factory::create_parent_item_ifc(int value)
{
    iref<a::b::parent_class> item_iref_ptr = create_parent_item(value);
    return a::b::parent_class_ifc::_get(item_iref_ptr.get());
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

iref<a::b::c::child_class_ifc> a::factory::create_child_item_ifc(int value)
{
    iref<a::b::c::child_class> item_iref_ptr = create_child_item(value);
    return a::b::c::child_class_ifc::_get(item_iref_ptr.get());

}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
