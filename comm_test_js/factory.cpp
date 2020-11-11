#include "factory.hpp"
#include "item.hpp"
#include "ifc/item_interface.h"
#include "ifc/factory_interface.js.h"

iref<factory> factory::get()
{
    static iref<factory> instance = new factory;
    return instance;
}

iref<item_interface> factory::create_item()
{
    iref<item>* ptr = _items.add();
    ptr->create(new item());
    
    return item_interface::_get(ptr->get());
}

void factory::initialize()
{
    static const coid::charstr script = "var ITEM = {\n"
        "return_something() {\n"
        "return 5;\n"
        "}\n"
        "}\n"
        "\n"
        "function do_something()\n"
        "{\n"
        "var it = this.create_item();\n"
        "it.$rebind_events(ITEM);\n"
        "return 0;\n"
        "}";

    v8::HandleScope scope(v8::Isolate::GetCurrent());

    v8::Handle<v8::Context> _ctx;
    ::js::script_handle sh(script,false);
    iref<factory_interface> ifc = ::js::factory_interface::get(sh);
    ifc->force_bind_script_events();

    try{
        int result = do_something();
    }
    catch (std::exception& e) {
        // fuck
        int a = 1;
        coidlog_error("",e.what());
    }
}

int factory::run()
{
    return _items[0]->return_something();
}
