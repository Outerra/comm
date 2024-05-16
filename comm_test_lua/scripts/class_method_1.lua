implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_int()
    return self:some_new_method();
end;

function a.b.parent_class_ifc:some_new_method()
    return 9;
end;