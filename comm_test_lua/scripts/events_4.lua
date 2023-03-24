implements("a.b.parent_class_ifc");

function some_defined_function()
    return 9;
end;

function a.b.parent_class_ifc:some_event_int()
    return some_defined_function();
end;