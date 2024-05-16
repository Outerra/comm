implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_int()
	return 10;
end;

function a.b.parent_class_ifc:some_event_int_default()
    return self:some_event_int();
end;
