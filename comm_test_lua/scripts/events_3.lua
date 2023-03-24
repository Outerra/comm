implements("a.b.parent_class_ifc");

function some_defined_function()
    return 10;
end;

function a.b.parent_class_ifc:some_event_int()
    return some_undefined_function();
end;