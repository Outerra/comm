implements_as("a.b.parent_class_ifc", "parent_class_ifc");

function parent_class_ifc:some_event_int()
    return self:accumulate_values(5, function(acc, v)
        return acc + v * v;
    end);
end