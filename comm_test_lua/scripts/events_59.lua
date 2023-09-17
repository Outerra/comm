implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_bool_out2(out_arg1,out_arg2)
    local result = {};
    result.out_arg1 = "This sould be bool!";
    result.out_arg2 = false;
    result._ret = false;
    return result;
end;