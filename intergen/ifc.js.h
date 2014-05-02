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

#ifndef __INTERGEN_IFC_JS_H__
#define __INTERGEN_IFC_JS_H__

#include <comm/token.h>
#include <comm/metastream/metastream.h>
#include <comm/metastream/fmtstream_v8.h>
#include <v8/v8.h>

///Helper for script loading
struct script_handle
{
    ///Provide path or direct script
    //@param is_path true if path_or_script is a path to the script file, false if it's the script itself
    //@param context 
    script_handle( const coid::token& path_or_script, bool is_path, v8::Handle<v8::Context> context = v8::Handle<v8::Context>() )
    : _str(path_or_script), _is_path(is_path), _context(context)
    {}

    script_handle( v8::Handle<v8::Context> context )
    : _is_path(false), _context(context)
    {}

    script_handle()
    : _is_path(true)
    {}

    void set( const coid::token& path_or_script, bool is_path, v8::Handle<v8::Context> context = v8::Handle<v8::Context>() ) {
        _str = path_or_script;
        _is_path = is_path;
        _context = context;
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


    bool is_path() const { return _is_path; }
    bool is_script() const { return !_is_path && !_str.is_null(); }
    bool is_context() const { return _str.is_null(); }
    bool has_context() const { return !_context.IsEmpty(); }
    
    
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

namespace js {

////////////////////////////////////////////////////////////////////////////////
struct interface_context
{
    coid::metastream _meta;
    coid::fmtstream_v8 _fmtv8;

    v8::Persistent<v8::Context> _context;
    v8::Persistent<v8::Script> _script;
    v8::Persistent<v8::Object> _object;
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
class interface_wrapper_base
    : public T
    , public interface_context
{
public:
    iref<T> _base;

    T* _real() { return _base ? _base.get() : this; }
};	

////////////////////////////////////////////////////////////////////////////////
///Unwrap interface object from JS object
template<class T>
inline T* unwrap_object( const v8::Handle<v8::Value> &arg )
{
    if(arg.IsEmpty()) return 0;
    if(!arg->IsObject()) return 0;
    
    v8::Handle<v8::Object> obj = arg->ToObject();
    if(obj->InternalFieldCount() != 2) return 0;
    
    int hashid = (int)(ints)v8::Handle<v8::External>::Cast(obj->GetInternalField(1))->Value();
    if(hashid != T::HASHID)
        return 0;

    interface_wrapper_base<T>* p = static_cast<interface_wrapper_base<T>*>(
        v8::Handle<v8::External>::Cast(obj->GetInternalField(0))->Value());
    return p->_real();
}

} //namespace js


#endif //__INTERGEN_IFC_JS_H__
