
#ifndef __INTERGEN_IFC_JS_H__
#define __INTERGEN_IFC_JS_H__

#include <comm/token.h>
#include <v8/v8.h>

///Helper for script loading
struct script_handle
{
    ///Provide path or direct script
    //@param is_path true if path_or_script is a path to the script file, false if it's the script itself
    script_handle( const coid::token& path_or_script, bool is_path )
    : _str(path_or_script), _is_path(is_path)
    {}

    script_handle( v8::Handle<v8::Context> context )
    : _is_path(false), _context(context)
    {}

    script_handle()
    : _is_path(true)
    {}

    void set( const coid::token& path_or_script, bool is_path ) {
        _context.Clear();
        _str = path_or_script;
        _is_path = is_path;
    }

    void set( v8::Handle<v8::Context> context ) {
        _context = context;
        _is_path = false;
        _str.set_null();
    }

    ///Set prefix code to be included before the file/script
    void prefix( const coid::token& p ) {
        _prefix = p;
    }

    const coid::token& prefix() const { return _prefix; }


    bool is_context() const { return !_context.IsEmpty(); }
    bool is_path() const { return _is_path; }
    bool is_script() const { return !_str.is_null() && !_is_path; }
    
    
    const coid::token& str() const {
        return _str;
    }
    
    v8::Handle<v8::Context> context() const {
        return _context;
    }
    
private:

    coid::token _str;
    bool _is_path;

    coid::token _prefix;
    
    v8::Handle<v8::Context> _context;
};


///Unwrap interface object from JS object
template<class T>
inline iref<T> js_unwrap_object( const v8::Handle<v8::Value> &arg )
{
    iref<T> empty;

    if(arg.IsEmpty()) return empty;
    if(!arg->IsObject()) return empty;
    
    v8::Handle<v8::Object> obj = arg->ToObject();
    if(obj->InternalFieldCount() != 2) return empty;
    
    int hashid = (int)(ints)v8::Handle<v8::External>::Cast(obj->GetInternalField(1))->Value();
    if(hashid != T::hashid)
        return empty;

    return iref<T>(static_cast<T*>(v8::Handle<v8::External>::Cast(obj->GetInternalField(0))->Value()));
}


#endif //__INTERGEN_IFC_JS_H__
