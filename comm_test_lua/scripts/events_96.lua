implements("a.b.parent_class_ifc");

function a.b.parent_class_ifc:some_event_int_arr_out2()
    local arr = {};
	arr[1] = 0;
	arr[2] = 1;
	arr[3] = 2;
	
	local result = {}
	result.out_arg1 = arr;
	result.out_arg2 = arr;
	result._ret = arr;
	
	return arr;
end;