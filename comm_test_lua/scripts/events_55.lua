implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_bool_out1(out_arg1)
    local result = {};
    result.out_arg1 = "This sould be bool!";
    result._ret = false;
    return result;
end;