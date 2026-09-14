#include "factory.hpp"
#include "item.hpp"
#include "ifc/item_interface.h"
#include "ifc/js/factory_interface.h"

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
    static const coid::charstr script = R"(
var ITEM = {
    return_something : function() {
        return 5;
    }
};

//event of factory class implemented in js
function do_something()
{
    var it = this.create_item();
    it.$rebind_events(ITEM);

    it.enumerate(6, function(k, name) {
        $log(name + ' ');
        this.print(k + 1);
    });

    return 0;
}
)";

    v8::HandleScope scope(v8::Isolate::GetCurrent());

    v8::Handle<v8::Context> _ctx;
    ::js::script_handle sh(script,false);
    iref<factory_interface> ifc = ::js::factory_interface::get(sh);
    ifc->force_bind_script_events();

    try{
        int result = do_something();
    }
    catch (std::exception& e) {
        coidlog_error_src("", e.what());
    }
}

int factory::run()
{
    return _items[0]->return_something();
}
