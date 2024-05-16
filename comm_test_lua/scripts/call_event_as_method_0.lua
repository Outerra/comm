implements_as("a.b.parent_class_ifc", "my_class");

function my_class:some_event_int()
	return 10;
end;

function my_class:some_event_int_default()
    return self:some_event_int();
end;
