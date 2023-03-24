implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_float_out1(out_arg1)
    local result = {};
    result.out_arg1 = "This sould be number!";
    result._ret = 9.0;
    return result;
end;