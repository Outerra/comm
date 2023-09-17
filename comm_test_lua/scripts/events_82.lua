implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_string_out2(out_arg1,out_arg2)
    local result = {};
    result.bad_out_arg1 = "some string";
    result.out_arg2 = "some string";
    result._ret = "some string";
    return result;
end;