implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_int()
    local data_ifc = query_interface("lua.a.b.c.data_ifc.create");
    data_ifc:set_int(9);
    return data_ifc:get_int();
end;