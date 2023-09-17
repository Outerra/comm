implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_bool_out1(out_arg1)
    local result = {};
    result.bad_out_arg1 = false;
    result._ret = false;
    return result;
end;