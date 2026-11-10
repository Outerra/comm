implements_as("a.b.parent_class_ifc", "parent_class_ifc");

function parent_class_ifc:some_event_int()
    local sum = 0;
    local n = self:enumerate_values(4, function(v, name)
        if name ~= "item" then
            error("unexpected name " .. name);
        end;
        -- calls back into the interface from within the callback
        sum = sum + v - self:return_some_value_parent();
    end);
    return sum * 10 + n;
end