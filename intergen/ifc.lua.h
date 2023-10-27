/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2013-2023
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __INTERGEN_IFC_LUA_H__
#define __INTERGEN_IFC_LUA_H__

#include "../token.h"
#include "../dir.h"
#include "../log.h"
#include "../metastream/metastream.h"
#include "../binstream/filestream.h"
#include "../singleton.h"

#include <luaJIT/lua.hpp>
#include <luaJIT/luaext.h>

 //#include "lua_utils.h"

 // rebind v interface objectoch treba

namespace lua {
int ctx_query_interface(lua_State* L);

typedef int (*class_implement_fn)(lua_State*, const coid::token&);

static constexpr coid::token_literal _lua_register_class_key = "lua::register_class";
static constexpr coid::token_literal _lua_implement_class_key = "lua::implement_class";
static constexpr coid::token_literal _lua_parent_index_key = "__index";
static constexpr coid::token_literal _lua_new_index_key = "__newindex";
static constexpr coid::token_literal _lua_member_count_key = "__memcount";
static constexpr coid::token_literal _lua_dispatcher_cptr_key = "__dispatcher_cptr";
static constexpr coid::token_literal _lua_interface_cptr_key = "__interface_cptr";
static constexpr coid::token_literal _lua_class_hash_key = "__class_hash";
static constexpr coid::token_literal _lua_gc_key = "__gc";
static constexpr coid::token_literal _lua_weak_meta_key = "__weak_object_meta";
static constexpr coid::token_literal _lua_context_script_path_key = "__ctx_script_path";
static constexpr coid::token_literal _lua_context_script_dir_key = "__ctx_script_dir";
static constexpr coid::token_literal _lua_implements_fn_name = "implements";
static constexpr coid::token_literal _lua_implements_as_fn_name = "implements_as";
static constexpr coid::token_literal _lua_log_key = "log";
static constexpr coid::token_literal _lua_query_interface_key = "query_interface";
static constexpr coid::token_literal _lua_include_key = "include";
static constexpr coid::token_literal _lua_rebind_events_key = "rebind_events";

const uint32 LUA_WEAK_REGISTRY_INDEX = 1;
const uint32 LUA_WEAK_IFC_METATABLE_INDEX = 2;
const uint32 LUA_CONTEXT_MATETABLE_INDEX = 3;
const uint32 LUA_INTERFACE_METATABLE_REGISTER_INDEX = 4;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class lua_event_exception : public ::coid::exception
{
public:
    explicit lua_event_exception(const char* stext)
        :coid::exception(stext)
    {}
};


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline void insert_indentation(int count, coid::charstr& result)
{
    for (int i = 0; i < count; i++)
    {
        result << "\t";
    }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline void debug_stack_value_to_str(lua_State* L, int idx, bool recursive, coid::charstr& result, int level)
{

    DASSERT(idx <= lua_gettop(L));

    if (lua_isboolean(L, idx)) {

        result << lua_toboolean(L, idx);
    }
    else if (lua_isnumber(L, idx)) {

        result << lua_tonumber(L, idx);
    }
    else if (lua_isstring(L, idx)) {

        result << lua_tostring(L, idx);
    }
    else if (lua_isnil(L, idx)) {

        result << "nil";
    }
    else if (lua_iscfunction(L, idx)) {

        result << "C function";
    }
    else if (lua_isfunction(L, idx)) {

        result << "lua function";
    }
    else if (lua_islightuserdata(L, idx))
    {
        void* ptr = lua_touserdata(L, idx);
        result << "0x";
        result.append_num_uint(16, &ptr, sizeof(&ptr), 0);
    }
    else if (lua_isuserdata(L, idx))
    {
        result << "{\n";
        insert_indentation(level + 1, result);
        void* ptr = lua_touserdata(L, idx);
        result << " memory_address: 0x";
        result.append_num_uint(16, &ptr, sizeof(&ptr));

        if (lua_getmetatable(L, idx) != 0)
        {
            debug_stack_value_to_str(L, lua_gettop(L), recursive, result, level + 2);
            lua_pop(L, 1);
        }

        result << "\n";

        insert_indentation(level, result);
        result << "}";
    }
    else if (lua_istable(L, idx))
    {
        result << "{\n";
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        if (lua_rawequal(L, -1, idx))
        {
            insert_indentation(level, result);
            result << "\t LUA_GLOBAL\n";
            insert_indentation(level, result);
            result << "}";
            lua_pop(L, 1);
            return;
        }

        lua_pop(L, 1);
        lua_pushnil(L);
        while (lua_next(L, idx) != 0)
        {
            insert_indentation(level, result);
            lua_pushvalue(L, -2);
            result << "\t" << lua_tostring(L, -1) << ": ";
            lua_pop(L, 1);

            if (lua_istable(L, -1) && !recursive)
            {
                result << "{...}";
            }
            else
            {
                debug_stack_value_to_str(L, lua_gettop(L), recursive, result, level + 1);
                result << "\n";
            }

            lua_pop(L, 1);
        }

        insert_indentation(level, result);
        result << "}";
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline void debug_print_stack(lua_State* L, int from_index = 0)
{
    const int n = lua_gettop(L);
    if (n > 0) {
        int start_index = 1;
        if (from_index > 0)
        {
            if (from_index <= n)
            {
                start_index = from_index;
            }
        }
        if (from_index < 0)
        {
            if (-n <= from_index)
            {
                start_index = n + from_index + 1;
            }
        }

        coid::charstr res = "";
        for (int i = start_index; i <= n; i++)
        {
            res << "\n" << i << ": ";
            debug_stack_value_to_str(L, i, true, res, 0);
        }

        coidlog_debug("debug_print_stack", res);
    }
    else
    {
        coidlog_debug("debug_print_stack", "The stack is empty!");
    }

    DASSERT(n == lua_gettop(L));
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline void process_exception_and_push_error_string_internal(const coid::exception& e, lua_State* L)
{
    coid::charstr message;

    luaL_where_ext(L, 1);

    if (!lua_isnil(L, -1))
    {
        message << "[" << lua_tointeger(L, -1) << "]: "
            << e.c_str() << " (" << lua_tostring(L, -2) << ")";
        lua_pop(L, 2);
    }
    else
    {
        message << "[unknown line]: "
            << e.c_str() << "(unknown script)";

        lua_pop(L, 1);
    }

    lua_pushtoken(L, message.c_str());
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline int lua_class_new_index_fn(lua_State* L) {
    // -3 = table
    // -2 = key
    // -1 = value

    lua_pushvalue(L, -2);
    lua_rawget(L, -4);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);

        if (lua_isnil(L, -1)) {
            return 0;
        }

        lua_getmetatable(L, -3);
        lua_pushtoken(L, _lua_member_count_key);
        lua_pushvalue(L, -1);
        lua_rawget(L, -3);
        size_t mcount = lua_tointeger(L, -1);
        lua_pop(L, 1);
        lua_pushinteger(L, mcount + 1);
        lua_rawset(L, -3);
        lua_pop(L, 1);

        lua_rawset(L, -3);
    }
    else {
        lua_pop(L, 1);

        if (lua_isnil(L, -1)) {
            lua_getmetatable(L, -3);
            lua_pushtoken(L, _lua_member_count_key);
            lua_pushvalue(L, -1);
            lua_rawget(L, -1);
            size_t mcount = lua_tointeger(L, -1);
            lua_pushinteger(L, mcount - 1);
            lua_rawset(L, -3);
            lua_pop(L, 1);
        }

        lua_rawset(L, -3);
    }

    return 0;
}

inline size_t lua_classlen(lua_State* L, int idx) {
    lua_getmetatable(L, idx);
    lua_pushtoken(L, _lua_member_count_key);
    lua_rawget(L, -2);
    size_t res = lua_tointeger(L, -1);
    lua_pop(L, 2);
    return res;
}

inline void lua_create_class_table(lua_State* L, int mcount = 0, int parent_idx = 0) {
    lua_createtable(L, 0, mcount); // create table
    lua_createtable(L, 0, 3); // metatable

    lua_pushtoken(L, _lua_member_count_key);
    lua_pushnumber(L, 0);
    lua_rawset(L, -3);

    lua_pushtoken(L, _lua_new_index_key);
    lua_pushcfunction(L, lua_class_new_index_fn);
    lua_rawset(L, -3);


    if (parent_idx != 0) {
        lua_pushtoken(L, _lua_parent_index_key);
        lua_pushvalue(L, parent_idx);
        lua_rawset(L, -3);
    }

    lua_setmetatable(L, -2);
}

///////////////////////////////////////////////////////////////////////////////
inline int catch_lua_error(lua_State* L) {
    coid::charstr msg;

    luaL_where_ext(L, 1);

    if (0)
    {
        debug_print_stack(L);
    }

    if (!lua_isnil(L, -1)) // we have stack info where it happens (usually runtime script error like calling non existent funcion)
    {

        DASSERT(lua_isstring(L, -3)); // original error message
        DASSERT(lua_isstring(L, -2)); // script path from luaL_where_ext
        DASSERT(lua_isstring(L, -1)); // line number from luaL_where_ext

        coid::token msg_token = lua_totoken(L, -3);
        msg_token.cut_left("]:"_T);
        coid::token line_num = msg_token.cut_left(':');

        msg << "[" << line_num << "]: " << msg_token << " (" << lua_totoken(L, -2) << ")";

        lua_pop(L, 3); // pop error message, script path and line number

        lua_pushtoken(L, msg);
    }
    else  // we have no stack info(usually when exception is thrown in C function called from script, in that case we already have stack info in error msg)
    {
        lua_pop(L, 1);
    }

    return 1;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
inline void throw_lua_error(lua_State* L, const coid::token& str = coid::token()) {
    coid::exception ex;
    coid::token message(lua_totoken(L, -1));

    ex << message;

    if (str) {
        ex << " (" << str << ')';
    }

    lua_pop(L, 1);
    throw ex;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
inline int ctx_log(lua_State* L) {
    bool exception_caught = false;

    try
    {
        if (lua_gettop(L) != 1)
        {
            throw coid::exception("Wrong number of arguments!");
        }

        if (!lua_isstring(L, -1))
        {
            throw coid::exception("Bad type of argument(string expected)!");
        }

        lua_pushvalue(L, LUA_ENVIRONINDEX);
        coid::token hash;
        lua_getfield(L, -1, _lua_context_script_path_key);

        if (lua_isnil(L, -1)) {
            hash = "Unknown script";
        }
        else {
            hash = lua_totoken(L, -1);
        }

        lua_pop(L, 2);


        coid::token msg = lua_totoken(L, -1);
        coidlog_none(hash, msg);
    }
    catch (const coid::exception& e)
    {
        exception_caught = true;
        process_exception_and_push_error_string_internal(e, L);
    }

    if (exception_caught)
    {
        lua_error(L);
    }

    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline int lua_iref_release_callback(lua_State* L) {
    if (lua_isuserdata(L, -1)) {
        policy_intrusive_base* obj = reinterpret_cast<policy_intrusive_base*>(*static_cast<size_t*>(lua_touserdata(L, -1)));
        obj->release_refcount();
    }

    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class registry_handle :public policy_intrusive_base {
public:
    //static const iref<registry_handle>& get_empty() {
    //    static iref<registry_handle> empty = new registry_handle();
    //    return empty;
    //}

    bool is_empty() {
        return _lua_handle == 0;
    }

    lua_State* get_state() const { return _L; }

    virtual void release() {
        if (_lua_handle) {
            luaL_unref(_L, LUA_REGISTRYINDEX, _lua_handle);
            _lua_handle = 0;
        }
    }

    // set handle to reference object on the top of the stack and pop the object
    virtual void set_ref() {
        if (!is_empty()) {
            release();
        }

        _lua_handle = luaL_ref(_L, LUA_REGISTRYINDEX);
    }

    // push the referenced object onto top of the stack
    virtual void push_ref() {
        if (!is_empty()) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, _lua_handle);
        }
    }

    virtual ~registry_handle() {
        release();
    }

    explicit registry_handle(lua_State* L)
        : _L(L)
        , _lua_handle(0)
    {
    };
protected:
    int32 _lua_handle = 0;
    lua_State* _L = nullptr;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class weak_registry_handle :public registry_handle {
public:

    virtual void release() override {
        if (_lua_handle) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, LUA_WEAK_REGISTRY_INDEX);
            luaL_unref(_L, -1, _lua_handle);
            _lua_handle = 0;
            lua_pop(_L, 1);
        }
    }

    // set handle to reference object on the top of the stack and pop the object
    virtual void set_ref() override {
        if (!is_empty()) {
            release();
        }

        lua_rawgeti(_L, LUA_REGISTRYINDEX, LUA_WEAK_REGISTRY_INDEX);
        lua_insert(_L, -2);
        _lua_handle = luaL_ref(_L, -2);
        lua_pop(_L, 1);
    }

    // push the referenced object onto top of the stack
    virtual void push_ref() override {
        if (!is_empty()) {
            lua_rawgeti(_L, LUA_REGISTRYINDEX, LUA_WEAK_REGISTRY_INDEX);
            lua_rawgeti(_L, -1, _lua_handle);
            lua_insert(_L, -2);
            lua_pop(_L, 1);
        }
    }

    virtual ~weak_registry_handle() {
        release();
    }

    explicit weak_registry_handle(lua_State* L)
        : registry_handle(L)
    {};
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline void implement_class_in_context(lua_State* L, const coid::token& interface_class_name, const coid::token& script_class_name) {
    coid::charstr implement_function_registrar_name;
    implement_function_registrar_name << _lua_implement_class_key << "." << interface_class_name;

    class_implement_fn reg_fn = reinterpret_cast<class_implement_fn>(coid::interface_register::get_interface_creator(implement_function_registrar_name));
    if (!reg_fn) {
        coid::exception e;
        e << interface_class_name << " class not found!";
        throw e;
    }

    reg_fn(L, script_class_name);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline int script_implements(lua_State* L) {
    const int n = lua_gettop(L); // number of arguments
    bool exception_caught = false;

    try
    {
        if (n != 1) // it must contain class name argument
        {
            throw coid::exception("Wrong number of arguments!");
        }

        if (lua_isnumber(L, -1) || !lua_isstring(L, -1)) // it must have string with class name on the top of the stack
        {
            throw coid::exception("The argument of string type expected!");
        }

        coid::token class_name = lua_totoken(L, -1);
        implement_class_in_context(L, class_name, "");
    }
    catch (const coid::exception& e)
    {
        exception_caught = true;
        process_exception_and_push_error_string_internal(e, L);
    }

    if (exception_caught)
    {
        lua_error(L);
    }

    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline int script_implements_as(lua_State* L) {
    const int n = lua_gettop(L); // number of arguments
    bool exception_caught = false;

    try {
        if (n != 2) // it must contain interface class name argument and script class name
        {
            throw coid::exception("Wrong number of arguments!");
        }

        if (lua_isnumber(L, -1) || !lua_isstring(L, -1)) // it must have string with interface class name
        {
            throw coid::exception("The first argument of string type expected!");
        }

        if (lua_isnumber(L, -1) || !lua_isstring(L, -1)) // it must have string with script class name
        {
            throw coid::exception("The second argument of string type expected!");
        }

        coid::token script_class_name = lua_totoken(L, -1);
        coid::token interface_class_name = lua_totoken(L, -2);

        implement_class_in_context(L, interface_class_name, script_class_name);
    }
    catch (const coid::exception& e)
    {
        exception_caught = true;
        process_exception_and_push_error_string_internal(e, L);
    }

    if (exception_caught)
    {
        lua_error(L);
    }

    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline bool is_interface_table(lua_State* L, int index)
{
    const int n = lua_gettop(L); // number of arguments

    if (n < index || n < -index)
    {
        throw coid::exception("Wrong number of arguments!");
    }

    return lua_istable(L, index)
        && lua_hasfield(L, index, _lua_dispatcher_cptr_key)
        && lua_hasfield(L, index, _lua_interface_cptr_key)
        && lua_hasfield(L, index, _lua_class_hash_key)
        && lua_hasfield(L, index, _lua_parent_index_key);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline bool is_context_table(lua_State* L, int index)
{
    const int n = lua_gettop(L); // number of arguments

    if (n < index || n < -index)
    {
        throw coid::exception("Wrong number of arguments!");
    }

    bool result = lua_istable(L, index);
    result &= lua_hasfield(L, index, _lua_context_script_dir_key);
    result &= lua_hasfield(L, index, _lua_context_script_path_key);
    
    return result;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

    //inline int rebind_interface(lua_State* L) {
    //    const int n = lua_gettop(L); // number of arguments
    //    bool exception_caught = false;

    //    try {

    //        if (n != 2) // it must contain interface source interface table and destination
    //        {
    //            throw coid::exception("Wrong number of arguments!");
    //        }

    //        if (is_interface_table(L, -1)) // must be valid interface table
    //        {
    //            throw coid::exception("The first argument is not interface table!");
    //            return 0;
    //        }

    //        if (!lua_istable(L, -2)) // it must have string with script class name
    //        {
    //            throw coid::exception("The second argument is not the string!");
    //        }

    //        coid::token script_class_name = lua_totoken(L, -1);
    //        coid::token interface_class_name = lua_totoken(L, -2);
    //
    //        implement_class_in_context(L, interface_class_name, script_class_name);
    //    }
    //    catch (const coid::exception& e)
    //    {
    //        exception_caught = true;
    //        process_exception_and_push_error_string_internal(e, L);
    //    }

    //    if (exception_caught)
    //    {
    //        lua_error(L);
    //    }

    //    return 0;
    //}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
class lua_state_wrap {
public:
    static lua_state_wrap* get_lua_state() {
        LOCAL_SINGLETON_DEF(lua_state_wrap) _state = new lua_state_wrap();
        return _state.get();
    }

    ~lua_state_wrap() {
        //_throw_fnc_handle.release();
        close_lua();
    }

    lua_State* get_raw_state() { return _L; }

    void close_lua() {
        if (_L) {
            lua_close(_L);
            _L = nullptr;
        }
    }

    void collect_garbage()
    {
        lua_gc(_L, LUA_GCCOLLECT, 0);
    }

private:
    lua_state_wrap() {
        _L = lua_open();

        luaL_openlibs(_L);

        lua_createtable(_L, 0, 0);
        lua_createtable(_L, 0, 1);
        lua_pushtoken(_L, "v");
        lua_setfield(_L, -2, "__mode");
        lua_setmetatable(_L, -2);
        int weak_register_idx = luaL_ref(_L, LUA_REGISTRYINDEX); // ensure the table for storing weak
        DASSERT(weak_register_idx == LUA_WEAK_REGISTRY_INDEX);   //references to be at index 1 in LUA_REGISTRYINDEX table

        lua_createtable(_L, 0, 1);
        lua_pushcfunction(_L, &lua_iref_release_callback);
        lua_setfield(_L, -2, _lua_gc_key);
        int weak_metatable_idx = luaL_ref(_L, LUA_REGISTRYINDEX);
        DASSERT(weak_metatable_idx == LUA_WEAK_IFC_METATABLE_INDEX);


        lua_createtable(_L, 0, 1);
        lua_pushvalue(_L, LUA_GLOBALSINDEX);
        lua_setfield(_L, -2, _lua_parent_index_key);
        int context_metatable_idx = luaL_ref(_L, LUA_REGISTRYINDEX);
        DASSERT(context_metatable_idx == LUA_CONTEXT_MATETABLE_INDEX);

        lua_createtable(_L, 0, 1);
        lua_pushvalue(_L, LUA_GLOBALSINDEX);
        lua_setfield(_L, -2, _lua_parent_index_key);
        const int interface_metatable_register_idx = luaL_ref(_L, LUA_REGISTRYINDEX);
        DASSERT(interface_metatable_register_idx == LUA_INTERFACE_METATABLE_REGISTER_INDEX);
    }
protected:
    lua_State* _L;
};

int ctx_include(lua_State* L); // need forward declaration for context

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
class lua_context : public registry_handle {
public:
    lua_context(lua_State* L)
        :registry_handle(L)
    {
        lua_newtable(_L); // create context table
        lua_rawgeti(_L, LUA_REGISTRYINDEX, LUA_CONTEXT_MATETABLE_INDEX); // get context metatable
        lua_setmetatable(_L, -2);

        lua_pushcfunction(_L, &script_implements);
        lua_pushvalue(_L, -2);
        lua_setfenv(_L, -2);
        lua_setfield(_L, -2, _lua_implements_fn_name);

        lua_pushcfunction(_L, &script_implements_as);
        lua_pushvalue(_L, -2);
        lua_setfenv(_L, -2);
        lua_setfield(_L, -2, _lua_implements_as_fn_name);

        lua_pushcfunction(_L, &ctx_log);
        lua_pushvalue(_L, -2);
        lua_setfenv(_L, -2);
        lua_setfield(_L, -2, _lua_log_key);

        lua_pushcfunction(_L, &ctx_query_interface);
        lua_pushvalue(_L, -2);
        lua_setfenv(_L, -2);
        lua_setfield(_L, -2, _lua_query_interface_key);

        lua_pushcfunction(_L, &ctx_include);
        lua_pushvalue(_L, -2);
        lua_setfenv(_L, -2);
        lua_setfield(_L, -2, _lua_include_key);

        set_ref();
    };

    /*lua_context(const lua_context& ctx)
    {
        _L = ctx._L;
        _lua_handle = ctx._lua_handle;
    }*/

    ~lua_context()
    {
        //lua_gc(_L, LUA_GCCOLLECT, 0);
    };

    void debug_print()
    {
        push_ref();
        debug_print_stack(_L);
        lua_pop(_L, 1);
    }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


inline void print_weak_registry(lua_State* L)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_WEAK_REGISTRY_INDEX);
    lua_getmetatable(L, -1);
    debug_print_stack(L, -2);
    lua_pop(L, 2);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


inline void print_registry(lua_State* L)
{
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    coid::charstr result = "\n{\n";
    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        if (lua_isnumber(L, -2))
        {
            lua_pushvalue(L, -2);
            lua_Integer index = lua_tointeger(L, -1);
            result << "\t" << index << ": ";
            lua_pop(L, 1);

            if (index == LUA_INTERFACE_METATABLE_REGISTER_INDEX)
            {
                result << "LUA_INTERFACE_METATABLE_REGISTER_INDEX";
            }
            else if (index == LUA_WEAK_REGISTRY_INDEX)
            {
                result << "LUA_WEAK_REGISTRY_INDEX";
            }
            else if (index == LUA_CONTEXT_MATETABLE_INDEX)
            {
                result << "LUA_CONTEXT_MATETABLE_INDEX";
            }
            else if (index == LUA_WEAK_IFC_METATABLE_INDEX)
            {
                result << "LUA_WEAK_IFC_METATABLE_INDEX";
            }


            debug_stack_value_to_str(L, lua_gettop(L), true, result, 1);
            result << "\n";
        }

        lua_pop(L, 1);
    }

    result << "}";
    lua_pop(L, 1);

    coidlog_debug("print_registry", result);
}


//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
inline void load_script(const iref<registry_handle>& context, const coid::token& script_code, const coid::token& script_path) {
    if (context.is_empty() || context->is_empty()) {
        throw coid::exception("Can't load script without context!");
    }

    lua_State* L = context->get_state();

#ifdef _DEBUG
    const int stack_top = lua_gettop(L);
#endif

    int res = luaL_loadbuffer(L, script_code._ptr, script_code.len(), script_path);
    if (res != 0) {
        throw_lua_error(L, script_path);
    }

    context->push_ref();

    lua_setfenv(L, -2);

    res = lua_pcall(L, 0, 0, 0);
    if (res != 0) {
        throw_lua_error(L, "");
    }

#ifdef _DEBUG
    const int current_stack_top = lua_gettop(L);
    DASSERT(current_stack_top == stack_top);
#endif
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
inline void load_script(const iref<registry_handle>& context, const coid::token& script_path) {
    coid::token script_tok;
    coid::charstr script_tmp;

    coid::bifstream bif(script_path);
    if (!bif.is_open())
        throw coid::exception() << script_path << " not found";

    coid::binstreambuf buf;
    buf.swap(script_tmp);
    buf.transfer_from(bif);
    buf.swap(script_tmp);

    script_tok = script_tmp;

    load_script(context, script_tok, script_path);
}

///////////////////////////////////////////////////////////////////////////////
inline int ctx_include(lua_State* L) {
    const int n = lua_gettop(L); // number of arguments
    bool exception_caught = false;

    try
    {
        if (0)
        {
            debug_print_stack(L);
        }

        if (n != 1) // it must contain script path
        {
            throw coid::exception("Wrong number of arguments!");
        }

        if (lua_isnumber(L, -1) || !lua_isstring(L, -1)) // it must have string with interface class name
        {
            throw coid::exception("The first argument of string type expected!");
        }

        if (!lua_hasfield(L, LUA_ENVIRONINDEX, ::lua::_lua_context_script_dir_key))
        {
            throw coid::exception("Script context is not valid!");
        }

        coid::charstr script_path;

        coid::token script_path_arg = lua_totoken(L, -1);

        iref<weak_registry_handle> context_weak = new ::lua::weak_registry_handle(L);
        lua_pushvalue(L, LUA_ENVIRONINDEX);
        context_weak->set_ref();

        if (!coid::directory::is_absolute_path(script_path_arg))
        {
            lua_getfield(L, LUA_ENVIRONINDEX, ::lua::_lua_context_script_dir_key);
            script_path = lua_totoken(L, -1);
            lua_pop(L, 1);

            script_path << script_path_arg;
            script_path_arg = script_path;
        }

        load_script(context_weak, script_path);
    }
    catch (const coid::exception& e)
    {
        exception_caught = true;
        process_exception_and_push_error_string_internal(e, L);
    }

    if (exception_caught)
    {
        lua_error(L);
    }

#ifdef _DEBUG
    const int current_stack_top = lua_gettop(L);
    DASSERT(current_stack_top == n);
#endif

    return 0;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    ///Helper for script loading
struct script_handle
{
    ///Provide path or direct script
    /// @param path_or_script path or script content
    /// @param is_path true if path_or_script is a path to the script file, false if it's the script itself
    /// @param url string to use when identifying script origin
    /// @param context
    script_handle(
        const coid::token& path_or_script,
        bool is_path,
        const coid::token& url = coid::token(),
        const iref<registry_handle>& ctx = nullptr
    )
        : _path_or_code(path_or_script), _is_path(is_path), _context(ctx), _url(url)
    {}

    script_handle(lua_context* ctx)
        : _is_path(false), _context(ctx)
    {}

    script_handle()
        : _is_path(true)
    {}

    void set(
        const coid::token& path_or_script,
        bool is_path,
        const coid::token& url = coid::token(),
        const iref<registry_handle>& ctx = nullptr
    )
    {
        _path_or_code = path_or_script;
        _is_path = is_path;
        _context = ctx;
        _url = url;
    }

    void set(const iref<registry_handle>& ctx) {
        _context = ctx;
        _is_path = false;
        _path_or_code.set_null();
    }

    ///Set prefix code to be included before the file/script
    void prefix(const coid::token& p) {
        _prefix = p;
    }

    const coid::token& prefix() const { return _prefix; }


    bool is_path() const { return _is_path; }
    bool is_script() const { return !_is_path && !_path_or_code.is_null(); }
    bool is_context() const { return _path_or_code.is_null(); }
    bool has_context() const { return !_context.is_empty(); }


    const coid::token& str() const {
        return _path_or_code;
    }

    const coid::token& url() const { return _url ? _url : (_is_path ? _path_or_code : _url); }
    const iref<registry_handle>& context() const {
        return _context;
    }

    iref<registry_handle> load_script(lua_State* L) const
    {
#ifdef _DEBUG
        const int stack_top = lua_gettop(L);
#endif

        if (is_context())
        {
            return _context;
        }

        iref<lua_context> context = new ::lua::lua_context(L);


        const coid::token url_tok = url();

        coid::token script_dir;
        script_dir = url_tok;
        if (script_dir.contains_back('\\') || script_dir.contains_back('/')) {
            script_dir.cut_right_group_back(coid::DIR_SEPARATORS, coid::token::cut_trait_keep_sep_default_empty());
        }

        context->push_ref();
        lua_pushtoken(L, script_dir);
        lua_setfield(L, -2, ::lua::_lua_context_script_dir_key);
        lua_pushtoken(L, url_tok);
        lua_setfield(L, -2, ::lua::_lua_context_script_path_key);

        lua_pop(L, 1);

        coid::charstr script_code_buf;
        coid::token script_code;

        if (_is_path)
        {
            coid::bifstream bif(_path_or_code);
            if (!bif.is_open())
                throw coid::exception() << url_tok << " not found";

            script_code_buf = prefix();

            coid::binstreambuf buf;
            buf.swap(script_code_buf);
            buf.transfer_from(bif);
            buf.swap(script_code_buf);

            script_code = script_code_buf;
        }
        else if (!_prefix.is_empty())
        {
            script_code_buf = _prefix;
            script_code_buf.append(_path_or_code);
            script_code = script_code_buf;
        }
        else
        {
            script_code = _path_or_code;
        }

        ::lua::load_script(context, script_code, url_tok);

#ifdef _DEBUG
        const int current_stack_top = lua_gettop(L);
        DASSERT(current_stack_top == stack_top);
#endif

        return context;
    }

private:

    coid::token _path_or_code;
    bool _is_path;

    coid::token _prefix;
    coid::token _url;

    iref<registry_handle> _context;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
struct interface_context
{
    iref<weak_registry_handle> _context = nullptr;
    iref<registry_handle> _object = nullptr;

    interface_context() {
    }

    void set_context(const iref<registry_handle>& context)
    {
        DASSERT(!context.is_empty());

        lua_State* L = context->get_state();

        _context.create(new weak_registry_handle(L));
        context->push_ref();
        DASSERT(is_context_table(L, -1));
        _context->set_ref();
    }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
template<class T>
class interface_wrapper_base
    : public T
    , public interface_context
{
public:
    iref<T> _base;                      //< original c++ interface object

    intergen_interface* intergen_real_interface() override final
    {
        return _base ? _base.get() : this;
    }

    T* _real() { return static_cast<T*>(intergen_real_interface()); }

    ~interface_wrapper_base()
    {
        _base.release();
    }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
    ///Unwrap interface object from LUA object on the top of the stack
template<class T>
inline T* unwrap_interface(lua_State* L)
{
    if (!lua_istable(L, -1)) return 0;

    if (!lua_hasfield(L, -1, _lua_interface_cptr_key) || !lua_hasfield(L, -1, _lua_class_hash_key)) return 0;

    lua_getfield(L, -1, _lua_interface_cptr_key);
    intergen_interface* p = reinterpret_cast<intergen_interface*>(*static_cast<size_t*>(lua_touserdata(L, -1)));
    lua_pop(L, 1);

    lua_getfield(L, -1, _lua_class_hash_key);
    int hashid = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);

    if (hashid != p->intergen_hash_id())    //sanity check
        return 0;

    if (!p->iface_is_derived(T::HASHID))
        return 0;

    return static_cast<T*>(p->intergen_real_interface());
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
inline iref<registry_handle> wrap_interface(intergen_interface* orig, const iref<registry_handle>& ctx)
{
    if (!orig) {
        return nullptr;
    }

    typedef iref<registry_handle>(*fn_wrapper)(intergen_interface*, const iref<registry_handle>&);
    fn_wrapper fn = static_cast<fn_wrapper>(orig->intergen_wrapper(intergen_interface::backend::lua));

    if (fn) {
        return fn(orig, ctx);
    }

    return nullptr;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// /// @brief Wrap data host object to a LUA object handle
template <class T>
inline iref<registry_handle> wrap_data_object(T* data, const coid::token& ifcname, const iref<registry_handle>& context)
{
    if (!data) return nullptr;

    typedef iref<registry_handle>(*fn_dcmaker)(void*, const iref<registry_handle>&);
    static fn_dcmaker fn = static_cast<fn_dcmaker>(coid::interface_register::get_script_interface_dcmaker(ifcname, "lua"_T));

    if (fn)
        return fn(data, context);
    
    return nullptr;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template <class T>
inline iref<registry_handle> wrap_data_interface(const coref<T>& data, const coid::token& ifcname, const iref<registry_handle>& context)
{
    typedef iref<registry_handle>(*fn_wrapper)(const coref<T>&, const iref<registry_handle>&);
    static fn_wrapper fn = static_cast<fn_wrapper>(coid::interface_register::get_script_interface_dcwrapper(ifcname, "lua"_T));

    if (fn)
        return fn(data, context);

    return nullptr;
}

/*
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline bool push_interface(lua_State* L, const iref<registry_handle>& ref)
{
    iref<registry_handle> handle = wrap_interface(ref.get(), ifc->_context);
    if (handle.is_empty()) {
        lua_pushnil(L);
    }
    else {
        handle->push_ref();
    }
}*/

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline __declspec(noinline) int ctx_query_interface_exc(lua_State* L)
{
    bool exception_caught = false;

    try {
        if (lua_gettop(L) < 1) {
            throw coid::exception("Wrong number of arguments!");
        }

        lua_pushbot(L); // move creator key onto top of the stack

        if (lua_isnumber(L, -1) || !lua_isstring(L, -1))
            throw coid::exception("The first argument of string type expected!");

        coid::token tokey = lua_totoken(L, -1);

        typedef int(*fn_get)(lua_State* L, interface_context*);
        fn_get get = reinterpret_cast<fn_get>(
            coid::interface_register::get_interface_creator(tokey));

        if (!get) {
            coid::charstr tmp = "interface creator ";
            tmp << tokey << " not found";
            throw coid::exception(tmp);
        }

        lua_pop(L, 1); // pop redundant data from stack

        get(L, nullptr);
        return 1;
    }
    catch (const coid::exception& e) {
        exception_caught = true;
        process_exception_and_push_error_string_internal(e, L);
    }

    if (exception_caught)
    {
        lua_error(L);
    }


    return -1;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

inline int ctx_query_interface(lua_State* L) {
    int res = ctx_query_interface_exc(L);
    if (res == -1) {
        lua_error(L);
    }

    return res;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

} //namespace lua


#endif //__INTERGEN_IFC_LUA_H__
