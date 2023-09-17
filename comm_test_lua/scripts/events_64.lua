implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_bool_out2(out_arg1,out_arg2)
    local result = {};
    result.out_arg1 = false;
    result.out_arg2 = false;
    result.bad_ret = false;
    return result;
end;