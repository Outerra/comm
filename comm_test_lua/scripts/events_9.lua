implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_int_retvoid_out2(out_arg1, out_arg2)
    local result = {};
    result.out_arg1 = 9;
    result.out_arg2 = "This sould be number";
    return result;
end;