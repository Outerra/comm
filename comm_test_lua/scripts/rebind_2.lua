implements_as("a.b.parent_class_ifc", "factory");
implements_as("a.b.parent_class_ifc", "item");


local items = {};

function item:some_event_int_wrong()
    return 11;
end;

function factory:some_event_default()
    local f = query_interface("lua.a.factory_ifc.get_instance");    
    local new_item = f:create_parent_item(5);
    new_item:rebind_events(item);
    table.insert(items, new_item);
end;