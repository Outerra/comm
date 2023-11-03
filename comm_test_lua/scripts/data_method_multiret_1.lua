implements_as("a.b.parent_class_ifc", "parent_class_ifc");

function parent_class_ifc:some_event_int()
    local data = self:get_data_w_params(35, 7, 11)
    local ret = data:get_int_multiret_2() 
    return ret._ret + ret.out_arg1 + ret.out_arg2;
end;