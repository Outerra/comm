implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_compound()
    local result = {};
    result.integer_value = 9;
    result.float_value = 9.9;
    result.bool_value = true;
    result.string_value = "some string";
    
    return result;
end;