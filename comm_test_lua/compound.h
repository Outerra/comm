#pragma once

#include <comm/metastream/metastream.h>
#include <comm/str.h>
#include <comm/dynarray.h>



namespace c
{

struct other_compound 
{
    int _int_val;
    float _float_val;
    bool _bool_val;
    coid::charstr _str_val;
    coid::dynarray<int> _int_arr_val;

    friend coid::metastream& operator||(coid::metastream& m, other_compound& script)
    {
        return m.compound_type(script, [&]()
        {
            m.member("integer_value", script._int_val);
            m.member("float_value", script._float_val);
            m.member("bool_value", script._bool_val);
            m.member("string_value", script._str_val);
            m.member("integer_array_value", script._int_arr_val);
        });
    }

    bool operator==(const other_compound& val)
    {
#ifdef _DEBUG
        if (val._bool_val != _bool_val)
        {
            return false;
        }
        if (val._int_val != _int_val)
        {
            return false;
        }
        if (val._float_val != _float_val)
        {
            return false;
        }
        if (val._str_val != _str_val)
        {
            return false;
        }
        if (val._int_arr_val != _int_arr_val)
        {
            return false;
        }

        return true;
#else  
        return
            (val._bool_val == _bool_val) &&
            (val._int_val == _int_val) &&
            (val._float_val == _float_val) &&
            (val._str_val == _str_val) &&
            (val._int_arr_val == _int_arr_val);
#endif
    }

    other_compound()
        : _int_val(0)
        , _float_val(0.0f)
        , _bool_val(false)
        , _str_val("default string")
        , _int_arr_val()

    {}
};

namespace d
{
struct compound
{
    int _int_val;
    float _float_val;
    bool _bool_val;
    coid::charstr _str_val;
    coid::dynarray<int> _int_arr_val;
    other_compound _other_compound_val;

    friend coid::metastream& operator||(coid::metastream& m, compound& c)
    {
        return m.compound_type(c, [&]()
        {
            m.member("integer_value", c._int_val);
            m.member("float_value", c._float_val);
            m.member("bool_value", c._bool_val);
            m.member("string_value", c._str_val);
            m.member("integer_array_value", c._int_arr_val);
            m.member("other_compound_value", c._other_compound_val);

        });
    }

    bool operator==(const compound& val)
    {
#ifdef _DEBUG
        if(val._bool_val != _bool_val) 
        {
            return false;
        }
        if(val._int_val != _int_val) 
        {
            return false;
        }
        if(val._float_val != _float_val) 
        {
            return false;
        }
        if(val._str_val != _str_val)
        {
            return false;
        }
        if (val._int_arr_val != _int_arr_val) 
        {
            return false;
        }
        if (val._other_compound_val != _other_compound_val)
        {
            return false;
        }

        return true;
#else  
        return
            (val._bool_val == _bool_val) &&
            (val._int_val == _int_val) &&
            (val._float_val == _float_val) &&
            (val._str_val == _str_val) &&
            (val._int_arr_val == _int_arr_val) &&
            (val._other_compound_val == _other_compound_val);
#endif
    }

    compound()
        : _int_val(0)
        , _float_val(0.0f)
        , _bool_val(false)
        , _str_val("default string")
        , _int_arr_val()
        , _other_compound_val()
    {}
};
}; // end of namespace d
}; // end of namespace c