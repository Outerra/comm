implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_int_out1(out_arg1)
    local result = {};
    result.out_arg1 = "This sould be number!";
    result._ret = 9;
    return result;
end;