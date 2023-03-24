implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_string_out1(out_arg1)
    local result = {};
    result.out_arg1 = "some string";
    result.bad_ret = "some string";
    return result;
end;