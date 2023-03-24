implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_bool_retvoid_out2(out_arg1, out_arg2)
    local result = {};
    result.out_arg1 = false;
    result.bad_out_arg2 = false;
    return result;
end;