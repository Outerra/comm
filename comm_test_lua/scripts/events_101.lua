implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_compound()
    local result = {};
    result.integer_value = 9;
    result.float_value = 9.9;
    result.string_value = "some string";
    result.integer_array_value = {1,2,3};
    
    return result;
end;