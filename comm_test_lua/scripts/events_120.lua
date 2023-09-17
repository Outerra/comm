implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_compound_retvoid_out1()
    local result = {};
    result.integer_value = 9;
    result.float_value = 9.9;
    result.bool_value = true;
    result.string_value = "some string";
    result.integer_array_value = {1,2,3};
    
    result.other_compound_value = {};
    result.other_compound_value.integer_value = 9;
    result.other_compound_value.float_value = 9.9;
    result.other_compound_value.bool_value = true;
    result.other_compound_value.string_value = "some string";
    result.other_compound_value.integer_array_value = {1,2,3};
    
    return result;
end;