#pragma once
#include <comm/ref.h>
#include <comm/intergen/ifc.h>
#include <comm/dynarray.h>

class item : public policy_intrusive_base
{
public:
    ifc_class_var(item_interface, "ifc", _client);
    ifc_fn static iref<item> _get(void* ptr);
    ifc_event int return_something() ifc_default_body(return 0;);
};

