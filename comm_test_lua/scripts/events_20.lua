implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_int_out2(out_arg1,out_arg2)
    local result = {};
    result.out_arg1 = 9;
    result.out_arg2 = "This sould be number!";
    result._ret = 9;
    return result;
end;