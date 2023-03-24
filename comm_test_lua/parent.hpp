#pragma once
#include <intergen/ifc.h>
#include <comm/ref.h>

//ifc{
#include "compound.h"
//}ifc

namespace a
{
namespace b
{

class parent_class : public policy_intrusive_base
{
public: // interfaces only
    ifc_class_var_inheritable(a::b::parent_class_ifc, "ifc", _client);
    ifc_fn static iref<parent_class> get_default();
    ifc_fn static iref<parent_class> get_value(int value);
    ifc_fn static iref<parent_class> _get(void* ptr);
    ifc_fn int return_some_value_parent();
    
    ifc_event void some_event_default() ifc_default_body(return;);
    ifc_event void some_event();

    ifc_event int some_event_int_default() ifc_default_body(return 0;);
    ifc_event int some_event_int();
    ifc_event void some_event_int_retvoid_out1(ifc_inout int &out_arg1);
    ifc_event void some_event_int_retvoid_out2(ifc_inout int &out_arg1, ifc_inout int &out_arg2);
    ifc_event int some_event_int_out1(ifc_inout int &out_arg1);
    ifc_event int some_event_int_out2(ifc_inout int &out_arg1, ifc_inout int &out_arg2);

    ifc_event float some_event_float_default() ifc_default_body(return 0.0f;);
    ifc_event float some_event_float();
    ifc_event void some_event_float_retvoid_out1(ifc_inout float& out_arg1);
    ifc_event void some_event_float_retvoid_out2(ifc_inout float& out_arg1, ifc_inout float& out_arg2);
    ifc_event float some_event_float_out1(ifc_inout float& out_arg1);
    ifc_event float some_event_float_out2(ifc_inout float& out_arg1, ifc_inout float& out_arg2);

    ifc_event bool some_event_bool_default() ifc_default_body(return true;);
    ifc_event bool some_event_bool();
    ifc_event void some_event_bool_retvoid_out1(ifc_inout bool& out_arg1);
    ifc_event void some_event_bool_retvoid_out2(ifc_inout bool& out_arg1, ifc_inout bool& out_arg2);
    ifc_event bool some_event_bool_out1(ifc_inout bool& out_arg1);
    ifc_event bool some_event_bool_out2(ifc_inout bool& out_arg1, ifc_inout bool& out_arg2);

    ifc_event coid::charstr some_event_string_default() ifc_default_body(return "default string";);
    ifc_event coid::charstr some_event_string();
    ifc_event void some_event_string_retvoid_out1(ifc_inout coid::charstr& out_arg1);
    ifc_event void some_event_string_retvoid_out2(ifc_inout coid::charstr& out_arg1, ifc_inout coid::charstr& out_arg2);
    ifc_event coid::charstr some_event_string_out1(ifc_inout coid::charstr& out_arg1);
    ifc_event coid::charstr some_event_string_out2(ifc_inout coid::charstr& out_arg1, ifc_inout coid::charstr& out_arg2);
    
    ifc_event coid::dynarray<int> some_event_int_arr_default() ifc_default_body(return coid::dynarray<int>(););
    ifc_event coid::dynarray<int> some_event_int_arr();
    ifc_event void some_event_int_arr_retvoid_out1(ifc_inout coid::dynarray<int>& out_arg1);
    ifc_event void some_event_int_arr_retvoid_out2(ifc_inout coid::dynarray<int>& out_arg1, ifc_inout coid::dynarray<int>& out_arg2);
    ifc_event coid::dynarray<int> some_event_int_arr_out1(ifc_inout coid::dynarray<int>& out_arg1);
    ifc_event coid::dynarray<int> some_event_int_arr_out2(ifc_inout coid::dynarray<int>& out_arg1, ifc_inout coid::dynarray<int>& out_arg2);

    ifc_event ::c::d::compound some_event_compound_default() ifc_default_body(return ::c::d::compound(););
    ifc_event ::c::d::compound some_event_compound();
    ifc_event void some_event_compound_retvoid_out1(ifc_inout ::c::d::compound&out_arg1);
    ifc_event void some_event_compound_retvoid_out2(ifc_inout ::c::d::compound&out_arg1, ifc_inout ::c::d::compound&out_arg2);
    ifc_event ::c::d::compound some_event_compound_out1(ifc_inout ::c::d::compound& out_arg1);
    ifc_event ::c::d::compound some_event_compound_out2(ifc_inout ::c::d::compound&out_arg1, ifc_inout ::c::d::compound&out_arg2);



public: // methods only
    explicit parent_class(int some_value);
    static iref<parent_class> get_instance();
protected: //members only
    static inline iref<parent_class> _instance = nullptr;
    int _some_value;
};

}; // end of namespace b

}; // end of namespace a