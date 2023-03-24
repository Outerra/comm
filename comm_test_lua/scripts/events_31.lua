implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_float_retvoid_out2(out_arg1, out_arg2)
    local result = {};
    result.bad_outarg_name = 9.0;
    result.out_arg2 = 9.0;
    return result;
end;