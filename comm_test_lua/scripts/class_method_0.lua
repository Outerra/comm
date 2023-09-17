implements_as("a.b.parent_class_ifc", "parent_class_ifc");

function parent_class_ifc:some_event_int()
    return self:some_new_method();
end;

function parent_class_ifc:some_new_method()
    return 9;
end;