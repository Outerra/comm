#include "factory.hpp"
#include "parent.hpp"
#include "child.hpp"
#include "compound.h"

#include "ifc/parent_class_ifc.lua.h"
#include "ifc/child_class_ifc.lua.h"

#include <comm/dynarray.h>
#include <comm/log/logger.h>
#include <comm/metastream/fmtstreamjson.h>
#include <comm/binstream/filestream.h>
#include <comm/metastream/fmtstreamjson.h>

constexpr coid::token fail_prefix = "fail_"_T;
constexpr coid::token succ_prefix = "succ_"_T;

struct test_script
{
public:
    enum class result_type
    {
        void_type,
        bool_type,
        int_type,
        float_type,
        string_type,
        int_array_type,
        compound_type
    };

    coid::charstr _script_path;
    coid::token _test_name;
    coid::charstr _script_class_name;
    coid::charstr _script_binding_variable_name;

    bool _call_event_with_default_body = false;
    bool _should_throw = false;
    bool _is_retvoid = false;
    bool _test_non_equal = false;
    bool _use_child_class = false;
    uint _num_outargs;
   

    result_type _type;
    union {
        bool _exception;
        bool _bool;
        int _int;
        float _float;
        coid::charstr* _string_ptr;
        coid::dynarray<int>* _int_array_ptr;
        c::d::compound* _compound_ptr;
    } _expected_value;

    template <typename T> const T& get_result_value() const
    {
        DASSERT(0);
    }

    using void_ptr = void*;

    template<> const void_ptr& get_result_value<>() const
    {
        static void_ptr dummy = nullptr;
        return dummy;
    }

    template<> const int& get_result_value<>() const
    {
        return _expected_value._int;
    }

    template<> const bool& get_result_value<>() const
    {
        return _expected_value._bool;
    }

    template<> const float& get_result_value<>() const
    {
        return _expected_value._float;
    }

    template<> const coid::charstr& get_result_value<>() const
    {
        return *_expected_value._string_ptr;
    }

    template<> const coid::dynarray<int>& get_result_value<>() const
    {
        return *_expected_value._int_array_ptr;
    }
    
    template<> const c::d::compound& get_result_value<>() const
    {
        return *_expected_value._compound_ptr;
    }

    ~test_script()
    {
        if (_type == result_type::string_type)
        {
            delete _expected_value._string_ptr;
        }

        if (_type == result_type::int_array_type)
        {
            delete _expected_value._int_array_ptr;
        }

        if (_type == result_type::compound_type)
        {
            delete _expected_value._compound_ptr;
        }
    }

    friend coid::metastream& operator||(coid::metastream& m, test_script& script)
    {
        return m.compound("test_script", [&]()
        {            
            m.member_optional("call_event_with_default_body", script._call_event_with_default_body, true);

            m.member_optional("should_throw", script._should_throw, false);

            m.member_optional("is_retvoid", script._is_retvoid, false);

            m.member_optional("use_child_class", script._use_child_class, false);

            m.member_optional("num_outargs", script._num_outargs);

            m.member_optional("test_non_equal", script._test_non_equal);

            m.member_as_type<coid::charstr>("script_path", 
                script, 
                &set_script_path,
                &get_script_path);

            m.member_optional("script_class_name", script._script_class_name, "");

            m.member_optional("script_binding_variable_name", script._script_binding_variable_name, "");

            m.member_optional_as_type<bool>("bool_result",
                script,
                &bool_setter,
                &bool_getter);

            m.member_optional_as_type<int>("int_result", 
                script, 
                &int_setter,
                &int_getter);

            m.member_optional_as_type<float>("float_result",
                script,
                &float_setter,
                &float_getter);

            m.member_optional_as_type<coid::token>("string_result",
                script,
                &string_setter,
                &string_getter);

            m.member_optional_as_type<coid::dynarray<int>>("int_array_result",
                script,
                &int_array_setter,
                &int_array_getter);

            m.member_optional_as_type<c::d::compound>("compound_result",
                script,
                &compound_setter,
                &compound_getter);

            m.member_optional("exception_result", script._expected_value._exception);

        });
    }

    private:
        friend class coid::dynarray<test_script>;
        test_script()
        : _script_path("")
        , _type(result_type::void_type)
        , _should_throw(false)
        , _call_event_with_default_body(true)
        , _is_retvoid(false)
        , _num_outargs(0)
        , _test_non_equal(false)
        {}

        static void set_script_path(const coid::charstr& script_path, test_script& dst)
        {
            dst._script_path = script_path;
            constexpr coid::token extension_postfix = ".lua"_T;
            dst._test_name = dst._script_path;
            dst._test_name.shift_end(-4);
        }

        static const coid::charstr& get_script_path(const test_script& dst)
        {
            throw coid::exception("read only structure");
            return dst._script_path;
        }

        static void int_setter(int val, test_script& dst)
        {
            dst._expected_value._int = val;
            dst._type = result_type::int_type;
        }

        static int int_getter(const test_script& src)
        {
            throw coid::exception("read only structure");
            return src._expected_value._int;
        }


        static void bool_setter(bool val, test_script& dst)
        {
            dst._expected_value._bool = val;
            dst._type = result_type::bool_type;
        }

        static bool bool_getter(const test_script& src)
        {
            throw coid::exception("read only structure");
            return src._expected_value._bool;
        }

        static void float_setter(float val, test_script& dst)
        {
            dst._expected_value._float = val;
            dst._type = result_type::float_type;
        }

        static float float_getter(const test_script& src)
        {
            throw coid::exception("read only structure");
            return src._expected_value._float;
        }

        static void string_setter(coid::token val, test_script& dst)
        {
            dst._expected_value._string_ptr = new coid::charstr(val);
            dst._type = result_type::string_type;
        }

        static coid::token string_getter(const test_script& src)
        {
            throw coid::exception("read only structure");
            return "";
        }

        static void int_array_setter(const coid::dynarray<int>& val, test_script& dst)
        {
            dst._expected_value._int_array_ptr = new coid::dynarray<int>(val);
            dst._type = result_type::int_array_type;
        }

        static coid::dynarray<int> int_array_getter(const test_script& src)
        {
            throw coid::exception("read only structure");
            return coid::dynarray<int>();
        }

        static void compound_setter(const c::d::compound& val, test_script& dst)
        {
            dst._expected_value._compound_ptr = new c::d::compound(val);
            dst._type = result_type::compound_type;
        }

        static c::d::compound compound_getter(const test_script& src)
        {
            throw coid::exception("read only structure");
            return c::d::compound();
        }
};

struct test_result
{
    bool _passed;
    coid::charstr _error_message;
};


ref<coid::stdoutlogger> logger;

static ref<coid::logmsg> canlog(coid::log::level type, const coid::token& src, const void* inst, coid::log::target target) {
    return logger->create_msg(type, target, src, inst);
}

static coid::logger* getlog() {
    return logger.get();
}

#define BREAK_ON_TEST(current_test, test_name) {if(test_name == current_test._test_name) __debugbreak();}

template <typename T, bool child>
void test_template(lua_State* L, coid::token test_path, const test_script& test, test_result& result)
{
    const int stack_top_begin = lua_gettop(L);
    a::factory& factory_instance = a::factory::get_instance();

    try
    {
        if (0)
        {
            ::lua::print_registry(L);
        }

        ::lua::script_handle sh(test_path, true);

        iref<a::b::parent_class> class_instance;
        iref<::lua::registry_handle> context;
        iref<a::b::parent_class_ifc> class_instance_client;

        if constexpr (child)
        {
            class_instance = factory_instance.create_child_item(35);
            class_instance_client =
                a::b::c::lua::child_class_ifc::_get(L, sh, class_instance.get(), test._script_binding_variable_name, test._script_class_name, &context);
        }
        else 
        {
            class_instance = factory_instance.create_parent_item(35);
            class_instance_client =
                a::b::lua::parent_class_ifc::_get(L, sh, class_instance.get(), test._script_binding_variable_name, test._script_class_name, &context);
        }


        factory_instance.get_item(0)->some_event_default();
        const uint items_count = factory_instance.get_items_count();

        DASSERT(!factory_instance.get_last_item().is_empty());

        if (0)
        {
            ::lua::print_registry(L);
        }

        lua_gc(L, LUA_GCCOLLECT, 0);

        if (0)
        {
            ::lua::print_registry(L);
        }

        T res = T();

        if (test._call_event_with_default_body)
        {
            if constexpr (std::is_same<T, void*>::value)
            {
                DASSERT(!factory_instance.get_last_item().is_empty());
                factory_instance.get_last_item()->some_event_default();
            }
            else if constexpr (std::is_same<T, int>::value)
            {
                if constexpr (child)
                {
                    res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_default();
                }
                else 
                {
                    res = factory_instance.get_last_item()->some_event_int_default();
                }
            }
            else if constexpr (std::is_same<T, float>::value)
            {
                if constexpr (child)
                {
                    res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_float_default();
                }
                else
                {
                    res = factory_instance.get_last_item()->some_event_float_default();
                }
            }
            else if constexpr (std::is_same<T, bool>::value)
            {
                if constexpr (child)
                {
                    res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_bool_default();
                }
                else
                {
                    res = factory_instance.get_last_item()->some_event_bool_default();
                }
            }
            else if constexpr (std::is_same<T, coid::charstr>::value)
            {
                if constexpr (child)
                {
                    res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_string_default();
                }
                else
                {
                    res = factory_instance.get_last_item()->some_event_string_default();
                }
            }
            else if constexpr (std::is_same<T, coid::dynarray<int>>::value)
            {
                if constexpr (child)
                {
                    res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_arr_default();
                }
                else
                {
                    res = factory_instance.get_last_item()->some_event_int_arr_default();
                }
            }
            else if constexpr (std::is_same<T, c::d::compound>::value)
            {
                if constexpr (child)
                {
                    res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_compound_default();
                }
                else
                {
                    res = factory_instance.get_last_item()->some_event_compound_default();
                }
            }
            else 
            {
                RASSERT(0 && "not implemented");
            }
            
            result._passed &= (res == test.get_result_value<T>()) != test._test_non_equal;
            
        }
        else
        {
            if (test._is_retvoid)
            {
                if (test._num_outargs == 1)
                {
                    T out_param0 = T();
                    if constexpr (std::is_same<T, int>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_retvoid_out1(out_param0);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_int_retvoid_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, float>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_float_retvoid_out1(out_param0);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_float_retvoid_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, bool>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_bool_retvoid_out1(out_param0);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_bool_retvoid_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::charstr>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_string_retvoid_out1(out_param0);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_string_retvoid_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::dynarray<int>>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_arr_retvoid_out1(out_param0);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_int_arr_retvoid_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, c::d::compound>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_compound_retvoid_out1(out_param0);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_compound_retvoid_out1(out_param0);
                        }
                    }
                    else
                    {
                        RASSERT(0 && "not implemented");
                    }

                    result._passed &= (out_param0 == test.get_result_value<T>()) != test._test_non_equal;
                }
                else if (test._num_outargs == 2)
                {
                    T out_param0 = T();
                    T out_param1 = T();
                    if constexpr (std::is_same<T, int>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_retvoid_out2(out_param0, out_param1);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_int_retvoid_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, float>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_float_retvoid_out2(out_param0, out_param1);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_float_retvoid_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, bool>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_bool_retvoid_out2(out_param0, out_param1);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_bool_retvoid_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::charstr>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_string_retvoid_out2(out_param0, out_param1);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_string_retvoid_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::dynarray<int>>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_arr_retvoid_out2(out_param0, out_param1);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_int_arr_retvoid_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, c::d::compound>::value)
                    {
                        if constexpr (child)
                        {
                            static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_compound_retvoid_out2(out_param0, out_param1);
                        }
                        else
                        {
                            factory_instance.get_last_item()->some_event_compound_retvoid_out2(out_param0, out_param1);
                        }
                    }
                    else
                    {
                        RASSERT(0 && "not implemented");
                    }
                    
                    result._passed &= (out_param0 == test.get_result_value<T>()) != test._test_non_equal;
                    result._passed &= (out_param1 == test.get_result_value<T>()) != test._test_non_equal;
                }
                else
                {
                    if constexpr (child)
                    {
                        static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event();
                    }
                    else
                    {
                        factory_instance.get_last_item()->some_event();
                    }
                }
            }
            else
            {
                if (test._num_outargs == 0)
                {
                    if constexpr (std::is_same<T, int>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int();
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_int();
                        }
                    }
                    else if constexpr (std::is_same<T, float>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_float();
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_float();
                        }
                    }
                    else if constexpr (std::is_same<T, bool>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_bool();
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_bool();
                        }
                    }
                    else if constexpr (std::is_same<T, coid::charstr>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_string();
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_string();
                        }
                    }
                    else if constexpr (std::is_same<T, coid::dynarray<int>>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_arr();
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_int_arr();
                        }
                    }
                    else if constexpr (std::is_same<T, c::d::compound>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_compound();
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_compound();
                        }
                    }
                    else
                    {
                        RASSERT(0 && "not implemented");
                    }

                    result._passed &= (res == test.get_result_value<T>()) != test._test_non_equal;
                    
                }
                else if (test._num_outargs == 1)
                {
                    T out_param0 = T();
                    if constexpr (std::is_same<T, int>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_out1(out_param0);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_int_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, float>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_float_out1(out_param0);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_float_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, bool>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_bool_out1(out_param0);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_bool_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::charstr>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_string_out1(out_param0);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_string_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::dynarray<int>>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_arr_out1(out_param0);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_int_arr_out1(out_param0);
                        }
                    }
                    else if constexpr (std::is_same<T, c::d::compound>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_compound_out1(out_param0);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_compound_out1(out_param0);
                        }
                    }
                    else
                    {
                        RASSERT(0 && "not implemented");
                    }

                    result._passed &= (res == test.get_result_value<T>()) != test._test_non_equal;
                    result._passed &= (out_param0 == test.get_result_value<T>()) != test._test_non_equal;
                }
                else if (test._num_outargs == 2)
                {
                    T out_param0 = T();
                    T out_param1 = T();
                    if constexpr (std::is_same<T, int>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_out2(out_param0, out_param1);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_int_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, float>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_float_out2(out_param0, out_param1);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_float_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, bool>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_bool_out2(out_param0, out_param1);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_bool_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::charstr>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_string_out2(out_param0, out_param1);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_string_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, coid::dynarray<int>>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_int_arr_out2(out_param0, out_param1);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_int_arr_out2(out_param0, out_param1);
                        }
                    }
                    else if constexpr (std::is_same<T, c::d::compound>::value)
                    {
                        if constexpr (child)
                        {
                            res = static_cast<a::b::c::child_class*>(factory_instance.get_last_item().get())->some_event_compound_out2(out_param0, out_param1);
                        }
                        else
                        {
                            res = factory_instance.get_last_item()->some_event_compound_out2(out_param0, out_param1);
                        }
                    }
                    else
                    {
                        RASSERT(0 && "not implemented");
                    }

                    result._passed &= (res == test.get_result_value<T>()) != test._test_non_equal;
                    result._passed &= (out_param0 == test.get_result_value<T>()) != test._test_non_equal;
                    result._passed &= (out_param1 == test.get_result_value<T>()) != test._test_non_equal;                    
                }
                else
                {
                    DASSERT(0);
                }
            }
        }

        if (!test._should_throw)
        {
            result._passed &= true;
        }
        else
        {
            result._passed &= false;
        }
        
            const int stack_top_end = lua_gettop(L);
        
            DASSERT(stack_top_begin == stack_top_end);
    }
    catch (const coid::exception& e)
    {
        if (test._should_throw)
        {
            result._passed = true;
        }
        else
        {
            result._passed = false;
        }

        result._error_message = e.c_str();
        const int stack_top_end = lua_gettop(L);
        DASSERT(stack_top_begin == stack_top_end);
    }

    factory_instance.reset();
}

int main() 
{
    logger.create(new coid::stdoutlogger);
    coid::interface_register::setup(coid::directory::get_cwd(), &canlog, 0, &getlog);

    ::lua::lua_state_wrap* lua_state = ::lua::lua_state_wrap::get_lua_state();

    a::factory::initialize();

    coid::charstr tests_dir = SCRIPTDIRPATH;
    tests_dir << "/scripts/";

    coid::charstr test_path = tests_dir;
    test_path << "tests.json";

    coid::dynarray<test_script> tests;

    coid::fmtstreamjson json_fmt(true, true);
    coid::bifstream file_stream(test_path);

    if (!file_stream.is_open())
    {
        coidlog_error("", "Can't open file with tests!");
        return -1;
    }

    json_fmt.bind(file_stream);
    coid::metastream meta(json_fmt);
    
    try {
        meta.xstream_in(tests);
    }
    catch (coid::exception& e)
    {
        coidlog_error("", e.c_str());
        logger->terminate();
        logger.release();
        return -1;
    }

    static bool backward = false;
    coid::dynarray<test_result> results;
    results.add_uninit(tests.size());

    for(int i = backward ? coid::down_cast<int>(tests.size()) - 1 : 0;
        backward ? i >= 0 : i < coid::down_cast<int>(tests.size());
        backward ? --i : ++i)
    {
        test_script& test = tests[i];
        test_path = tests_dir;
        test_path << test._script_path;

        test_result* result = new (results.ptr() + i) test_result();
        result->_passed = true;

        lua_State* L = lua_state->get_raw_state();
        const int stack_top_begin = lua_gettop(L);
     
        //BREAK_ON_TEST(test, "events_109");
        
        if (test._use_child_class)
        {
            if (test._type == test_script::result_type::void_type)
            {
                test_template<void*, true>(L, test_path, test, *result);
            }

            if (test._type == test_script::result_type::int_type)
            {
                test_template<int, true>(L, test_path, test, *result);
            }

            if (test._type == test_script::result_type::float_type)
            {
                test_template<float, true>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::bool_type)
            {
                test_template<bool, true>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::string_type)
            {
                test_template<coid::charstr, true>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::int_array_type)
            {
                test_template<coid::dynarray<int>, true>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::compound_type)
            {
                test_template<c::d::compound, true>(L, test_path, test, *result);
            }
        }
        else 
        {
            if (test._type == test_script::result_type::void_type)
            {
                test_template<void*, false>(L, test_path, test, *result);
            }

            if (test._type == test_script::result_type::int_type)
            {
                test_template<int, false>(L, test_path, test, *result);
            }

            if (test._type == test_script::result_type::float_type)
            {
                test_template<float, false>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::bool_type)
            {
                test_template<bool, false>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::string_type)
            {
                test_template<coid::charstr, false>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::int_array_type)
            {
                test_template<coid::dynarray<int>, false>(L, test_path, test, *result);
            }
            if (test._type == test_script::result_type::compound_type)
            {
                test_template<c::d::compound, false>(L, test_path, test, *result);
            }
        }
    }
    
    uint passed_count = 0;
    bool all_passed = true;

    for (uints i = 0; i < tests.size(); i++)
    {
        all_passed &= results[i]._passed;
        passed_count += results[i]._passed ? 1 : 0;

        coidlog_none("", "Test name: " << tests[i]._test_name);
        if (results[i]._passed)
        {
            coidlog_none("", "Result: SUCCEEDED");
        }
        else 
        {
            coidlog_none("", "Error:Result: FAILED");
        }
        
        if (!results[i]._error_message.is_empty())
        {
            coidlog_none("", "Warning:Error message:" << results[i]._error_message);
        }
    }

    coidlog_none("", "Test passed : " << passed_count << " of " << tests.size());

    if (all_passed)
    {
        coidlog_none("", "Info:TESTS SUCCEEDED!");
    }
    else 
    {
        coidlog_none("", "Error:TESTS FAILED!");
    }

    a::factory::shutdown();

    lua_state->close_lua();
    
    logger->terminate();
    logger.release();

    return 0;
}