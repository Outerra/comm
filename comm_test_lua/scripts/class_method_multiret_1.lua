implements_as("a.b.parent_class_ifc", "parent_class_ifc");

function parent_class_ifc:some_event_int()
    local ret = self:return_some_value_parent_multiret_2() 
    return ret._ret + ret.out_arg1 + ret.out_arg2;
end;