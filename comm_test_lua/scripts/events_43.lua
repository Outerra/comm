implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_float_out2(out_arg1,out_arg2)
    local result = {};
    result.out_arg1 = 9.0;
    result.bad_out_arg2 = 9.0;
    result._ret = 9.0;
    return result;
end;