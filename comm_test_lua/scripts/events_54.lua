implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_bool_out1(out_arg1)
    local result = {};
    result.out_arg1 = false;
    result._ret = "This sould be bool!";
    return result;
end;