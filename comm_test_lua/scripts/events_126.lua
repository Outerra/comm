implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_compound_out2()
    local result = {};
    local compound_val = {};
   
    compound_val.integer_value = 9;
    compound_val.float_value = 9.9;
    compound_val.bool_value = true;
    compound_val.string_value = "some string";
    compound_val.integer_array_value = {1, 2, 3};
    
    compound_val.other_compound_value = {};
    compound_val.other_compound_value.integer_value = 9;
    compound_val.other_compound_value.float_value = 9.9;
    compound_val.other_compound_value.bool_value = true;
    compound_val.other_compound_value.string_value = "some string";
    compound_val.other_compound_value.integer_array_value = {1, 2, 3};
    
    result.out_arg1 = compound_val;
    result.out_arg2 = compound_val;
    result._bad_ret = compound_val;

    return result;
end;