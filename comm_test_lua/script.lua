implements("factory_interface");
implements_as("item_interface", "item_ifc")

--namespace a
--{
--    namespace b
--    {
--        class parent_class_ifc;
--    }
--}

-- implements puzit bodky miesto stvorbodiek
-- error handling catchovat exception este v imlements a implements as (vo vsetkych lua injectoch) a vygenerovat to ako error na lua strane

implements("a.b.parent_clas_ifc")

function factory_interface:do_something()
    local item = self:create_item();
    item:rebind_events(item_ifc);
    return 10;
end;

function item_ifc:return_something()
    return 5;
end;

function a.b.parent_class_ifc:return_something()
    return self.return_some_value();
end;