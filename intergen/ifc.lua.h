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
 * Portions created by the Initial Developer are Copyright (C) 2013
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

#include <comm/interface.h>
#include <comm/token.h>
#include <comm/dir.h>
#include <comm/metastream/metastream.h>
#include <comm/metastream/fmtstream_lua_capi.h>
#include <comm/binstream/filestream.h>
#include <comm/singleton.h>
#include <luaJIT/lua.hpp>
#include <luaJIT/luaext.h>

namespace lua {

    static const char * _lua_class_register_key = "__ifc_class_register";
    static const char * _lua_mt_index_key = "__index";
    static const char * _lua_global_table_key = "_G";
    static const char * _lua_implements_fn_name = "$implements";

////////////////////////////////////////////////////////////////////////////////
    inline void register_class_to_lua(lua_State * L, const coid::token& class_name) {
        coid::charstr class_registrar_name;
        class_registrar_name << "lua::" << class_name << "_ifc.register_class";
        lua_CFunction reg_fn = reinterpret_cast<lua_CFunction>(coid::interface_register::get_interface_creator(class_registrar_name));
        if (!reg_fn) {
            throw coid::exception() << class_name << " class is not registered!";
        }

        reg_fn(L);
    }

////////////////////////////////////////////////////////////////////////////////
    inline int script_implements(lua_State * L) {
        coid::metastream ms;
        ms.bind_formatting_stream(coid::fmtstream_lua_capi(L));
        coid::charstr class_name;
        ms.stream_out(class_name);
        lua_getglobal(L, _lua_class_register_key);
        lua_getfield(L, -1, class_name.c_str());
        
        // class is not registered so try to register class.
        if (lua_isnil(L, -1)) {
            register_class_to_lua(L, class_name);
        }

        lua_getfield(L, -1, class_name.c_str());

        int32 tlen = lua_objlen(L, -1);
        lua_createtable(L, 0, tlen);
        lua_pushnil(L);

        while (lua_next(L, -2)) {
            lua_pushvalue(L, -2);
            lua_insert(L, -2);
            lua_settable(L, -4);
        }

        lua_setfield(L, LUA_ENVIRONINDEX, class_name.c_str());

        return 0;
    }

////////////////////////////////////////////////////////////////////////////////
    inline int throw_lua_error(lua_State * L) {
        coid::exception ex;
        coid::token message(lua_totoken(L, -1));

        ex << message;
        throw ex;
        return 0;
    }

////////////////////////////////////////////////////////////////////////////////
    class registry_handle :public policy_intrusive_base {
    public:
//        int32 get_handle() { return _lua_handle; };
        
        bool is_empty() {
            return _lua_handle == 0;
        }

        void create_new_handle(lua_State * L) {
            if (!is_empty()) {
                release();
            }
            _L = L;
            _lua_handle = lua_ref(L, LUA_REGISTRYINDEX);
        }

        lua_State * get_state() { return _L; }

        void release() {
            if (_lua_handle) {
                lua_unref(_L, _lua_handle);
            }
        }

        // set handle to reference object on the top of the stack and pop the object
        void set_ref() {
            if (!is_empty()) {
                lua_rawseti(_L, LUA_REGISTRYINDEX, _lua_handle);
            }
        }

        // push the referenced object onto top of the stack
        void get_ref() {
            if (!is_empty()) {
                lua_rawgeti(_L, LUA_REGISTRYINDEX, _lua_handle);
            }
        }

        virtual ~registry_handle() {
            release();
        }

        registry_handle()
            : _lua_handle(0)
            , _L(nullptr)
        {};

        registry_handle(lua_State * L) {
            _L = L;
            create_new_handle(L);
        };
    protected:
        int32 _lua_handle;
        lua_State * _L;
    };

////////////////////////////////////////////////////////////////////////////////
    class lua_state_wrap {
    public:
        static lua_state_wrap * get_lua_state() {
            LOCAL_SINGLETON_DEF(lua_state_wrap) _state = new lua_state_wrap();
            return _state.get();
        }

        ~lua_state_wrap() {
            _throw_fnc_handle.release();
            lua_close(_L);
        }

        lua_State * get_raw_state() { return _L; }

    private:
        lua_state_wrap() {
            _L = lua_open();
            luaL_openlibs(_L);
            
            _throw_fnc_handle.create_new_handle(_L);
            lua_pushcfunction(_L, &throw_lua_error);
            _throw_fnc_handle.set_ref();
            
            lua_pushcfunction(_L, &script_implements);
            lua_setglobal(_L, _lua_implements_fn_name);

            lua_getglobal(_L, _lua_global_table_key);
            _global_context.create_new_handle(_L);
            _global_context.set_ref();
        }
    protected:
        lua_State * _L;
        registry_handle _throw_fnc_handle;
        registry_handle _global_context;
    };

////////////////////////////////////////////////////////////////////////////////
    class lua_context: public registry_handle{
    public:
        bool activate_context(int32 fun_idx) {
            lua_rawgeti(_L,LUA_REGISTRYINDEX,_lua_handle);
            return lua_setfenv(_L,fun_idx) != 0;
        }
        
        lua_context(lua_State * L)
            :registry_handle(L)
        {
            lua_newtable(_L);
            lua_getmetatable(L,-1);
            lua_pushnumber(L, LUA_GLOBALSINDEX);
            lua_setfield(L, -1, _lua_mt_index_key);
            lua_pop(L, 1);
            lua_rawseti(L, LUA_REGISTRYINDEX, _lua_handle);
        };

        lua_context(const lua_context& ctx) {
            _L = ctx._L;
            _lua_handle = ctx._lua_handle;
        }

        ~lua_context() {
        };
    };

////////////////////////////////////////////////////////////////////////////////
    class lua_script : public registry_handle{
    public:
        lua_script() 
            : registry_handle()
        {    
        }

        void compile(const coid::token& script_code, const coid::token& script_path) {
            if (!has_context) {
                coid::exception ex;
                ex << "Can't compile LUA script without context!";
                throw ex;
            }
            
            lua_State * L = _context->get_state();

            int res = luaL_loadbuffer(L,script_code._ptr,script_code.len(),script_path);
            if (res != 0) {
                throw_lua_error(L);
            }
            
            create_new_handle(L);
            lua_rawseti(L,LUA_REGISTRYINDEX,_lua_handle);
        };

        void run() {
            if (is_empty()) {
               

            }
        }

        bool has_context() {
            return !_context.is_empty();
        }

        void set_context(lua_context * ctx) {
            _context = ctx;
        }
    protected:
        iref<lua_context> _context;
    };

////////////////////////////////////////////////////////////////////////////////
    ///Helper for script loading
    struct script_handle
    {
        ///Provide path or direct script
        //@param path_or_script path or script content
        //@param is_path true if path_or_script is a path to the script file, false if it's the script itself
        //@param url string to use when identifying script origin
        //@param context 
        script_handle(
            const coid::token& path_or_script,
            bool is_path,
            const coid::token& url = coid::token(),
            lua_context* ctx = new lua_context(lua_state_wrap::get_lua_state()->get_raw_state())
        )
            : _str(path_or_script), _is_path(is_path), _context(ctx), _url(url)
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
            lua_context * ctx = new lua_context(lua_state_wrap::get_lua_state()->get_raw_state())
        )
        {
            _str = path_or_script;
            _is_path = is_path;
            _context = ctx;
            _url = url;
        }

        void set(lua_context * ctx) {
            _context = ctx;
            _is_path = false;
            _str.set_null();
        }

        ///Set prefix code to be included before the file/script
        void prefix(const coid::token& p) {
            _prefix = p;
        }

        const coid::token& prefix() const { return _prefix; }


        bool is_path() const { return _is_path; }
        bool is_script() const { return !_is_path && !_str.is_null(); }
        bool is_context() const { return _str.is_null(); }
        bool has_context() const { return !_context.is_empty(); }


        const coid::token& str() const {
            return _str;
        }

        const coid::token& url() const { return _url ? _url : (_is_path ? _str : _url); }

        const iref<lua_context>& context() const {
            return _context;
        }

        ///Load and run script
        lua_script load_script()
        {
            
            iref<lua_state_wrap> state = lua_state_wrap::get_lua_state();
            if (!has_context()) {
                _context = new lua_context(state->get_raw_state());
            }

            coid::token script_tok, script_path = _url;
            coid::charstr script_tmp;
            
            if (is_path()) {
                if (!script_path)
                    script_path = _str;

                coid::bifstream bif(_str);
                if (!bif.is_open())
                    throw coid::exception() << _str << " not found";

                script_tmp = _prefix;

                coid::binstreambuf buf;
                buf.swap(script_tmp);
                buf.transfer_from(bif);
                buf.swap(script_tmp);

                script_tok = script_tmp;
            }
            else if (_prefix) {
                script_tmp << _prefix << _str;
                script_tok = script_tmp;
            }
            else {
                script_tok = _str;
            }

            lua_script script;
            script.set_context(_context.get());
            script.compile(script_tok, script_path);
            script.run();
        }

    private:

        coid::token _str;
        bool _is_path;

        coid::token _prefix;
        coid::token _url;

        iref<lua_context> _context;
    };

////////////////////////////////////////////////////////////////////////////////
    struct interface_context
    {
        iref<lua_context> _context;
        iref<lua_script> _script;
        iref<registry_handle> _object;
    };
    
////////////////////////////////////////////////////////////////////////////////
    template<class T>
    class interface_wrapper_base
        : public T
        , public interface_context
    {
    public:
        iref<T> _base;                      //< original c++ interface object

                                            //T* _real() { return _base ? _base.get() : this; }
        intergen_interface* intergen_real_interface() override final { return _base ? _base.get() : this; }

        T* _real() { return static_cast<T*>(intergen_real_interface()); }
    };

////////////////////////////////////////////////////////////////////////////////
  /*  ///Unwrap interface object from JS object
    template<class T>
    inline T* unwrap_object(const v8::Handle<v8::Value> &arg)
    {
        if (arg.IsEmpty()) return 0;
        if (!arg->IsObject()) return 0;

        v8::Handle<v8::Object> obj = arg->ToObject();
        if (obj->InternalFieldCount() != 2) return 0;

        intergen_interface* p = static_cast<intergen_interface*>(
            v8::Handle<v8::External>::Cast(obj->GetInternalField(0))->Value());

        int hashid = (int)(ints)v8::Handle<v8::External>::Cast(obj->GetInternalField(1))->Value();
        if (hashid != p->intergen_hash_id())    //sanity check
            return 0;

        if (!p->iface_is_derived(T::HASHID))
            return 0;

        return static_cast<T*>(p->intergen_real_interface());
    }*/

////////////////////////////////////////////////////////////////////////////////
/*inline iref<registry_handle> wrap_object(intergen_interface* orig, iref<lua_context> ctx)
    {
    if (!orig) {
        return nullptr;
    }



        if (!orig) return v8::Null();
        v8::HandleScope handle_scope;


        typedef v8::Handle<v8::Value>(*fn_wrapper)(intergen_interface*, v8::Handle<v8::Context>);
        fn_wrapper fn = static_cast<fn_wrapper>(orig->intergen_wrapper(intergen_interface::IFC_BACKEND_JS));

        if (fn)
#ifdef V8_MAJOR_VERSION
            return handle_scope.Escape(fn(orig, context));
#else
            return handle_scope.Close(fn(orig, context));
#endif
        return v8_Undefined(iso);
    }*/

////////////////////////////////////////////////////////////////////////////////
/*    inline bool bind_object(const coid::token& bindname, intergen_interface* orig, v8::Handle<v8::Context> context)
    {
        if (!orig) return false;

#ifdef V8_MAJOR_VERSION
        v8::Isolate* iso = v8::Isolate::GetCurrent();
        v8::HandleScope handle_scope(iso);
#else
        v8::HandleScope handle_scope;
#endif

        typedef v8::Handle<v8::Value>(*fn_wrapper)(intergen_interface*, v8::Handle<v8::Context>);
        fn_wrapper fn = static_cast<fn_wrapper>(orig->intergen_wrapper(intergen_interface::IFC_BACKEND_JS));

#ifdef V8_MAJOR_VERSION
        return fn && context->Global()->Set(v8::String::NewFromOneByte(iso,
            (const uint8*)bindname.ptr(), v8::String::kNormalString, bindname.len()), fn(orig, context));
#else
        return fn && context->Global()->Set(v8::String::New(bindname.ptr(), bindname.len()), fn(orig, context));
#endif
    }*/
} //namespace lua


#endif //__INTERGEN_IFC_LUA_H__