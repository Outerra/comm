implements_as("a.b.parent_class_ifc", "my_class");

my_class.new_member_variable = 11

function my_class:some_event_int()
    return self.new_member_variable
end;