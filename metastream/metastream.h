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
 * Robert Strycek.
 * Portions created by the Initial Developer are Copyright (C) 2003-2017
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

 /** @file */


#ifndef __COID_COMM_METASTREAM__HEADER_FILE__
#define __COID_COMM_METASTREAM__HEADER_FILE__

#include "../namespace.h"
#include "../dynarray.h"
#include "../range.h"
#include "../str.h"
#include "../commexception.h"

#include "fmtstream.h"
#include "fmtstreamnull.h"
#include "metavar.h"

#include <type_traits>

COID_NAMESPACE_BEGIN

/** \class metastream
Declaring metastream operator for custom types allows to persist objects from/to
various format sources. Example:

struct waypoint
{
    double3 pos;
    float4 rot;
    float3 dir;
    float weight;
    float speed;
    uint color;

    friend metastream& operator || (metastream& m, waypoint& w)
    {
        m.compound_type(w, [&]()
        {
            m.member("pos", w.pos);
            m.member("rot", w.rot);
            m.member_obsolete<float>("speed");      //use member_obsolete for data that were removed from the struct but can still exist in stream
            m.member("dir", w.dir);
            m.member("weight", w.weight, 1);
            m.member("speed", w.speed, 10.0f);

            //to format color as a string, use get/set lambdas
            m.member_type<coid::charstr>("color", "#ffff",
                [&](const coid::charstr& v) { w.color = str2color(v); },
                [&]() { return color2str(w.color); }
                );

        });
    }
};

Then using metastream class and one of the formatting streams to stream in/out the object:

    bifstream bif("waypoint.cfg");
    fmtstreamcxx fmt(bif);
    metastream m(fmt);

    waypoint x;
    m.stream_in(x);
**/

////////////////////////////////////////////////////////////////////////////////
///Base class for streaming structured data
class metastream
{
public:

    bool streaming() const { return _binw || _binr; }
    bool stream_reading() const { return _binr; }
    bool stream_writing() const { return _binw; }

    typedef bstype::kind            type;
    void* context = NULL;


    metastream() = default;
    metastream(const metastream&) = delete;
    metastream& operator=(const metastream&) = delete;

    explicit metastream(fmtstream& bin)
    {
        bind_formatting_stream(bin);
    }

    explicit metastream(fmtstream& bin, void* ctx)
    {
        context = ctx;
        bind_formatting_stream(bin);
    }

    ~metastream()
    {
        if (_sesopen > 0)
            stream_flush();
        else if (_sesopen < 0)
            stream_acknowledge(true);
    }

    ///Initialize object from its metastream operator, assuming all members have defaults
    template<class T>
    static void initialize_from_defaults(T* that) {
        fmtstreamnull fmt;
        metastream meta(fmt);

        meta.xstream_in(*that);
    }

    ///Set metastream context and return the previous one
    void* set_context(void* ctx) {
        void* prev = context;
        context = ctx;
        return prev;
    }

    ///Define struct streaming scheme
    /// @param fn functor with member functions defining the struct layout
    template<typename T, typename Fn>
    metastream& compound_type(T&, Fn fn)
    {
        if (streaming()) {
            _xthrow(movein_process_key(_binr != 0 ? READ_MODE : WRITE_MODE));
            _rvarname.reset();

            movein_struct(_binr != 0);
            fn();
            moveout_struct(_binr != 0);
        }
        else if (!meta_insert(typeid(T).name(), sizeof(T), false)) {
            fn();

            _last_var = _smap->pop();
            _last_var->desc->streaming_type = _last_var->desc;

            _current_var = _smap->last();

            meta_exit();
        }
        return *this;
    }

    ///Define a compound type that streams as a different (possibly plain) type
    /// @param set void function(T&, Tstream&&) receiving object from stream
    /// @param get [const Tstream& | Tstream] function(T&) returning object to stream
    template<typename Tstream, typename T, typename Fn, typename FnIn, typename FnOut>
    metastream& compound_type_stream_as_type(T& v, Fn fn, FnIn set, FnOut get)
    {
        if (_binw) {
            const Tstream& val = get(v);
            *this || const_cast<Tstream&>(val);
        }
        else if (_binr) {
            Tstream val;
            *this || val;
            set(v, std::forward<Tstream>(val));
        }
        else {
            MetaDesc* md = meta_insert(typeid(T).name(), sizeof(T), false);
            if (!md)
            {
                fn();

                _last_var = _smap->pop();
                md = _last_var->desc;

                _current_var = _smap->last();

                meta_exit();
            }

            //capture alias streaming type
            MetaDesc::Var* oldvar = _current_var;
            MetaDesc::Var* oldlastvar = _last_var;
            MetaDesc desc;
            MetaDesc::Var var;
            var.desc = &desc;
            _current_var = &var;

            *this || *(Tstream*)0;

            md->streaming_type = desc.children[0].desc;
            _current_var = oldvar;
            _last_var = oldlastvar;
        }

        return *this;
    }

    ///Define struct streaming scheme with separate structures for streaming and reflection
    /// @param fnrefl functor with member functions defining the struct layout for reflection
    /// @param fnstream functor with member functions defining the struct layout for streaming
    template<typename T, typename FnRefl, typename FnStream>
    metastream& compound_type_stream_as(T&, FnRefl fnrefl, FnStream fnstream)
    {
        if (streaming()) {
            _xthrow(movein_process_key(_binr != 0 ? READ_MODE : WRITE_MODE));
            _rvarname.reset();

            movein_struct(_binr != 0);
            fnstream();
            moveout_struct(_binr != 0);
        }
        else {
            MetaDesc* md = meta_insert(typeid(T).name(), sizeof(T), false);
            if (!md)
            {
                fnrefl();

                _last_var = _smap->pop();
                md = _last_var->desc;

                _current_var = _smap->last();

                meta_exit();
            }

            //capture alias streaming type
            MetaDesc::Var* oldvar = _current_var;
            MetaDesc::Var* oldlastvar = _last_var;
            MetaDesc desc;
            MetaDesc::Var var;
            var.desc = &desc;
            _current_var = &var;

            fnstream();

            md->streaming_type = desc.children[0].desc;
            _last_var = oldlastvar;
            _current_var = oldvar;
        }

        return *this;
    }



    ///Define struct streaming scheme [OBSOLETE - use compound_type instead]
    /// @param name unique struct type name
    /// @param fn functor with member functions defining the struct layout
    template<typename Fn>
    metastream& compound(const token& name, Fn fn)
    {
        if (streaming()) {
            _xthrow(movein_process_key(_binr != 0 ? READ_MODE : WRITE_MODE));
            _rvarname.reset();

            movein_struct(_binr != 0);
            fn();
            moveout_struct(_binr != 0);
        }
        else if (!meta_insert(name, _curvar.var ? _curvar.var->desc->type_size : 0, false))
        {
            fn();

            _last_var = _smap->pop();
            _last_var->desc->streaming_type = _last_var->desc;

            _current_var = _smap->last();

            meta_exit();
        }
        return *this;
    }

    ///Define struct streaming scheme, where the members correspond to the physical layout
    /// @param fn functor with member functions defining the struct layout
    template<typename T, typename Fn>
    metastream& plain_type(T&, Fn fn)
    {
        if (streaming()) {
            _xthrow(movein_process_key(_binr != 0 ? READ_MODE : WRITE_MODE));
            _rvarname.reset();

            movein_struct(_binr != 0);
            fn();
            moveout_struct(_binr != 0);
        }
        else if (!meta_insert(typeid(T).name(), sizeof(T), true))
        {
            fn();

            _last_var = _smap->pop();
            _last_var->desc->streaming_type = _last_var->desc;

            _current_var = _smap->last();

            meta_exit();
        }
        return *this;
    }

    ///Define a member variable
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool member(const token& name, T& v)
    {
        if (streaming()) {
            *this || *(typename resolve_enum<T>::type*)&v;
            return true;
        }

        meta_variable<T>(name, &v);
        return false;
    }

    ///Define a member variable with default value to use if missing in stream
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @param defval value to use if the variable is missing from the stream, convertible to T
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename D>
    bool member(const token& name, T& v, const D& defval)
    {
        bool used = false;

        if (_binw) {
            *this || *(typename resolve_enum<T>::type*)&v;
            used = true;
        }
        else if (_binr) {
            used = read_optional(v);
            if (!used)
                v = static_cast<const T&>(defval);
        }
        else
            meta_variable_optional<T>(name, &v);

        return used;
    }

    ///Define a member variable with default value to use if missing in stream
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @param defval value to use if the variable is missing from the stream, convertible to T
    /// @param write_default if false, does not write value that equals the defval into output stream
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename D>
    bool member(const token& name, T& v, const D& defval, bool write_default)
    {
        bool used = false;

        if (_binw) {
            used = write_optional(!cache_prepared() && !write_default && v == static_cast<const T&>(defval)
                ? 0 : (typename resolve_enum<T>::type*)&v);
        }
        else if (_binr) {
            used = read_optional(v);
            if (!used)
                v = static_cast<const T&>(defval);
        }
        else
            meta_variable_optional<T>(name, &v);

        return used;
    }

    ///Define a fixed size array member variable
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @param optional true if value may not exist
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, size_t N>
    bool member(const token& name, T(&v)[N], bool optional = false)
    {
        return member_fixed_size_array(name, v, optional, false);
    }

    ///Define a member variable referenced by a pointer (non-streamable) except const char*
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to variable to read/write to
    /// @param streamed false variable should not be streamed, just a part of meta description, can point to 1..N items (default)
    /// @param          true variable is indirectly referenced to by a pointer
    /// @note use member_optional or member_type for special handling when the pointer has to be allocated etc.
    template<typename T, typename = std::enable_if_t<!std::is_same<const char*, T*>::value>>
    bool member(const token& name, T*& v, bool streamed = false)
    {
        return member_ptr(name, v, streamed);
    }

    ///Define a member variable referenced by a pointer, assumed to be already allocated when streaming to/from
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to variable to read/write to
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool member_indirect(const token& name, T*& v)
    {
        typedef typename std::remove_const<T>::type TNC;

        if (streaming()) {
            if (!v)
                throw exception() << "null pointer";

            *this || *(typename resolve_enum<TNC>::type*)v;
            return true;
        }
        else
            meta_variable_indirect<TNC>(name, (ints)&v);

        return false;
    }

    ///Define a raw pointer member variable to 1..N objects
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to variable to read/write to
    /// @param streamed false if variable should not be streamed, just a part of meta description
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool member_ptr(const token& name, T*& v, bool streamed)
    {
        typedef typename std::remove_const<T>::type TNC;

        if (streaming()) {
            *this || *(typename resolve_enum<TNC>::type*)v;
            return true;
        }
        else
            meta_variable_raw_ptr<TNC>(name, (ints)&v, streamed);

        return false;
    }

    ///Define a raw pointer member variable to 1..N objects
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to variable to read/write to
    /// @param streamed false if variable should not be streamed, just a part of meta description
    template<typename T, typename Telem>
    metastream& member_container_type(
        T& v,
        Telem** raw_ptr,
        bool streamed,
        MetaDesc::fn_ptr fnptr,
        MetaDesc::fn_count fncount,
        MetaDesc::fn_push fnpush,
        MetaDesc::fn_extract fnextract)
    {
        if (stream_reading()) {
            meta_container mc(_curvar.var->desc, &v);
            read_container(mc);
        }
        else if (stream_writing()) {
            meta_container mc(_curvar.var->desc, &v);
            write_container(mc);
        }
        else {
            if (meta_decl_container(
                typeid(T).name(),
                sizeof(v),
                streamed,
                raw_ptr ? (ints)raw_ptr - (ints)&v : -1,
                fnptr, fncount, fnpush, fnextract
            ))
                *this || *(typename resolve_enum<Telem>::type*)0;
        }

        return *this;
    }


    ///Define a member variable, with default value constructed by streaming the value object from nullstream
    /// @note obviously, T's metastream declaration must be made only of members with default values
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    template<typename T>
    metastream& member_stream_default(const token& name, T& v)
    {
        if (streaming())
            *this || (typename resolve_enum<T>::type&)v;
        else {
            meta_variable<T>(name, &v);
            meta_cache_default_stream<T>(&v);
        }
        return *this;
    }


    ///Define a variable streamed as a different type, with explicit set/get functors
    /// @param name variable name, used as a key in output formats
    /// @param set void function(AsType&&, T&) receiving object from stream
    /// @param get [const AsType& | AsType] function(T&) returning object to stream
    template<typename AsType, typename T, typename FnIn, typename FnOut>
    metastream& member_as_type(const token& name, T& v, FnIn set, FnOut get)
    {
        if (_binw) {
            const AsType& val = get(v);
            *this || const_cast<AsType&>(val);
        }
        else if (_binr) {
            if constexpr (std::is_same_v<token, AsType>) {
                //special handling for tokens that are normally not readable from stream
                charstr val;
                *this || val;
                set(token(val), v);
            }
            else {
                AsType val;
                *this || val;
                set(std::forward<AsType>(val), v);
            }
        }
        else
            meta_variable<AsType>(name, 0);
        return *this;
    }

    ///Define a variable streamed as a different type, with explicit set/get functors and a default value
    /// @param name variable name, used as a key in output formats
    /// @param defval value to use if the variable is missing from the stream, convertible to AsType
    /// @param set void function(AsType&&, T&) receiving object from stream
    /// @param get [const AsType& | AsType] function(T&) returning object to stream
    template<typename AsType, typename T, typename D, typename FnIn, typename FnOut>
    metastream& member_as_type(const token& name, T& v, FnIn set, FnOut get, const D& defval)
    {
        if (_binw) {
            if constexpr (std::is_same_v<token, AsType>) {
                const charstr& val = get(v);
                *this || const_cast<charstr&>(val);
            }
            else {
                const AsType& val = get(v);
                *this || const_cast<AsType&>(val);
            }
        }
        else if (_binr) {
            if constexpr (std::is_same_v<token, AsType>) {
                //special handling for tokens that are normally not readable from stream
                charstr val;
                *this || val;
                set(token(val), v);
            }
            else {
                AsType val;
                *this || val;
                set(std::forward<AsType>(val), v);
            }
        }
        else {
            meta_variable<AsType>(name, 0);
            meta_cache_default(AsType(defval));
        }
        return *this;
    }

    ///Define a variable streamed as a different type, with explicit set/get functors and a default value, with optional writing of default value
    /// @param name variable name, used as a key in output formats
    /// @param defval value to use if the variable is missing from the stream, convertible to T
    /// @param set void function(AsType&&, T&) receiving object from stream
    /// @param get [const AsType& | AsType] function(T&) returning object to stream
    /// @param write_default if false, does not write value that equals the defval into output stream
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename AsType, typename T, typename D, typename FnIn, typename FnOut>
    bool member_as_type(const token& name, T& v, FnIn set, FnOut get, const D& defval, bool write_default)
    {
        bool used = false;

        if (_binw) {
            if coid_constexpr_if(std::is_same_v<token, AsType>) {
                const charstr& val = get(v);
                used = write_optional(!cache_prepared() && !write_default && val == defval ? 0 : &val);
            }
            else {
                const AsType& val = get(v);
                used = write_optional(!cache_prepared() && !write_default && val == defval ? 0 : &val);
            }
        }
        else if (_binr) {
            if coid_constexpr_if(std::is_same_v<token, AsType>) {
                //special handling for tokens that are normally not readable from stream
                charstr val;
                used = read_optional(val);
                if (used)
                    set(token(val), v);
                else
                    set(defval, v);
            }
            else {
                AsType val;
                used = read_optional(val);
                if (used)
                    set(std::forward<AsType>(val), v);
                else
                    set(defval, v);
            }
        }
        else
            meta_variable_optional<AsType>(name, (const AsType*)-1);

        return used;
    }

    ///Define a variable of given type, with explicit set/get functors
    /// @param name variable name, used as a key in output formats
    /// @param set void function(T&&) receiving object from stream
    /// @param get [const T& | T] function() returning object to stream
    template<typename T, typename FnIn, typename FnOut>
    metastream& member_type(const token& name, FnIn set, FnOut get)
    {
        if (_binw) {
            const T& val = get();
            *this || const_cast<T&>(val);
        }
        else if (_binr) {
            T val;
            *this || val;
            set(std::forward<T>(val));
        }
        else
            meta_variable<T>(name, 0);
        return *this;
    }

    ///Define a variable of given type, with explicit set/get functors and a default value
    /// @param name variable name, used as a key in output formats
    /// @param defval value to use if the variable is missing from the stream, convertible to T
    /// @param set void function(T&&) receiving object from stream
    /// @param get [const T& | T] function() returning object to stream
    template<typename T, typename D, typename FnIn, typename FnOut>
    metastream& member_type(const token& name, const D& defval, FnIn set, FnOut get)
    {
        if (_binw) {
            const T& val = get();
            *this || const_cast<T&>(val);
        }
        else if (_binr) {
            T val;
            *this || val;
            set(std::forward<T>(val));
        }
        else {
            meta_variable<T>(name, 0);
            meta_cache_default(T(defval));
        }
        return *this;
    }

    ///Define a variable of given type, with explicit set/get functors and a default value, with optional writing of default value
    /// @param name variable name, used as a key in output formats
    /// @param defval value to use if the variable is missing from the stream, convertible to T
    /// @param set void function(T&&) receiving object from stream
    /// @param get [const T& | T] function() returning object to stream
    /// @param write_default if false, does not write value that equals the defval into output stream
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename D, typename FnIn, typename FnOut>
    bool member_type(const token& name, const D& defval, FnIn set, FnOut get, bool write_default)
    {
        bool used = false;

        if (_binw) {
            const T& val = get();
            used = write_optional(!cache_prepared() && !write_default && val == static_cast<const T&>(defval) ? 0 : &val);
        }
        else if (_binr) {
            T val;
            used = read_optional(val);
            if (used)
                set(std::forward<T>(val));
            else
                set(defval);
        }
        else
            meta_variable_optional<T>(name, (const T*)-1);

        return used;
    }

    ///Define an optional variable, with explicit set/get functors that take pointer (optionally nullptr)
    /// @param name variable name, used as a key in output formats
    /// @param set void function(const T*) receiving streamed object, or nullptr if the object wasn't present in the stream
    /// @param get const T* function() called to provide object to be streamed, or nullptr if nothing should go into the stream
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename FnIn, typename FnOut>
    bool member_type_optional(const token& name, FnIn set, FnOut get)
    {
        bool used = false;

        if (_binw) {
            const T* p = get();
            used = write_optional(p);
        }
        else if (_binr) {
            T val;
            used = read_optional(val);
            if (used)
                set(&val);
            else
                set(nullptr);
        }
        else
            meta_variable_optional<T>(name, (const T*)-1);

        return used;
    }

    ///Define an optional variable, value doesn't get overwritten if it wasn't present in the input stream
    /// @param name variable name, used as a key in output formats
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool member_optional(const token& name, T& v)
    {
        bool used = false;

        if (_binw) {
            *this || v;
            used = true;
        }
        else if (_binr) {
            used = read_optional(v);
        }
        else
            meta_variable_optional<T>(name, &v);

        return used;
    }

    ///Define an optional variable, value doesn't get overwritten if it wasn't present in the input stream
    /// @param name variable name, used as a key in output formats
    /// @param defval default value for optional writing of values
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename D>
    bool member_optional(const token& name, T& v, const D& defval)
    {
        bool used = false;

        if (_binw) {
            used = write_optional(v == static_cast<const T&>(defval) ? 0 : &v);
        }
        else if (_binr) {
            used = read_optional(v);
        }
        else
            meta_variable_optional<T>(name, &v);

        return used;
    }

    /// Define an optional variable (value doesn't get overwritten if it wasn't present in the input) streamed as a different type, with explicit set/get functors, not writing variable to output if equals the default
    /// @param name variable name, used as a key in output formats
    /// @param set void function(AsType&&, T&) receiving object from stream
    /// @param get [const AsType& | AsType] function(T&) returning object to stream
    /// @param defval used to suppress writing the default value
    /// @return true if value was read or written, false in meta phase
    template<typename AsType, typename T, typename FnIn, typename FnOut>
    bool member_optional_as_type(const token& name, T& v, FnIn set, FnOut get)
    {
        bool used = false;

        if (_binw) {
            if constexpr (std::is_same_v<token, AsType>) {
                const charstr& val = get(v);
                used = write_optional(&val);
            }
            else {
                const AsType& val = get(v);
                used = write_optional(&val);
            }
        }
        else if (_binr) {
            if constexpr (std::is_same_v<token, AsType>) {
                //special handling for tokens that are normally not readable from stream
                charstr val;
                used = read_optional(val);
                if (used)
                    set(token(val), v);
            }
            else {
                AsType val;
                used = read_optional(val);
                if (used)
                    set(std::forward<AsType>(val), v);
            }
        }
        else
            meta_variable_optional<AsType>(name, (const AsType*)-1);

        return used;
    }

    /// Define an optional variable (value doesn't get overwritten if it wasn't present in the input) streamed as a different type, with explicit set/get functors, not writing variable to output if equals the default
    /// @param name variable name, used as a key in output formats
    /// @param set void function(AsType&&, T&) receiving object from stream
    /// @param get [const AsType& | AsType] function(T&) returning object to stream
    /// @param defval used to suppress writing the default value
    /// @return true if value was read or written, false in meta phase
    template<typename AsType, typename T, typename FnIn, typename FnOut>
    bool member_optional_as_type(const token& name, T& v, FnIn set, FnOut get, const T& defval)
    {
        bool used = false;

        if (_binw) {
            if (v == defval) {
                used = write_optional<AsType>(nullptr);
            }
            else if constexpr (std::is_same_v<token, AsType>) {
                const charstr& val = get(v);
                used = write_optional(&val);
            }
            else {
                const AsType& val = get(v);
                used = write_optional(&val);
            }
        }
        else if (_binr) {
            if constexpr (std::is_same_v<token, AsType>) {
                //special handling for tokens that are normally not readable from stream
                charstr val;
                used = read_optional(val);
                if (used)
                    set(token(val), v);
            }
            else {
                AsType val;
                used = read_optional(val);
                if (used)
                    set(std::forward<AsType>(val), v);
            }
        }
        else
            meta_variable_optional<AsType>(name, (const AsType*)-1);

        return used;
    }

    ///Define a variable that can have a finite set of values, mapped to strings in output stream
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @param values array of possible values
    /// @param names array of names corresponding to the values array, terminated by nullptr
    /// @param defval value to use if no input string matches
    /// @param write_default if false, does not write value that equals the defval into output stream
    /// @note if written value is not present in the list, nothing is written out
    /// @note if read string doesn't match any string from the set, defval is set
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool member_enum(const token& name, T& v, const T values[], const char* const names[], const T& defval, bool write_default = true)
    {
        bool used = false;

        if (_binw) {
            if (!cache_prepared() && !write_default && v == defval)
                used = write_optional((const char**)0);
            else {
                int i = 0;
                while (names[i] != 0 && !(values[i] == v))
                    ++i;
                used = write_optional(names[i] ? &names[i] : 0);
            }
        }
        else if (_binr) {
            used = read_optional(_convbuf);
            if (used) {
                int i = 0;
                while (names[i] != 0 && _convbuf != names[i])
                    ++i;
                if (names[i])
                    v = values[i];
                else
                    v = defval;
            }
            else
                v = defval;
        }
        else
            meta_variable_optional<charstr>(name, (const charstr*)&v);

        return used;

    }

    ///Define an obsolete member - not present in the object, ignored on output, but doesn't fail when present in the input stream
    /// @param name variable name, used as a key in output formats
    /// @param v optional pointer to the read value, not written if not found in stream
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool member_obsolete(const token& name, T* v = nullptr)
    {
        bool used = false;

        if (!streaming()) {
            meta_variable_obsolete<T>(name);
        }
        else if (stream_reading()) {
            if (cache_prepared()) {
                _current->offs += sizeof(uints);
                moveto_expected_target(true);
            }
            else {
                T temp;
                used = read_optional(v ? *v : temp);

                if (used && !v)
                    warn_obsolete(name);
            }
        }
        else if (cache_prepared())
            _current->offs += sizeof(uints);

        return used;
    }


    ///Define a non-member variable (no valid offset within the parent struct)
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool nonmember(const token& name, T& v)
    {
        if (streaming()) {
            *this || *(typename resolve_enum<T>::type*)&v;
            return true;
        }

        meta_variable<T>(name, &v, true);
        return false;
    }

    ///Define a non-member variable with default value to use if missing in stream
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @param defval value to use if the variable is missing from the stream, convertible to T
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename D>
    bool nonmember(const token& name, T& v, const D& defval)
    {
        bool used = false;

        if (_binw) {
            *this || *(typename resolve_enum<T>::type*)&v;
            used = true;
        }
        else if (_binr) {
            used = read_optional(v);
            if (!used)
                v = static_cast<const T&>(defval);
        }
        else
            meta_variable_optional<T>(name, &v, true);

        return used;
    }

    ///Define a non-member variable with default value to use if missing in stream
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @param defval value to use if the variable is missing from the stream, convertible to T
    /// @param write_default if false, does not write value that equals the defval into output stream
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename D>
    bool nonmember(const token& name, T& v, const D& defval, bool write_default)
    {
        bool used = false;

        if (_binw) {
            used = write_optional(!cache_prepared() && !write_default && v == defval
                ? 0 : (typename resolve_enum<T>::type*)&v);
        }
        else if (_binr) {
            used = read_optional(v);
            if (!used)
                v = static_cast<const T&>(defval);
        }
        else
            meta_variable_optional<T>(name, &v, true);

        return used;
    }

    ///Define a fixed size array non-member variable (no valid offset within the parent struct)
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, size_t N>
    bool nonmember(const token& name, T(&v)[N], bool optional = false)
    {
        return member_fixed_size_array(name, v, optional, true);
    }

    ///Define a non-member variable referenced by a pointer (non-streamable), except const char*
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to variable to read/write to
    /// @param streamed false variable should not be streamed, just a part of meta description, can point to 1..N items (default)
    /// @param          true variable is indirectly referenced to by a pointer
    /// @return true if value was read or written and no default was used, false in meta phase
    /// @note use member_optional or member_type for special handling when the pointer has to be allocated etc.
    template<typename T, typename = std::enable_if_t<!std::is_same<const char*, T*>::value>>
    bool nonmember(const token& name, T*& v, bool streamed = false)
    {
        return nonmember_ptr(name, v, streamed);
    }

    ///Define an optional non-member variable, doesn't get read if it wasn't present in the input stream
    /// @param name variable name, used as a key in output formats
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool nonmember_optional(const token& name, T& v)
    {
        bool used = false;

        if (_binw) {
            *this || v;
            used = true;
        }
        else if (_binr) {
            used = read_optional(v);
        }
        else
            meta_variable_optional<T>(name, &v, true);

        return used;
    }

    ///Define an optional non-member variable, doesn't get read if it wasn't present in the input stream
    /// @param name variable name, used as a key in output formats
    /// @param defval default value for optional writing of values
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T, typename D>
    bool nonmember_optional(const token& name, T& v, const D& defval)
    {
        bool used = false;

        if (_binw) {
            used = write_optional(v == static_cast<const T&>(defval) ? 0 : &v);
        }
        else if (_binr) {
            used = read_optional(v);
        }
        else
            meta_variable_optional<T>(name, &v, true);

        return used;
    }

    ///Define a non-member variable referenced by a pointer, assumed to be already allocated when streaming to/from
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to variable to read/write to
    template<typename T>
    metastream& nonmember_indirect(const token& name, T*& v)
    {
        typedef typename std::remove_const<T>::type TNC;

        if (streaming()) {
            if (!v)
                throw exception() << "null pointer";

            *this || *(typename resolve_enum<TNC>::type*)v;
        }
        else
            meta_variable_indirect<TNC>(name, -1);
        return *this;
    }

    ///Define a raw pointer non-member variable to 1..N objects
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to variable to read/write to
    /// @param streamed false if variable should not be streamed, just a part of meta description
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool nonmember_ptr(const token& name, T*& v, bool streamed)
    {
        typedef typename std::remove_const<T>::type TNC;

        if (streaming()) {
            *this || *(typename resolve_enum<TNC>::type*)v;
            return true;
        }
        else
            meta_variable_raw_ptr<TNC>(name, -1, streamed);

        return false;
    }

    ///Define a non-member variable, with default value constructed by streaming the value object from nullstream
    /// @note obviously, T's metastream declaration must be made only of members with default values
    /// @param name variable name, used as a key in output formats
    /// @param v variable to read/write to
    template<typename T>
    metastream& nonmember_stream_default(const token& name, T& v)
    {
        if (streaming())
            *this || (typename resolve_enum<T>::type&)v;
        else {
            meta_variable<T>(name, &v, true);
            meta_cache_default_stream<T>(&v);
        }
        return *this;
    }


    ///Define a fixed size array member
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to the first array element
    /// @param size element count
    /// @param optional true if optional on input
    /// @param nonmember non-member variable (no valid offset within the parent struct)
    /// @return true if value was read or written and no default was used, false in meta phase
    template<size_t N, typename T>
    bool member_fixed_size_array(const token& name, T(&v)[N], bool optional, bool nonmember = false)
    {
        return member_fixed_size_array_ptr<N>(name, v, optional, nonmember);
    }

    ///Define a fixed size array member
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to the first array element
    /// @param size element count
    /// @param optional true if optional on input
    /// @param nonmember non-member variable (no valid offset within the parent struct)
    /// @return true if value was read or written and no default was used, false in meta phase
    template<size_t N, typename T>
    bool member_fixed_size_array_ptr(const token& name, T* v, bool optional, bool nonmember = false)
    {
        if (_binw) {
            binstream_container_fixed_array<T, ints> bc(v, N);
            write_container(bc);
            return true;
        }
        else if (_binr) {
            binstream_container_fixed_array<T, ints> bc(v, N);
            return read_container(bc, optional);
        }
        else
            meta_variable_fixed_array<N>(name, v, optional, nonmember);
        return false;
    }

    ///Define a dynamically sized array member
    /// @param name variable name, used as a key in output formats
    /// @param v pointer to the first array element
    /// @param n element count (when streaming), max element count when declaring, [out] real element count
    /// @param optional true if optional on input
    /// @param nonmember non-member variable (no valid offset within the parent struct)
    /// @return true if value was read or written and no default was used, false in meta phase
    template<typename T>
    bool member_dynamic_array(const token& name, T* v, uints& n, bool optional, bool nonmember = false)
    {
        if (_binw) {
            if (optional && n == 0) {
                moveto_expected_target(WRITE_MODE);
                return false;
            }
            binstream_container_fixed_array<T, ints> bc(v, n);
            write_container(bc);
            n -= bc.count();
            return true;
        }
        else if (_binr) {
            binstream_container_fixed_array<T, ints> bc(v, n);
            bool res = read_container(bc, optional);
            n -= bc.count();
            return res;
        }
        else
            meta_variable_array(name, v, n, optional, nonmember);
        return false;
    }


    ///Write value in stream_writing mode of metastream operator
    /// @param p pointer to value, nullptr if the value should not be written
    template<class T>
    bool write_optional(const T* p)
    {
        if (!p) {
            moveto_expected_target(WRITE_MODE);
            return false;
        }

        *this || *const_cast<T*>(p);
        return true;
    }

    ///Read value in stream_reading mode of metastream operator
    /// @return true if the value was in the stream
    /// @param val receives the value
    template<class T>
    bool read_optional(T& val)
    {
        opcd e = movein_process_key(READ_MODE);
        if (!e) {
            *this || *(typename resolve_enum<T>::type*)&val;
            return true;
        }
        else {
            if (cache_prepared())
                _current->offs += sizeof(uints);
            moveto_expected_target(READ_MODE);
        }

        return false;
    }

    ///Read value in stream_reading mode of metastream operator
    /// @return pointer to object created with new(), or nullptr if the object wasn't in the stream
    template<class T>
    T* read_optional()
    {
        opcd e = movein_process_key(READ_MODE);
        if (!e) {
            T* p = new T;
            *this || *p;
            return p;
        }
        else
            moveto_expected_target(READ_MODE);

        return 0;
    }

    template <class T>
    void set_next_container_default_value(const T& defval) {
        if (stream_reading()) {
            _container_default_value = &defval;
            _container_default_value_type = typeid(T).name();
        }
    }


    ///Bind the same stream to both input and output
    void bind_formatting_stream(fmtstream& bin)
    {
        _fmtstreamwr = _fmtstreamrd = &bin;
        stream_reset(0, cache_prepared());
        _sesopen = 0;
        _beseparator = false;
    }

    ///Bind formatting streams for input and output
    void bind_formatting_streams(fmtstream& brd, fmtstream& bwr)
    {
        _fmtstreamwr = &bwr;
        _fmtstreamrd = &brd;
        stream_reset(0, cache_prepared());
        _sesopen = 0;
        _beseparator = false;
    }

    fmtstream& get_reading_formatting_stream() const {
        return *_fmtstreamrd;
    }

    fmtstream& get_writing_formatting_stream() const {
        return *_fmtstreamwr;
    }

    ///Set current file name (for error reporting)
    void set_file_name(const token& name) {
        if (_fmtstreamrd)
            _fmtstreamrd->fmtstream_file_name(name);
        if (_fmtstreamwr)
            _fmtstreamwr->fmtstream_file_name(name);
    }

    metastream& _xthrow(opcd e)
    {
        if (e)
        {
            before_exception_throw();
            throw exception(e);
        }
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////
    template<class T, class C>
    struct container : binstream_container<uints>
    {
        typedef T       data_t;
        typedef binstream_container_base::fnc_stream    fnc_stream;

        C& _container;
        metastream& _m;

        container(C& container, metastream& m)
            : binstream_container<uints>(bstype::t_type<T>(), &stream, &stream)
            , _container(container)
            , _m(m)
        {
            _type = _container._type;
        }

        ///Provide a pointer to next object that should be streamed
        /// @param n number of objects to allocate the space for
        virtual const void* extract(uints n) override { return _container.extract(n); }
        virtual void* insert(uints n, const void* defval) override { return _container.insert(n, defval); }

        /// @return true if the storage is continuous in memory
        virtual bool is_continuous() const override { return _container.is_continuous(); }

        virtual uints count() const override { return _container.count(); }

    protected:

        static opcd stream(binstream& bin, void* p, binstream_container_base& bc)
        {
            container<T, C>& me = static_cast<container<T, C>&>(bc);
            try {
                me._m || *(T*)p;
            }
            catch (const exception&) {
                return ersEXCEPTION;
            }
            catch (opcd e) {
                return e;
            }
            return 0;
        }
    };

    bool read_container(binstream_container_base& c, bool optional = false)
    {
        opcd e = movein_process_key(READ_MODE);
        if (e == ersNO_MORE) {
            if (!optional)
                _xthrow(e);

            if (cache_prepared())
                _current->offs += sizeof(uints);
            moveto_expected_target(READ_MODE);
            return false;
        }
        _rvarname.reset();

        read_container_body(c);
        return true;
    }

    metastream& read_container_body(binstream_container_base& c)
    {
        uints n = UMAXS;
        type t = c._type;

        //read bgnarray
        _xthrow(data_value(&n, t.get_array_begin<uints>(), READ_MODE));

        if (n == UMAXS)
            t = c.set_array_needs_separators();

        uints count = 0;
        _xthrow(data_read_array_content(c, n, &count));

        //read endarray
        _xthrow(data_value(&count, t.get_array_end(), READ_MODE));

        return *this;
    }


    metastream& write_token(const token& tok) {
        binstream_container_fixed_array<char, uint> c((char*)tok.ptr(), tok.len());
        return write_container(c);
    }

    template<class T>
    metastream& write_container(binstream_container_base& c)
    {
        return write_container(c);
    }

    metastream& write_container(binstream_container_base& c)
    {
        _xthrow(movein_process_key(WRITE_MODE));

        uints n = c.count();
        type t = c._type;

        //write bgnarray
        _xthrow(data_value(&n, t.get_array_begin<uints>(), WRITE_MODE));

        //if container doesn't know number of items in advance, require separators
        if (n == UMAXS)
            t = c.set_array_needs_separators();

        uints count = 0;
        _xthrow(data_write_array_content(c, &count));

        _xthrow(data_value(&count, t.get_array_end(), WRITE_MODE));

        return *this;
    }


    ///Used in metastream operators for templated containers
    template<class T, class COUNT>
    metastream& meta_container_type(binstream_containerT<T, COUNT>& a, ints raw_pointer_offset = -1)
    {
        if (_binr)
            read_container(a);
        else if (_binw)
            write_container(a);
        else {
            if (meta_decl_array(typeid(T[]).name(), raw_pointer_offset, -1, false, 0, 0, 0, 0))
                *this << *(T*)0;
        }
        return *this;
    }

    ///Used in metastream operators to define primitive types
    template<class T>
    metastream& meta_base_type(const char* type_name, T& v)
    {
        if (_binr)
            data_read(&v, bstype::t_type<T>());
        else if (_binw)
            data_write(&v, bstype::t_type<T>());
        else
            meta_def_primitive<T>(type_name);

        return *this;
    }

protected:

    ////////////////////////////////////////////////////////////////////////////////
    /// @{ methods to physically stream the data utilizing the metastream

    bool prepare_type_common(bool read)
    {
        _root.desc = 0;
        _current_var = 0;

        DASSERT(read || _sesopen >= 0);      //or else pending ack on read
        DASSERT(!read || _sesopen <= 0);     //or else pending flush on write

        if (_sesopen == 0) {
            _err.reset();
            _sesopen = read ? -1 : 1;
            _curvar.kth = 0;
        }

        _beseparator = false;
        return true;
    }

    opcd prepare_type_final(const token& name, bool cache, bool read)
    {
        _dometa = true;

        _curvar.var = &_root;
        _curvar.var->varname = name;
        _curvar.var->nameless_root = name.is_empty();
        _curvar.defval = nullptr;

        if (cache)
            cache_fill_root();
        //else if( _curvar.var->nameless_root  &&  _curvar.var->is_compound() )
        //    movein_struct(read);

        return 0;
    }


    template<class T>
    opcd prepare_type(T&, const token& name, bool cache, bool read)
    {
        //reset on unnamed properties - assumed root property streaming
        stream_reset(name.is_empty(), cache);

        if (!prepare_type_common(read))  return 0;

        typename resolve_enum<T>::type* p = 0;
        *this || *p;     // build description

        return prepare_type_final(name, cache, read);
    }

    opcd prepare_named_type(const token& type, const token& name, bool cache, bool read)
    {
        //reset on unnamed properties - assumed root property streaming
        stream_reset(name.is_empty(), cache);

        if (!prepare_type_common(read))  return 0;

        if (!meta_find(type))
            return ersNOT_FOUND;

        return prepare_type_final(name, cache, read);
    }

    template<class T>
    opcd prepare_type_array(T&, uints n, const token& name, bool cache, bool read)
    {
        //reset on unnamed properties - assumed root property streaming
        stream_reset(name.is_empty(), cache);

        if (!prepare_type_common(read))  return 0;

        if (meta_decl_array(typeid(T[]).name(), -1, -1, false, 0, 0, 0, 0, n))
            *this || *(typename resolve_enum<T>::type*)0;     // build description

        return prepare_type_final(name, cache, read);
    }

public:

    ///Read object of type T from the currently bound formatting stream
    template<class T>
    opcd stream_in(T& x, const token& name = token())
    {
        opcd e;
        try {
            xstream_in(x, name);
        }
        catch (opcd ee) { e = ee; }
        catch (exception&) { e = ersEXCEPTION; }
        return e;
    }

    ///Read object of type T from the currently bound formatting stream into the cache
    template<class T>
    opcd cache_in(const token& name = token())
    {
        opcd e;
        try {
            xcache_in<T>(name);
        }
        catch (opcd ee) { e = ee; }
        catch (exception&) { e = ersEXCEPTION; }
        return e;
    }


    ///Write object of type T to the currently bound formatting stream
    template<class T>
    opcd stream_out(const T& x, const token& name = token())
    {
        return stream_or_cache_out(x, false, name);
    }

    ///Write object of type T to the cache
    template<class T>
    opcd cache_out(const T& x, const token& name = token())
    {
        return stream_or_cache_out(x, true, name);
    }

    ///Write object of type T to the currently bound formatting stream
    /// @param cache true if the object should be trapped in the cache instead of sending it out through the formatting stream
    template<class T>
    opcd stream_or_cache_out(const T& x, bool cache, const token& name = token())
    {
        opcd e;
        try {
            xstream_or_cache_out(x, cache, name);
        }
        catch (opcd ee) { e = ee; }
        catch (exception&) { e = ersEXCEPTION; }
        return e;
    }

    ///Prepare streaming of a named type
    opcd stream_out_named(const token& type, const token& name, bool cache = false)
    {
        return prepare_named_type(type, name, cache, WRITE_MODE);
    }


    ///Read array of objects of type T from the currently bound formatting stream
    template<class T, class COUNT>
    opcd stream_array_in(binstream_containerT<T, COUNT>& C, const token& name = token(), uints n = UMAXS)
    {
        opcd e;
        try {
            e = prepare_type_array(*(T*)0, n, name, false, READ_MODE);
            if (e) return e;

            _binr = true;
            read_container(C);
            _binr = false;
        }
        catch (opcd ee) { e = ee; }
        catch (exception&) { e = ersEXCEPTION; }
        return e;
    }

    ///Read array of objects of type T from the currently bound formatting stream into the cache
    template<class T>
    opcd cache_array_in(const token& name = token(), uints n = UMAXS)
    {
        opcd e;
        try {
            e = prepare_type_array(*(T*)0, n, name, true, READ_MODE);
        }
        catch (opcd ee) { e = ee; }
        catch (exception&) { e = ersEXCEPTION; }
        return e;
    }


    ///Write array of objects of type T to the currently bound formatting stream
    template<class T, class COUNT>
    opcd stream_array_out(binstream_containerT<T, COUNT>& C, const token& name = token())
    {
        return stream_or_cache_array_out(C, false, name);
    }

    ///Write array of objects of type T to the currently bound formatting stream
    template<class T, class COUNT>
    opcd cache_array_out(binstream_containerT<T, COUNT>& C, bool cache = false, const token& name = token())
    {
        return stream_or_cache_array_out(C, true, name);
    }

    ///Write array of objects of type T to the currently bound formatting stream
    /// @param cache true if the array should be trapped in the cache instead of sending it out through the formatting stream
    template<class T, class COUNT>
    opcd stream_or_cache_array_out(binstream_containerT<T, COUNT>& C, bool cache, const token& name = token())
    {
        opcd e;
        try {
            e = prepare_type_array(*(T*)0, UMAXS, name, cache, WRITE_MODE);
            if (e) return e;

            if (!cache) {
                _binw = true;
                write_container<T>(C);
                _binw = false;
            }
        }
        catch (opcd ee) { e = ee; }
        catch (exception&) { e = ersEXCEPTION; }
        return e;
    }

    ///Read container of objects of type T from the currently bound formatting stream
    template<class CONT>
    opcd stream_container_in(CONT& C, const token& name = token())
    {
        typedef typename binstream_adapter_writable<CONT>::TBinstreamContainer     BC;

        BC bc = BC(C);
        return stream_array_in(bc, name);
    }

    ///Read container of objects of type T from the currently bound formatting stream into the cache
    template<class CONT>
    opcd cache_container_in(CONT& C, const token& name = token())
    {
        typedef typename binstream_adapter_writable<CONT>::TBinstreamContainer     BC;

        BC bc = BC(C);//binstream_container_writable<CONT,BC>::create(C);
        return cache_array_in(bc, name);
    }

    ///Write container of objects of type T to the currently bound formatting stream
    template<class CONT>
    opcd stream_container_out(const CONT& C, const token& name = token())
    {
        typedef typename binstream_adapter_readable<CONT>::TBinstreamContainer     BC;

        BC bc = BC(C);//binstream_container_readable<CONT,BC>::create(C);
        return stream_array_out(bc, name);
    }

    ///Write container of objects of type T to the cache
    template<class CONT>
    opcd cache_container_out(const CONT& C, const token& name = token())
    {
        typedef typename binstream_adapter_readable<CONT>::TBinstreamContainer     BC;

        BC bc = BC(C);//binstream_container_readable<CONT,BC>::create(C);
        return cache_array_out(bc, name);
    }


    template<class T>
    void xstream_in(T& x, const token& name = token())
    {
        _xthrow(prepare_type(x, name, false, READ_MODE));

        _binr = true;
        *this || (typename resolve_enum<T>::type&)x;
        _binr = false;
    }

    template<class T>
    void xcache_in(const token& name = token())
    {
        T* t = 0;
        _xthrow(prepare_type(*t, name, true, READ_MODE));
    }


    template<class T>
    void xstream_or_cache_out(const T& x, bool cache, const token& name = token())
    {
        _xthrow(prepare_type((typename resolve_enum<T>::type&)x, name, cache, WRITE_MODE));

        if (!cache) {
            _binw = true;
            *this || (typename resolve_enum<T>::type&)x;
            _binw = false;
        }
    }

    template<class T>
    void xstream_out(T& x, const token& name = token())
    {
        xstream_or_cache_out(x, false, name);
    }

    template<class T>
    void xcache_out(T& x, const token& name = token())
    {
        xstream_or_cache_out(x, true, name);
    }


    void stream_acknowledge(bool eat = false)
    {
        DASSERT(_sesopen <= 0);
        if (_sesopen < 0) {
            _sesopen = 0;
            _beseparator = false;
            _binr = false;

            _fmtstreamrd->acknowledge(eat);
        }
    }

    void stream_flush()
    {
        DASSERT(_sesopen >= 0);
        if (_sesopen > 0) {
            _sesopen = 0;
            _beseparator = false;
            _binw = false;

            _fmtstreamwr->flush();
        }
    }

    /// @param fmts_reset reset formatting stream
    /// @param cache_open leave cache open or closed
    void stream_reset(bool fmts_reset, bool cache_open)
    {
        if (fmts_reset) {
            if (_sesopen < 0)
                _fmtstreamrd->reset_read();
            else if (_sesopen > 0)
                _fmtstreamwr->reset_write();

            _sesopen = 0;
            _beseparator = false;
        }

        _stack.reset();
        cache_reset(cache_open);

        _dometa = 0;
        _binr = _binw = 0;

        _curvar.var = 0;
        _curvar.defval = nullptr;
        _cur_variable_name.set_empty();
        _cur_variable_offset = 0;
        _rvarname.reset();

        smap_init();

        _err.reset();
    }

    /// @}

        ///Reset cache and lead it into an active or inactive state
    void cache_reset(bool open)
    {
        _cache.reset();
        //_current = _cachestack.realloc(1);
        //_current->offs = open ? 0 : UMAXS;
        if (open) {
            _current = _cachestack.realloc(1);
            _current->buf = &_cache;
            _current->base = 0;
            _current->offs = 0;
            _current->ofsz = UMAXS;
        }
        else {
            _cachestack.reset();
            _current = 0;
        }

        _cachevar = 0;
        _cacheroot = 0;
        _cacheskip = 0;
    }

    const MetaDesc::Var& get_root_var(const uchar*& cachedata) const
    {
        cachedata = _cache.ptr() + sizeof(uints);
        return _root;
    }

    const charstr& error_string() const { return _err; }

    // new streaming operators

    metastream& operator || (bstype::key& a) { return meta_base_type("bstype::key", a.k); }

    metastream& operator || (bool& a) { return meta_base_type("bool", a); }
    metastream& operator || (int8& a) { return meta_base_type("int8", a); }
    metastream& operator || (uint8& a) { return meta_base_type("uint8", a); }
    metastream& operator || (int16& a) { return meta_base_type("int16", a); }
    metastream& operator || (uint16& a) { return meta_base_type("uint16", a); }
    metastream& operator || (int32& a) { return meta_base_type("int32", a); }
    metastream& operator || (uint32& a) { return meta_base_type("uint32", a); }
    metastream& operator || (int64& a) { return meta_base_type("int64", a); }
    metastream& operator || (uint64& a) { return meta_base_type("uint64", a); }

    metastream& operator || (char& a) { return meta_base_type("char", a); }

#if defined(SYSTYPE_WIN)
    metastream& operator || (long& a) { return meta_base_type("long", a); }
    metastream& operator || (ulong& a) { return meta_base_type("ulong", a); }
#endif

    metastream& operator || (float& a) { return meta_base_type("float", a); }
    metastream& operator || (double& a) { return meta_base_type("double", a); }
    metastream& operator || (long double& a) { return meta_base_type("long double", a); }


    metastream& operator || (const char*& a)
    {
        if (_binr) {
            throw exception("unsupported");
        }
        else if (_binw) {
            write_token(a);
        }
        else {
            if (meta_decl_raw_pointer(
                typeid(char*).name(),
                true,
                (ints)&a,
                [](const void* a) -> const void* { return *static_cast<char const* const*>(a); },
                [](const void* a) -> uints { return 0; },
                [](void* a, uints& i) -> void* { return static_cast<char**>(a) + i++; },
                [](const void* a, uints& i) -> const void* { return *static_cast<char const* const*>(a) + i++; }
            ))
                meta_def_primitive<char>("char");
        }
        return *this;
    }

    template<typename T>
    metastream& operator || (T*& a)
    {
        if (_binr) {
            throw exception("unsupported");
        }
        else if (_binw) {
            write_optional(a);
        }
        else {
            if (meta_decl_raw_pointer(
                typeid(T*).name(),
                true,
                (ints)&a,
                [](const void* a) -> const void* { return *static_cast<T* const*>(a); },
                [](const void* a) -> uints { return 0; },
                [](void* a, uints& i) -> void* { return static_cast<T**>(a) + i++; },
                [](const void* a, uints& i) -> const void* { return *static_cast<T const* const*>(a) + i++; }
            ))
                *this || *(T*)0;
        }
        return *this;
    }

    metastream& operator || (timet& a) { return meta_base_type("time", a); }

    metastream& operator || (opcd& a) { return meta_base_type("opcd", a); }

    metastream& operator || (charstr& a)
    {
        if (_binr) {
            auto& dyn = a.dynarray_ref();
            dyn.reset();
            dynarray<char, uint>::dynarray_binstream_container c(dyn);
            read_container(c);

            if (dyn.size())
                *dyn.add() = 0;
        }
        else if (_binw) {
            write_token(a);
        }
        else {
            if (meta_decl_array(
                typeid(a).name(),
                (ints)&a.ptr_ref(),
                sizeof(a),
                false,
                [](const void* a) -> const void* { return static_cast<const charstr*>(a)->ptr(); },
                [](const void* a) -> uints { return static_cast<const charstr*>(a)->len(); },
                [](void* a, uints& i) -> void* { return static_cast<charstr*>(a)->appendn_uninitialized(1); },
                [](const void* a, uints& i) -> const void* { return static_cast<const charstr*>(a)->ptr() + i++; }
            ))
                meta_def_primitive<char>("char");
        }
        return *this;
    }

    metastream& operator || (token& a)
    {
        if (_binr) {
            throw exception("unsupported");
        }
        else if (_binw) {
            write_token(a);
        }
        else {
            compound_type_stream_as(a,
                [&]() {
                member("ptr", a._ptr);
                member("pte", a._pte);
            },
                [&]() {
                if (meta_decl_array(
                    "coid::token"_T,        //must be different than typeid used above
                    (ints)&a._ptr,
                    sizeof(a),
                    false,
                    [](const void* a) -> const void* { return static_cast<const token*>(a)->ptr(); },
                    [](const void* a) -> uints { return static_cast<const token*>(a)->len(); },
                    0,
                    [](const void* a, uints& i) -> const void* { return static_cast<const token*>(a)->ptr() + i++; }
                ))
                    meta_def_primitive<char>("char");
            }
            );
        }

        return *this;
    }

    metastream& operator ||(versionid& vid) {
        return meta_base_type("uint64", vid);
    }

    template<typename T>
    friend metastream& operator ||(metastream& m, typed_versionid<T>& val)
    {
        return m || static_cast<versionid&>(val);
    }

    ///Used for types that stream using a translation to a different type
    /// @param set void function(S&&) receiving object from stream
    /// @param get [const S& | S] function() returning object to stream
    template <typename T, typename FnIn, typename FnOut>
    metastream& operator_indirect(T& a, FnIn set, FnOut get) {
        using S = std::remove_const_t<std::remove_reference_t<decltype(get())>>;

        if (stream_writing()) {
            *this || const_cast<S&>(get());
        }
        else if (stream_reading()) {
            if coid_constexpr_if(std::is_same_v<token, S>) {
                //special handling for tokens that are normally not readable from stream
                charstr val;
                *this || val;
                set(token(val));
            }
            else {
                S val;
                *this || val;
                set(std::forward<S>(val));
            }
        }
        else {
            //_cur_variable_offset was set by a member method, but since this is using a temporary,
            // indicate that it should not be used
            _cur_variable_offset = -1;
            S* s = 0;
            *this || *s;
        }
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @{ meta_* functions deal with building the description tree
protected:

    MetaDesc::Var* meta_fill_parent_variable(MetaDesc* d)
    {
        //DASSERT(!_alias_mode);
        MetaDesc::Var* var;

        //remember the first descriptor, it's the root type requested for streaming
        if (!_root.desc) {
            _root.desc = d;
            var = &_root;
        }
        else {
            var = _current_var->add_child(d, _cur_variable_name, _cur_variable_offset);
        }

        _cur_variable_name.set_empty();

        return var;
    }

    MetaDesc* meta_find(const token& name)//, bool* is_plain = 0)
    {
        MetaDesc* d = _smap->find(name);
        if (!d)
            return d;

        _last_var = meta_fill_parent_variable(d);
        //if (is_plain)
        //    *is_plain = _last_var->get_type().is_plain();

        meta_exit();
        return d;
    }

    MetaDesc* meta_insert(const token& name, uints size, bool plain)
    {
        MetaDesc* md = meta_find(name);
        if (md)
            return md;

        md = _smap->create(
            name,
            plain ? type::plain_compound() : type(),
            _cur_stream_fn);

        md->type_size = size;

        _current_var = meta_fill_parent_variable(md);
        _smap->push(_current_var);

        return 0;
    }

public:

    template<class T>
    static const MetaDesc* meta_find_type() {
        return smap_tls().find(typeid(T).name());
    }

    template<class T>
    void meta_variable(const token& varname, const T* v, bool nonmember = false)
    {
        typedef typename resolve_enum<T>::type B;

        _cur_variable_name = varname;
        _cur_variable_offset = nonmember ? -1 : down_cast<int>((ints)v);
        _cur_stream_fn = &type_streamer<T>::fn;

        B* b = 0;
        *this || *b;
    }

    template<class T>
    void meta_variable_indirect(const token& varname, ints offs)
    {
        typedef typename resolve_enum<T>::type B;

        _cur_variable_name = varname;
        _cur_variable_offset = down_cast<int>(offs);
        _cur_stream_fn = &type_streamer<T>::fn;

        B* b = 0;
        *this || *b;

        _last_var->singleref = true;
    }

    template<class T>
    void meta_variable_raw_ptr(const token& varname, ints offs, bool streamed)
    {
        typedef typename resolve_enum<T>::type B;

        _cur_variable_name = varname;
        _cur_variable_offset = down_cast<int>(offs);
        _cur_stream_fn = &type_streamer<T>::fn;

        if (meta_decl_raw_pointer(
            typeid(T*).name(),
            streamed,
            0,
            [](const void* a) -> const void* { return *reinterpret_cast<T const* const*>(a); },
            0,
            [](void* a, uints& i) -> void* { return *reinterpret_cast<T**>(a) + i++; },
            [](const void* a, uints& i) -> const void* { return *reinterpret_cast<T const* const*>(a) + i++; }
        ))
            *this || *(B*)0;
    }

    template<class T, class Telem>
    void meta_variable_container(const token& varname, ints offs, ints raw_ptr_offs, bool streamed,
        MetaDesc::fn_ptr fnptr, MetaDesc::fn_count fncount, MetaDesc::fn_push fnpush, MetaDesc::fn_extract fnextract)
    {
        typedef typename resolve_enum<Telem>::type B;

        _cur_variable_name = varname;
        _cur_variable_offset = down_cast<int>(offs);
        _cur_stream_fn = &type_streamer<T>::fn;

        if (meta_decl_raw_pointer(
            typeid(T).name(),
            streamed,
            raw_ptr_offs,
            fnptr,
            fncount,
            fnpush,
            fnextract
        ))
            *this || *(B*)0;
    }

    template<class T>
    void meta_variable_optional(const token& varname, const T* v, bool nonmember = false)
    {
        meta_variable<T>(varname, v, nonmember);

        _last_var->optional = true;
    }

    template<class T>
    void meta_variable_obsolete(const token& varname)
    {
        meta_variable<T>(varname, (const T*)-1);

        _last_var->obsolete = true;
        _last_var->optional = true;
    }

    ///Define member array variable
    template<size_t N, class T>
    void meta_variable_fixed_array(const token& varname, T* v, bool optional, bool nonmember = false)
    {
        typedef typename resolve_enum<T>::type B;

        _cur_variable_name = varname;
        _cur_variable_offset = nonmember ? -1 : down_cast<int>((ints)v);
        _cur_stream_fn = &type_streamer<T>::fn;

        if (meta_decl_array(
            typeid(T[N]).name(),
            0,
            sizeof(v),
            true,
            [](const void* a) -> const void* { return static_cast<const T*>(a); },
            [](const void* a) -> uints { return N; },
            [](void* a, uints& i) -> void* { return static_cast<T*>(a) + i++; },
            [](const void* a, uints& i) -> const void* { return static_cast<T const*>(a) + i++; },
            N
        )) {
            _current_var->optional = optional;

            B* b = 0;
            *this || *b;
        }
        else {
            _current_var->optional = optional;
        }
    }

    ///Define member array variable with dynamic sizing
    /// @param n max array size
    template<class T>
    void meta_variable_array(const token& varname, T* v, uints n, bool optional, bool nonmember = false)
    {
        typedef typename resolve_enum<T>::type B;

        _cur_variable_name = varname;
        _cur_variable_offset = nonmember ? -1 : down_cast<int>((ints)v);
        _cur_stream_fn = &type_streamer<T>::fn;

        if (meta_decl_array(
            typeid(T[]).name(),
            0,
            sizeof(v),
            true,
            [](const void* a) -> const void* { return static_cast<const T*>(a); },
            [](const void* a) -> uints { return UMAXS; },
            [](void* a, uints& i) -> void* { return static_cast<T*>(a) + i++; },
            [](const void* a, uints& i) -> const void* { return static_cast<T const*>(a) + i++; },
            n
        )) {
            _current_var->optional = optional;

            B* b = 0;
            *this || *b;
        }
        else {
            _current_var->optional = optional;
        }
    }



    template<class T>
    void meta_cache_default(const T& defval)
    {
        //typedef typename std::conditional<std::is_enum<T>::value, typename EnumType<sizeof(T)>::TEnum, T>::type B;
        typedef typename resolve_enum<T>::type B;

        _curvar.var = _last_var;

        _current = _cachestack.push();
        _current->var = _curvar.var;
        _current->olddef = _cachedefval;
        _current->buf = &_curvar.var->defval;
        _current->base = 0;
        _current->offs = 0;
        _current->ofsz = UMAXS;

        //insert a dummy address field
        _current->set_addr(_current->insert_address(), sizeof(uints));

        _dometa = true;

        _binw = true;
        *this || *(B*)&defval;
        _binw = false;

        _cachedefval = _cachestack.last()->olddef;
        _cachestack.pop();
        _current = _cachestack.last();

        _dometa = 0;
        _curvar.var = 0;
    }

    ///Default value coming from the metastream operator, assumed all members have default values
    template<class T>
    void meta_cache_default_stream(const T*)
    {
        _curvar.var = _last_var;

        _current = _cachestack.push();
        _current->var = _curvar.var;
        _current->olddef = _cachedefval;
        _current->buf = &_curvar.var->defval;
        _current->base = 0;
        _current->offs = 0;
        _current->ofsz = UMAXS;

        //insert a dummy address field
        _current->set_addr(_current->insert_address(), sizeof(uints));

        _dometa = true;

        T def;

        fmtstreamnull null;
        metastream m(null);
        m.xstream_in(def);

        _binw = true;
        *this || def;
        _binw = false;

        _cachedefval = _cachestack.last()->olddef;
        _cachestack.pop();
        _current = _cachestack.last();

        _dometa = 0;
        _curvar.var = 0;
    }

    ///Signal that the primitive or compound type coming is an array
    /// @param n array element count, UMAXS if unknown or varying
    /// @param embedded true if array is embedded in parent (must be also fixed size in that case)
    MetaDesc* meta_decl_array(
        const token& type_name,
        ints raw_pointer_offset,
        ints type_size,
        bool embedded,
        MetaDesc::fn_ptr fnptr,
        MetaDesc::fn_count fncount,
        MetaDesc::fn_push fnpush,
        MetaDesc::fn_extract fnextract,
        uints n = UMAXS
    )
    {
        //embedded arrays must be static
        DASSERT(!embedded || n != UMAXS);
        DASSERT(n != 0);

        MetaDesc* d = _smap->find(type_name);

        if (!d) {
            d = _smap->create(type_name, bstype::kind(), _cur_stream_fn);
            d->array_size = n;
            d->type_size = type_size;
            d->is_array_type = true;
            d->embedded = embedded;
            d->raw_pointer_offset = down_cast<int>(raw_pointer_offset);
            d->fnptr = fnptr;
            d->fncount = fncount;
            d->fnpush = fnpush;
            d->fnextract = fnextract;
            d->streaming_type = d;

            _current_var = meta_fill_parent_variable(d);
            _smap->push(_current_var);

            return d;
        }
        else {
            DASSERT(n == d->array_size);
            DASSERT(d->type_size == (uints)type_size);
            DASSERT(d->is_array_type);

            _last_var = meta_fill_parent_variable(d);
            meta_exit();
        }

        //MetaDesc* d = _smap->create_array_desc(n, _cur_stream_fn);
        return 0;
    }

    ///Signal that the primitive or compound type coming is a container with optional raw pointer inside
    /// @param streamed false if variable should not be streamed, just a part of meta description
    MetaDesc* meta_decl_container(
        const token& type_name,
        uints type_size,
        bool streamed,
        ints raw_pointer_offset,
        MetaDesc::fn_ptr fnptr,
        MetaDesc::fn_count fncount,
        MetaDesc::fn_push fnpush,
        MetaDesc::fn_extract fnextract
    )
    {
        if (!streamed) {
            _current_var->obsolete = true;
            _current_var->optional = true;
        }

        return meta_decl_array(type_name, raw_pointer_offset, type_size, false, fnptr, fncount, fnpush, fnextract, UMAXS);
    }

    ///Signal that the primitive or compound type coming is a raw pointer
    /// @param streamed false if variable should not be streamed, just a part of meta description
    MetaDesc* meta_decl_raw_pointer(
        const token& type_name,
        bool streamed,
        ints raw_pointer_offset,
        MetaDesc::fn_ptr fnptr,
        MetaDesc::fn_count fncount,
        MetaDesc::fn_push fnpush,
        MetaDesc::fn_extract fnextract
    )
    {
        if (!streamed) {
            _current_var->obsolete = true;
            _current_var->optional = true;
        }

        MetaDesc* d = meta_decl_array(type_name, raw_pointer_offset, sizeof(void*), false, fnptr, fncount, fnpush, fnextract, UMAXS);

        if (d)
            d->is_pointer = true;

        return d;
    }

    ///Only for primitive types
    template<class T>
    MetaDesc* meta_def_primitive(const char* type_name)
    {
        type t = bstype::t_type<T>();
        DASSERT(t.is_primitive());

        MetaDesc* d = _smap->find_or_create(type_name, t, _cur_stream_fn);

        _last_var = meta_fill_parent_variable(d);
        d->streaming_type = d;

        meta_exit();
        return d;
    }

    ///Get back from multiple array decl around current type
    void meta_exit()
    {
        while (_current_var && _current_var->desc->is_array()) {
            _last_var = _smap->pop();
            _current_var = _smap->last();
        }
    }

    /// @} meta_*

    ///Get type descriptor for given type
    template<class T>
    const MetaDesc* get_type_desc()
    {
        _root.desc = 0;
        _current_var = 0;

        _cur_variable_name.set_empty();
        _cur_variable_offset = 0;

        smap_init();

        *this || *(typename resolve_enum<T>::type*)0;

        const MetaDesc* mtd = _root.desc;

        _root.desc = 0;
        return mtd;
    }

    const MetaDesc* get_type_info(const token& type) const { return _smap->find(type); }

    void get_type_info_all(dynarray<const MetaDesc*>& dst) { return _smap->get_all_types(dst); }

private:

    ////////////////////////////////////////////////////////////////////////////////
    ///Interface for holding built descriptors
    struct structure_map
    {
        MetaDesc* find(const token& k) const;

        MetaDesc* create(const token& n, type t, MetaDesc::stream_func fn)
        {
            MetaDesc d(n);
            d.btype = t;
            d.type_size = t.get_size();
            d.fnstream = fn;
            return insert(std::move(d));
        }

        MetaDesc* create_anon(const token& n, type t, MetaDesc::stream_func fn)
        {
            MetaDesc* d = insert_anon();
            d->type_name = n;
            d->btype = t;
            d->type_size = t.get_size();
            d->fnstream = fn;
            return d;
        }

        MetaDesc* find_or_create(const token& n, type t, MetaDesc::stream_func fn)
        {
            MetaDesc* d = find(n);
            return d ? d : create(n, t, fn);
        }

        void get_all_types(dynarray<const MetaDesc*>& dst) const;

        MetaDesc::Var* last() const { MetaDesc::Var** p = _stack.last();  return p ? *p : 0; }
        MetaDesc::Var* pop() { MetaDesc::Var* p;  return _stack.pop(p) ? p : 0; }

        void push(MetaDesc::Var* v) {
            _stack.push(v);
        }

        structure_map();
        ~structure_map();

    protected:
        MetaDesc* insert(MetaDesc&& v);
        MetaDesc* insert_anon();

        dynarray<MetaDesc::Var*> _stack;
        void* pimpl;
    };

    void smap_init() {
        _smap = &smap_tls();
    }

    static structure_map& smap_tls() {
        THREAD_LOCAL_SINGLETON_DEF(structure_map) _map;
        return *_map;
    }

    ////////////////////////////////////////////////////////////////////////////////

    charstr _err;

    // description parsing:
    MetaDesc::Var _root;
    MetaDesc::Var* _current_var = 0;
    MetaDesc::Var* _last_var = 0;

    const void* _container_default_value = 0;
    const char* _container_default_value_type = 0;

    token _cur_variable_name;
    MetaDesc::stream_func _cur_stream_fn;
    int _cur_variable_offset;

    int _sesopen = 0;                   //< flush(>0) or ack(<0) session currently open

    ///Entry for variable stack
    struct VarEntry {
        MetaDesc::Var* var = 0;         //< currently processed variable
        const void* defval = 0;         //< default value provided by set_next_container_default_value
        int kth = 0;                    //< its position within parent
    };

    dynarray<VarEntry> _stack;          //< stack for current variable

    VarEntry _curvar;                   //< currently processed variable (read/write)

    charstr _rvarname;                  //< name of variable that follows in the input stream
    charstr _struct_name;               //< used during the template name building step
    charstr _convbuf;

    bool _binw = false;
    bool _binr = false;
    bool _dometa = false;               //< true if shoud stream metadata, false if only the values
    bool _beseparator = false;          //< true if separator between members should be read or written

    fmtstream* _fmtstreamrd = 0;        //< bound formatting front-end binstream
    fmtstream* _fmtstreamwr = 0;        //< bound formatting front-end binstream

    dynarray<uchar> _cache;             //< cache for unordered input data or written data in the write-to-cache mode

    //workaround for M$ compiler, instantiate dynarray<uint> first so it doesn't think these are the same
    typedef dynarray<uint>              __Tdummy;

    ///Helper struct for cache traversal
    struct CacheEntry
    {
        dynarray<uchar>* buf;           //< cache buffer associated with the cache entry
        uints offs;                     //< offset to the current entry in stored class table
        uints ofsz;                     //< offset to the count field (only for arrays)
        const MetaDesc::Var* var;
        MetaDesc::Var* olddef;
        uints base;

        CacheEntry()
            : buf(0), offs(UMAXS), ofsz(UMAXS), var(0), olddef(0), base(0)
        {}

        uints size() const { return buf->size(); }

        /// @{ return ptr to data pointed to by the cache entry
        const uchar* data() const { return buf->ptr() + offs; }
        uchar* data() { return buf->ptr() + offs; }

        const uchar* data(uints o) const { return buf->ptr() + o; }
        uchar* data(uints o) { return buf->ptr() + o; }
        /// @}

        /// @{ return ptr to data pointed to indirectly by the cache entry
        const uchar* indirect() const { uints k = addr();  DASSERT(k < size());  return buf->ptr() + k; }
        uchar* indirect() { uints k = addr();  DASSERT(k < size());  return buf->ptr() + k; }
        /// @}

        ///Retrieve address (offset) stored at the current offset
        uints addr() const {
            uints v = offs + *(const uints*)(buf->ptr() + offs);
            DASSERT(v % sizeof(uints) == 0);    //should be aligned
            return v;
        }
        uints addr(uints v) const {
            DASSERT(v % sizeof(uints) == 0);
            uints r = v + *(const uints*)(buf->ptr() + v);

            DASSERT(r % sizeof(uints) == 0);
            return r;
        }

        ///Set address (offset) at the current offset
        void set_addr(uints v) {
            DASSERT(v % sizeof(uints) == 0);
            *(uints*)(buf->ptr() + offs) = v - offs;
        }
        void set_addr(uints adr, uints v) {
            DASSERT(adr % sizeof(uints) == 0);
            DASSERT(v % sizeof(uints) == 0);
            *(uints*)(buf->ptr() + adr) = v - adr;
        }
        void set_addr_invalid(uints adr) {
            DASSERT(adr % sizeof(uints) == 0);
            *(uints*)(buf->ptr() + adr) = 0;
        }

        bool valid_addr() const { return offs != UMAXS && 0 != *(const uints*)(buf->ptr() + offs); }
        bool valid_addr(uints adr) const { return 0 != *(const uints*)(buf->ptr() + adr); }

        ///Extract offset-containing field, moving to the next entry
        uints extract_offset()
        {
            uints v = addr();
            DASSERT(v % sizeof(uints) == 0);    //should be aligned

            offs += sizeof(uints);
            return v;
        }

        void insert_offset(uints v)
        {
            DASSERT(v % sizeof(uints) == 0);    //should be aligned

            set_addr(v);
            offs += sizeof(uints);
        }

        uints next_offset() {
            if (offs != UMAXS)
                offs += sizeof(uints);
            return offs;
        }

        uints get_asize() const { return *(const uints*)data(ofsz); }
        void set_asize(uints n) { *(uints*)data(ofsz) = n; }

        ///Extract size-containing field, moving to the next entry
        void extract_asize_field()
        {
            ofsz = offs;
            offs += sizeof(uints);
        }

        ///Insert size-containing field
        uints* insert_asize_field()
        {
            ofsz = insert_void(sizeof(uints));
            return (uints*)data(ofsz);
        }

        uints insert_table(uints n)
        {
            uints k = buf->size();
            DASSERT(k % sizeof(uints) == 0);    //should be padded

            buf->addc(n * sizeof(uints));
            return k;
        }

        ///Retrieve value stored at given address
        template <class T>
        const T& extract(uints o) const { return *(const T*)(buf->ptr() + o); }

        ///Pad cache to specified granularity (but not greater than 8 bytes)
        uints pad(uints size = sizeof(uints))
        {
            uints k = buf->size();
            uints t = align_value_up(k, size > 8 ? 8 : size);
            buf->add(t - k);
            return t;
        }

        ///Allocate space in the buffer, aligning the buffer position according to the \a size
        /// @return position where inserted
        uints insert_void(uints size)
        {
            uints k = pad();

            buf->add(size);
            return k;
        }

        void* insert_void_padded(uints size)
        {
            pad(size);
            return buf->add(size);
        }

        void* insert_void_unpadded(uints size)
        {
            return buf->add(size);
        }

        ///Append a field that will contain an address (offset)
        /// @note expects padded data
        uints insert_address()
        {
            uints k = buf->size();
            DASSERT(k % sizeof(uints) == 0);    //should be padded

            buf->addc(sizeof(uints), true);
            return k;
        }

        ///Prepare for insertion of a member entry
        void insert_member() { insert_offset(pad()); }

        ///Prepare for extraction of a member entry
        uints extract_member() { return extract_offset(); }

        ///Read cache entry data, moving to the next cache entry
        void read_cache(void* p, uints size)
        {
            const uchar* src = indirect();
            offs += sizeof(uints);

            ::memcpy(p, src, size);
        }

        ///Allocate cache entry data, moving to the next cache entry
        void* alloc_cache(uints size)
        {
            uints of = insert_void(size);
            set_addr(of);
            offs += sizeof(uints);
            return data(of);
        }
    };

    dynarray<CacheEntry>
        _cachestack;                    //< cache table stack
    CacheEntry* _current = 0;           //< currently processed cache entry

    MetaDesc::Var* _cacheroot = 0;      //< root of the cache, whose members are going to be cached
    MetaDesc::Var* _cachequit = 0;      //< member variable being read from cache
    MetaDesc::Var* _cachedefval = 0;    //< variable whose default value is currently being read
    MetaDesc::Var* _cachevar = 0;       //< variable being currently cached from input
    MetaDesc::Var* _cacheskip = 0;      //< set if the variable was not present in input (can be filled with default) or has been already cached

    structure_map* _smap = 0;

private:

    ////////////////////////////////////////////////////////////////////////////////
    MetaDesc::Var* last_var() const { return _stack.last()->var; }
    void pop_var() {
        bool has = _stack.pop(_curvar);
        DASSERT(has);
        DASSERT(_curvar.var);
    }

    void push_var(bool read) {
        _stack.push(_curvar);
        _curvar.var = _curvar.var->desc->first_child(read);
        _curvar.kth = 0;
        _curvar.defval = nullptr;
    }

    MetaDesc::Var* parent_var() const
    {
        VarEntry* v = _stack.last();
        return v ? v->var : 0;
    }

    bool is_first_var() const { return _curvar.kth == 0; }


    charstr& dump_stack(charstr& dst, int depth, MetaDesc::Var* var = 0) const
    {
        if (!dst.is_empty())
            dst << char('\n');

        if (depth <= 0)
            depth = (int)_stack.size() + depth;

        if (depth > (int)_stack.size())
            depth = (int)_stack.size();

        for (int i = 0; i < depth; ++i)
        {
            //dst << char('/');
            _stack[i].var->dump(dst);
        }

        if (var) {
            //dst << char('/');
            var->dump(dst);
        }
        return dst;
    }

    void before_exception_throw()
    {
        _fmtstreamrd->fmtstream_err(_err, true);
        _fmtstreamrd->on_exception_throw();
    }

    void warn_obsolete(const token& name);

    ////////////////////////////////////////////////////////////////////////////////
protected:

    static const bool READ_MODE = true;
    static const bool WRITE_MODE = false;

    void movein_cache_member(bool read)
    {
        CacheEntry* ce = _cachestack.push();
        _current = ce - 1;

        ce->var = _curvar.var;

        bool cachewrite = !read || _cachevar;

        //_current->offs points to the actual offset to member data if we are nested in another struct
        // or it is the offset to member data itself if we are in array
        if (_curvar.var->is_array_element())
        {
            ce->buf = _current->buf;
            ce->offs = cachewrite ? _current->pad() : _current->offs;
        }
        else if (cachewrite)
        {
            _current->insert_member();

            ce->buf = _current->buf;
            ce->offs = _current->pad();
        }
        else
        {
            //check if the member was cached or use the default value cache instead
            if (!_current->valid_addr()) {
                DASSERT(_curvar.var->has_default());
                _current->extract_member(); //move offset in parent to next member

                ce->buf = &_curvar.var->defval;
                ce->offs = sizeof(uints);
            }
            else {
                uints v = _current->extract_member();   //also moves offset to next

                ce->buf = _current->buf;
                ce->offs = v;

                DASSERT(ce->offs <= ce->size());
            }
        }

        _current = ce;
    }

    opcd movein_struct(bool read)
    {
        bool cache = cache_prepared();

        if (!cache || _cachevar)
        {
            bool nameless = _curvar.var->nameless_root;
            opcd e = read
                ? _fmtstreamrd->read_struct_open(nameless, &_curvar.var->desc->type_name)
                : _fmtstreamwr->write_struct_open(nameless, &_curvar.var->desc->type_name);

            if (e) {
                dump_stack(_err, 0);
                _err << " - error " << (read ? "reading" : "writing") << " struct opening token\n";
                if (read)
                    before_exception_throw();
                throw exception(_err);
                return e;
            }
        }

        if (cache)
        {
            movein_cache_member(read);

            //append member offset table
            if (!read || _cachevar)
                _current->insert_table(_curvar.var->desc->num_children());
        }

        if (read) _rvarname.reset();
        push_var(read);

        return 0;
    }

    opcd moveout_struct(bool read)
    {
        bool cache = cache_prepared();

        pop_var();
        if (read) _rvarname.reset();

        if (_current && _current->var == _curvar.var)
        {
            if (_cachestack.last()->olddef)
                _cachedefval = _cachestack.last()->olddef;
            _current = _cachestack.pop();

            if (_curvar.var == _cacheroot) {
                //DASSERT( _current == 0 );
                _cacheroot = 0;
                cache = false;
            }
        }


        if (!cache || _cachevar)
        {
            bool nameless = _curvar.var->nameless_root;
            opcd e = read
                ? _fmtstreamrd->read_struct_close(nameless, &_curvar.var->desc->type_name)
                : _fmtstreamwr->write_struct_close(nameless, &_curvar.var->desc->type_name);

            if (e) {
                dump_stack(_err, 0);
                _err << " - error " << (read ? "reading" : "writing") << " struct closing token";
                if (read)
                    before_exception_throw();
                throw exception(_err);
                return e;
            }
        }

        //if(cache)
        //    _current = _cachestack.pop();

        return moveto_expected_target(read);
    }


    opcd movein_process_key(bool read)
    {
        if (_container_default_value) {
            _curvar.defval = _container_default_value;
            _container_default_value = nullptr;
            _container_default_value_type = nullptr;
        }

        if (!_rvarname.is_empty()) {
            DASSERT(_rvarname == _curvar.var->varname);
            //_rvarname.reset();
            return 0;
        }
        else if (_curvar.var->nameless_root || _curvar.var->is_array_element())
            return 0;

        return read
            ? fmts_or_cache_read_key()
            : fmts_or_cache_write_key();
    }

    ///Traverse the tree and set up the next target for input/output streaming
    opcd moveto_expected_target(bool read)
    {
        DASSERT(!_container_default_value);
        _container_default_value = nullptr;

        //get next var
        MetaDesc::Var* par = parent_var();
        if (!par) {
            //_dometa = 0;
            return 0;
        }

        MetaDesc::Var* next = par->stream_desc()->next_child(_curvar.var, read);

        //find what should come next
        if (_curvar.var == _cachevar) {
            //end caching - _cachevar was cached completely
            _current->offs = UMAXS;
            return 0;
        }
        else if (_current)
        {
            if (_curvar.var == _cachedefval) {
                _cachedefval = _cachestack.last()->olddef;
                _current = _cachestack.pop();
                if (_current) _current->next_offset();
            }

            if (_curvar.var == _cachequit)
                invalidate_cache_entry();
            /*
            if( !next && par == _cacheroot ) {
                _current = _cachestack.pop();
                DASSERT( _current == 0 );
                _cacheroot = 0;
            }*/
        }

        _curvar.var = next;
        _curvar.defval = nullptr;

        if (read)
            _rvarname.reset();

        return 0;
    }


    ///This is called from internal binstream when a primitive data or control token is
    /// written. Possible type can be a primitive one, T_STRUCTBGN or T_STRUCTEND,
    /// or array cotrol tokens
    opcd data_write(const void* p, type t)
    {
        if (!_dometa)
            return data_write_nometa(p, t);

        if (!t.is_array_end()) {
            opcd e = movein_process_key(WRITE_MODE);
            if (e) return e;
        }

        return data_value((void*)p, t, WRITE_MODE);
    }

    ///This is called from internal binstream when a primitive data or control token is read.
    opcd data_read(void* p, type t)
    {
        if (!_dometa)
            return data_read_nometa(p, t);

        if (!t.is_array_end()) {
            opcd e = movein_process_key(READ_MODE);
            if (e) return e;
        }

        return data_value(p, t, READ_MODE);
    }

    ///
    opcd data_value(void* p, type t, bool read)
    {
        //read value
        opcd e = fmts_or_cache(p, t, read);
        if (e) {
            dump_stack(_err, 0);
            _err << " - error " << (read ? "reading" : "writing") << " variable '" << _curvar.var->varname << "', error: " << opcd_formatter(e);
            before_exception_throw();
            throw exception(_err);
            return e;
        }

        if (t.is_array_start())
            return 0;

        return moveto_expected_target(read);
    }

    ///
    opcd data_write_nometa(const void* p, type t)
    {
        if (!t.is_array_end() && _beseparator) {
            opcd e = _fmtstreamwr->write_separator();
            if (e) return e;
        }
        else
            _sesopen = 1;

        _beseparator = !t.is_array_start();

        return _fmtstreamwr->write(p, t);
    }

    ///
    opcd data_read_nometa(void* p, type t)
    {
        if (!t.is_array_end() && _beseparator)
        {
            opcd e = _fmtstreamrd->read_separator();
            if (e) {
                dump_stack(_err, 0);
                _err << " - error reading separator: " << opcd_formatter(e);
                before_exception_throw();
                throw exception(_err);
                return e;
            }
        }
        else
            _sesopen = -1;

        _beseparator = !t.is_array_start();

        return _fmtstreamrd->read(p, t);
    }

    opcd data_write_raw(const void* p, uints& len)
    {
        return _fmtstreamwr->write_raw(p, len);
    }

    opcd data_read_raw(void* p, uints& len)
    {
        if (_cachevar)
            p = _current->data(_current->insert_void(len));

        return _fmtstreamrd->read_raw(p, len);
    }

    opcd data_read_raw_full(void* p, uints& len)
    {
        for (;;) {
            uints olen = len;
            opcd e = data_read_raw(p, len);
            if (e != ersRETRY) return e;

            p = (char*)p + (olen - len);
        }
    }


    opcd data_write_array_content(binstream_container_base& c, uints* count)
    {
        c.set_array_needs_separators();
        type tae = c._type.get_array_element();
        uints n = c.count();

        opcd e = 0;
        if (!tae.is_primitive())
        {
            if (cache_prepared())  //cached compound array
            {
                //write to cache
                DASSERT(!_curvar.var->stream_desc()->is_primitive());

                uints prevoff = UMAXS, i;
                for (i = 0; i < n; ++i)
                {
                    const void* p = c.extract(1);
                    if (!p)
                        break;

                    //compound objects stored in an array are prefixed with offset past their body
                    // off will thus contain offset to the next element
                    uints off = _current->pad();
                    if (i > 0)
                        _current->set_addr(prevoff, off);
                    prevoff = _current->insert_address();

                    push_var(false);

                    c.stream_out(this, const_cast<void*>(p));

                    pop_var();
                }

                if (i > 0) {
                    uints off = _current->pad();
                    _current->set_addr(prevoff, off);
                }

                *count = i;
            }
            else                    //uncached compound array
                e = data_write_compound_array_content(c, count);
        }
        else if (cache_prepared()) //cache with a primitive array
        {
            if (!_cachevar && c.is_continuous() && n != UMAXS)
            {
                uints na = n * tae.get_size();
                xmemcpy(_current->insert_void_unpadded(na), c.extract(n), na);

                _current->offs += na;
                *count = n;
            }
            else
            {
                type t = c._type;
                uints n = c.count();

                if (t.is_primitive() && c.is_continuous() && n != UMAXS)
                {
                    DASSERT(!t.is_no_size());

                    uints na = n * t.get_size();
                    e = data_write_raw(c.extract(n), na);

                    if (!e)  *count = n;
                }
                else
                    e = data_write_compound_array_content(c, count);
            }
        }
        else
            e = _fmtstreamwr->write_array_content(c, count, this);

        return e;
    }

    opcd data_write_compound_array_content(binstream_container_base& c, uints* count)
    {
        type tae = c._type.get_array_element();
        uints n = c.count(), k = 0;
        bool complextype = !c._type.is_primitive();
        bool needpeek = c.array_needs_separators();

        opcd e;
        while (n > 0)
        {
            --n;

            const void* p = c.extract(1);
            if (!p)
                break;

            if (needpeek && (e = data_write_array_separator(tae, 0)))
                return e;

            push_var(false);

            if (complextype)
                e = c.stream_out(this, const_cast<void*>(p));
            else
                e = data_write(p, tae);

            pop_var();

            if (e)
                return e;
            ++k;

            type::mask_array_element_first_flag(tae);
        }

        if (needpeek)
            e = data_write_array_separator(tae, 1);

        if (!e)
            *count = k;

        return e;
    }

    opcd data_read_array_content(binstream_container_base& c, uints n, uints* count)
    {
        c.set_array_needs_separators();
        type tae = c._type.get_array_element();

        opcd e = 0;
        if (!tae.is_primitive())     //handles arrays of compound objects
        {
            if (cache_prepared())  //cached compound array
            {
                //reading from cache
                DASSERT(_cachevar || n != UMAXS);
                DASSERT(!_curvar.var->stream_desc()->is_primitive());

                uints i, prevoff = UMAXS;
                for (i = 0; i < n; ++i)
                {
                    if (_cachevar) {
                        if (ersNO_MORE == data_read_array_separator(tae))
                            break;
                        type::mask_array_element_first_flag(tae);
                    }

                    void* p = c.insert(1, _curvar.defval);
                    if (!p)
                        return ersNOT_ENOUGH_MEM;

                    //compound objects stored in an array are prefixed with offset past their body
                    // off will thus contain offset to the next element
                    uints off;
                    if (_cachevar) {
                        off = _current->pad();
                        if (i > 0)
                            _current->set_addr(prevoff, off);
                        prevoff = _current->insert_address();
                    }
                    else
                        off = _current->extract_offset();

                    push_var(true);

                    c.stream_in(this, p);

                    pop_var();

                    //set offset to the next array element
                    if (!_cachevar)
                        _current->offs = off;
                }

                if (_cachevar && i > 0) {
                    uints off = _current->pad();
                    _current->set_addr(prevoff, off);
                }

                *count = i;
            }
            else {
                //uncached compound array
                e = data_read_compound_array_content(c, n, count);
            }
        }
        else if (cache_prepared()) //cache with a primitive array
        {
            if (!_cachevar && c.is_continuous() && n != UMAXS)
            {
                uints na = n * tae.get_size();
                xmemcpy(c.insert(n, _curvar.defval), _current->data(), na);

                _current->offs += na;
                *count = n;
            }
            else
            {
                type t = c._type;

                if (t.is_primitive() && c.is_continuous() && n != UMAXS)
                {
                    DASSERT(!t.is_no_size());

                    uints na = n * t.get_size();
                    e = data_read_raw_full(c.insert(n, _curvar.defval), na);

                    if (!e)  *count = n;
                }
                else
                    e = data_read_compound_array_content(c, n, count);
            }
        }
        else                        //primitive uncached array
            e = _fmtstreamrd->read_array_content(c, n, count, this);

        return e;
    }

    opcd data_read_compound_array_content(binstream_container_base& c, uints n, uints* count)
    {
        type tae = c._type.get_array_element();
        bool complextype = !c._type.is_primitive();
        bool needpeek = c.array_needs_separators();
        uints k = 0;
        const void* defitemval = _curvar.defval;

        opcd e;
        while (n > 0)
        {
            --n;

            //peek if there's an element to read
            if (needpeek && (e = data_read_array_separator(tae)))
                break;

            void* p = c.insert(1, defitemval);
            if (!p)
                return ersNOT_ENOUGH_MEM;

            push_var(true);

            if (complextype)
                e = c.stream_in(this, p);
            else
                e = data_value(p, tae, READ_MODE);

            pop_var();

            if (e)
                return e;
            ++k;

            type::mask_array_element_first_flag(tae);
        }

        *count = k;
        return 0;
    }

    ///
    opcd data_write_array_separator(type t, uchar end)
    {
        if (!_dometa)
            _beseparator = false;

        write_array_separator(t, end);

        //if(!end)
        //    push_var();

        return 0;
    }

    ///Called from binstream to read array separator or detect an array end
    ///This method is called only for uncached data; reads from the cache
    /// use the data_read_array_content() method.
    opcd data_read_array_separator(type t)
    {
        if (!_dometa)
            _beseparator = false;

        if (!read_array_separator(t))
            return ersNO_MORE;

        //push_var();
        return 0;
    }

    ///
    void write_array_separator(type t, uchar end)
    {
        if (cache_prepared())
            return;

        opcd e = _fmtstreamwr->write_array_separator(t, end);

        if (e) {
            dump_stack(_err, 0);
            _err << " - error writing array separator: " << opcd_formatter(e);
            throw exception(_err);
        }
    }

    /// @return false if no more elements
    bool read_array_separator(type t)
    {
        if (cache_prepared() && !_cachevar)
            return true;

        opcd e = _fmtstreamrd->read_array_separator(t);
        if (e == ersNO_MORE)
            return false;

        if (e) {
            dump_stack(_err, 0);
            _err << " - error reading array separator: " << opcd_formatter(e);
            before_exception_throw();
            throw exception(_err);
        }

        return true;
    }


    bool cache_prepared() const {
        return _current != 0 && _current->offs != UMAXS;
    }


    void invalidate_cache_entry()
    {
        MetaDesc* desc = _stack.last()->var->desc;

        uints k = desc->get_child_pos(_cachequit) * sizeof(uints) + _current->base;
        DASSERT(k != UMAXS && _current->valid_addr(k));

        _current->set_addr_invalid(k);
        _current->offs = UMAXS;
        _cachequit = 0;
    }


    ///Read data from formatstream or cache
    opcd fmts_or_cache(void* p, bstype::kind t, bool read)
    {
        opcd e = 0;
        if (cache_prepared())
        {
            if (read && !t.is_array_end() && !_cachevar && !_curvar.var->is_array_element() && !_current->valid_addr()) {
                //cache is open for reading but the member is not there
                //this can happen when reading a struct that was cached due to reordered input
                if (!cache_use_default()) {
                    dump_stack(_err, 0);
                    _err << " - variable '" << _curvar.var->varname << "' not found and no default value provided";
                    before_exception_throw();
                    throw exception(_err);
                }
            }

            if (t.is_array_start())
            {
                movein_cache_member(read);

                if (!read || _cachevar) {
                    *_current->insert_asize_field() = UMAXS;

                    if (_cachevar)
                        e = _fmtstreamrd->read(p, t);
                }
                else {
                    _current->extract_asize_field();

                    uints n = _current->get_asize();
                    DASSERT(n != UMAXS);

                    t.set_count(n, p);
                }
            }
            else if (t.is_array_end())
            {
                if (!read || _cachevar) {
                    if (_cachevar)
                        e = _fmtstreamrd->read(p, t);

                    uints n = t.get_count(p);
                    DASSERT(n != UMAXS);

                    _current->set_asize(n);
                }
                else {
                    if (_current->get_asize() != t.get_count(p))
                        return ersMISMATCHED "elements left in cached array";
                }

                if (_cachestack.last()->olddef)
                    _cachedefval = _cachestack.last()->olddef;
                _current = _cachestack.pop();
            }
            else    //data reads
            {
                //only primitive data here
                DASSERT(t.is_primitive());
                DASSERT(t.type != type::T_STRUCTBGN && t.type != type::T_STRUCTEND);
                DASSERT(_cachevar || !_curvar.var->is_array_element());

                uints tsize = t.get_size();

                if (read && !_cachevar)
                    _current->read_cache(p, tsize);
                else {
                    void* dst = !_curvar.var->is_array_element()
                        ? _current->alloc_cache(tsize)
                        : _current->insert_void_padded(tsize);

                    if (read && _cachevar)
                        e = _fmtstreamrd->read(dst, t);
                    else
                        ::memcpy(dst, p, tsize);
                }
            }
        }
        else
            e = read ? _fmtstreamrd->read(p, t) : _fmtstreamwr->write(p, t);

        return e;
    }

    ///Read key from input
    opcd fmts_read_key()
    {
        opcd e = _fmtstreamrd->read_key(_rvarname, _curvar.kth, _curvar.var->varname);

        ++_curvar.kth;

        if (e == ersNO_MORE) {
            //if reading to cache, make it skip reading the variable
            // error will be dealt with later, or a default value will be used (if existing)
            if (_cachevar)
                _cacheskip = _curvar.var;
            //if normal reading (not a reading to cache), set up defval read or fail
            else if (!cache_use_default() && !_curvar.var->optional) {
                dump_stack(_err, 0);
                _err << " - variable '" << _curvar.var->varname << "' not found and no default value provided";
                before_exception_throw();
                throw exception(_err);
            }
        }
        else if (e) {
            dump_stack(_err, 0);
            _err << " - error while seeking for variable '" << _curvar.var->varname << "': " << opcd_formatter(e);
            before_exception_throw();
            throw exception(_err);
        }

        return e;
    }

    ///
    /// @return ersNO_MORE if no more keys under current compound variable parent
    opcd fmts_or_cache_read_key()
    {
        //if reading to cache, and if it's been already cached
        if (_cachevar) {
            if (_current->valid_addr()) {
                _cacheskip = _curvar.var;
                return 0;
            }
        }
        //already reading from the cache or the required key has been found in the cache
        else if (cache_prepared() || cache_lookup())
            return !_curvar.var->optional || _current->valid_addr() ? 0 : ersNO_MORE;

        opcd e;
        bool outoforder = false;

        do {
            e = fmts_read_key();
            if (e) {
                //no more members under current compound
                DASSERT(e == ersNO_MORE);
                if (!_curvar.var->optional)
                    e = 0;
                break;
            }

            DASSERT(!_curvar.var->is_array_element());

            //cache the next member if the key read belongs to an out of the order sibling
            outoforder = _rvarname != _curvar.var->varname;
            if (outoforder)
                cache_fill_member();

            //_rvarname.reset();
        }
        while (outoforder);

        return e;
    }

    ///
    opcd fmts_or_cache_write_key()
    {
        if (cache_prepared())
            return 0;

        opcd e = _fmtstreamwr->write_key(_curvar.var->varname, _curvar.kth);
        if (e) {
            dump_stack(_err, 0);
            _err << " - error while writing the variable name '" << _curvar.var->varname << "': " << opcd_formatter(e);
            throw exception(_err);
        }

        ++_curvar.kth;

        return 0;
    }


    ///Fill intermediate cache, _rvarname contains the key read, and
    /// _curvar.var->varname the key requested
    void cache_fill_member()
    {
        opcd e;
        //here _rvarname can only be another member of _curvar.var's parent
        // we should find it and cache its whole structure, as well as any other
        // members until the one pointed to by _curvar.var is found
        MetaDesc::Var* par = parent_var();

        if (!par) {
            //if there is no parent, that means this was attempt to read the top level
            // member itself
            //there is no point in caching current member since it's not defined
            // and thus an error
            dump_stack(_err, 0);
            _err << " - expected variable: " << _curvar.var->varname;
            before_exception_throw();
            e = ersNOT_FOUND "no such variable";
            throw exception(_err);
        }


        uints base;
        if (_cachestack.size() > 0 && _cachestack.last()->var == par) { //_cache.size() > 0 ) {
            //compute base offset
            base = _current->offs == UMAXS
                ? _current->base
                : _current->offs - par->desc->get_child_pos(_curvar.var) * sizeof(uints);   //restore from cur var

            if (_cacheroot == 0) //cache opened in advance
                _cacheroot = par;
        }
        else {
            //create child offset table for the members of par variable
            bool newroot = _cacheroot == 0;
            if (newroot) {
                _cacheroot = par;
                _cache.reset();
            }

            _current = _cachestack.push();
            _current->var = par;
            _current->buf = &_cache;

            base = _current->base = _current->pad();

            _current->offs = UMAXS;//newroot ? UMAXS : base;
            _current->ofsz = UMAXS;

            _current->insert_table(par->desc->num_children());
        }

        MetaDesc::Var* crv = par->desc->find_child(_rvarname);
        if (!crv) {
            dump_stack(_err, -1);
            _err << " - member variable: " << _rvarname << " not defined";
            before_exception_throw();
            e = ersNOT_FOUND "no such member variable";
            throw exception(_err);
            //return e;
        }

        uints k = par->desc->get_child_pos(crv) * sizeof(uints);
        if (_current->valid_addr(base + k)) {
            dump_stack(_err, -1);
            _err << " - data for member: " << _rvarname << " specified more than once";
            before_exception_throw();
            e = ersMISMATCHED "redundant member data";
            throw exception(_err);
            //return e;
        }

        cache_fill(crv, base + k);
    }

    ///Fill intermediate cache
    void cache_fill_root()
    {
        DASSERT(_current && _current->buf->size() == 0);
        _cacheroot = &_root;

        _current->insert_table(1);
        cache_fill(&_root, 0);
    }

    ///
    opcd cache_fill(MetaDesc::Var* crv, uints offs)
    {
        MetaDesc::Var* old_var = _curvar.var;
        MetaDesc::Var* old_cvar = _cachevar;
        uints old_offs = _current->offs;
        const void* old_defval = _curvar.defval;

        _current->offs = offs;

        //force reading to cache
        _cachevar = _curvar.var = crv;

        opcd e = streamvar(*crv);

        _current->offs = old_offs;
        _cachevar = old_cvar;
        _curvar.var = old_var;
        _curvar.defval = old_defval;

        return e;
    }


    ///
    struct cache_container : public binstream_container<uints>
    {
        const void* extract(uints n) override { return 0; }

        //just fake it as the memory wouldn't be used anyway but it can't be NULL
        void* insert(uints n, const void* defval) override { return (void*)1; }

        bool is_continuous() const override { return true; }    //for primitive types it's continuous

        uints count() const override { return desc.array_size; }


        cache_container(metastream& meta, MetaDesc& desc)
            : binstream_container<uints>(desc.array_type(), 0, &stream_in)
            , meta(meta)
            , desc(desc)
        {}

        static void stream_in(metastream* m, void* p, binstream_container_base* co) {
            cache_container* me = static_cast<cache_container*>(co);
            m->streamvar(me->desc.children[0]);
        }

    protected:
        metastream& meta;
        const MetaDesc& desc;
    };
    /*
    static void stream_element(metastream* m, void*)
    {
        m->streamvar(*m->_curvar.var);
    }*/

    ///
    opcd streamvar(const MetaDesc::Var& var)
    {
        if (!_curvar.var->nameless_root && !_curvar.var->is_array_element())
            movein_process_key(READ_MODE);

        //set if reading to cache, when the current variable has been already read or its default value will be used
        if (_cacheskip) {
            _cacheskip = 0;
            return moveto_expected_target(READ_MODE);
        }

        MetaDesc& desc = *var.stream_desc();

        if (desc.is_primitive()) {
            return data_value(0, desc.btype, READ_MODE);
        }
        else if (desc.is_array()) {
            cache_container cc(*this, desc);
            read_container_body(cc);//, stream_element);
            return 0;
        }

        movein_struct(READ_MODE);

        uints offs = _current->offs;
        uints n = desc.children.size();
        for (uints i = 0; i < n; ++i)
        {
            _curvar.var = &desc.children[i];
            streamvar(*_curvar.var);

            offs += sizeof(uints);
            _current->offs = offs;
        }

        moveout_struct(READ_MODE);

        return 0;
    }


    ///Find _curvar.var->varname in cache
    bool cache_lookup()
    {
        if (_cachestack.size() == 0)
            return false;

        MetaDesc::Var* parvar = parent_var();
        if (_current->var != parvar)
            return false;

        //get child map
        MetaDesc* par = parvar->desc;

        uints k = par->get_child_pos(_curvar.var) * sizeof(uints) + _current->base;
        if (_current->valid_addr(k))
        {
            //found in the cache, set up a cache read
            _current->offs = k;
            _current->ofsz = UMAXS;
            _current->buf = &_cache;
            _cachequit = _curvar.var;

            return true;
        }

        return false;
    }

    ///Setup usage of the default value for reading
    bool cache_use_default()
    {
        if (!_curvar.var->has_default())  return false;

        _current = _cachestack.push();
        _current->var = _curvar.var;
        _current->olddef = _cachedefval;
        _current->base = 0;
        _current->offs = 0;
        _current->buf = &_curvar.var->defval;
        _cachedefval = _curvar.var;

        return true;
    }
};

////////////////////////////////////////////////////////////////////////////////
///Helper class for conversion of default values
template<class T>
struct type_holder {
    const T* operator = (const T& val) const {
        return &val;
    }

    const T* operator * () const {
        return (const T*)0;
    }
};

template<class T>
inline type_holder<T> get_type_holder(T*) {
    return type_holder<T>();
}

////////////////////////////////////////////////////////////////////////////////

template<class T>
void type_streamer<T>::fn(metastream* m, void* p, binstream_container_base*) {
    *m || *static_cast<typename resolve_enum<T>::type*>(p);
}

////////////////////////////////////////////////////////////////////////////////

///TODO: move to dynarray.h:
template <class T, class COUNT, class A>
metastream& operator << (metastream& m, const dynarray<T, COUNT, A>& a)
{
    if (m.meta_decl_array(
        typeid(a).name(),
        (ints)&a.ptr_ref(),
        sizeof(a),
        false,
        [](const void* p) -> void* { return static_cast<const dynarray<T, COUNT, A>*>(p)->ptr(); },
        [](const void* p) -> uints { return static_cast<const dynarray<T, COUNT, A>*>(p)->count(); },
        [](void* p, uints&) -> void* { return static_cast<dynarray<T, COUNT, A>*>(p)->add(); },
        [](const void* p, uints& i) -> const void* { return static_cast<const dynarray<T, COUNT, A>*>(p)->ptr() + i++; }
    ))
        m << *(T*)0;
    return m;
}


template <class T, class COUNT, class A>
metastream& operator || (metastream& m, dynarray<T, COUNT, A>& a)
{
    if (m.stream_reading()) {
        a.reset();
        typename dynarray<T, COUNT, A>::dynarray_binstream_container c(a);
        m.read_container(c);
    }
    else if (m.stream_writing()) {
        typename dynarray<T, COUNT, A>::dynarray_binstream_container c(a);
        m.write_container(c);
    }
    else {
        if (m.meta_decl_array(
            typeid(a).name(),
            (ints)&a.ptr_ref(),
            sizeof(a),
            false,
            [](const void* p) -> const void* { return static_cast<const dynarray<T, COUNT, A>*>(p)->ptr(); },
            [](const void* p) -> uints { return static_cast<const dynarray<T, COUNT, A>*>(p)->size(); },
            [](void* p, uints&) -> void* { return static_cast<dynarray<T, COUNT, A>*>(p)->add(); },
            [](const void* p, uints& i) -> const void* { return static_cast<const dynarray<T, COUNT, A>*>(p)->ptr() + i++; }
        )) {
            T* v = 0;
            m || *v;
        }
    }
    return m;
}

template <class T>
metastream& operator || (metastream& m, range<T>& a)
{
    if (m.stream_reading()) {
        a.reset();
        typename range<T>::range_binstream_container c(a);
        m.read_container(c);
    }
    else if (m.stream_writing()) {
        typename range<T>::range_binstream_container c(a);
        m.write_container(c);
    }
    else {
        if (m.meta_decl_array(
            typeid(a).name(),
            -1,
            sizeof(a),
            false,
            [](const void* a) -> const void* { return static_cast<const range<T>*>(a)->ptr(); },
            [](const void* a) -> uints { return static_cast<const range<T>*>(a)->size(); },
            0,
            [](const void* a, uints& i) -> const void* { return static_cast<const range<T>*>(a)->ptr() + i++; }
        ))
            m || *(T*)0;
    }
    return m;
}


///A helper to check if a type has metastream operator defined
/// Usage: has_metastream_operator<T>::value

namespace check {
    template<typename T> char operator || (coid::metastream&, T&);

    template <typename T>
    struct helper {
        typedef typename std::remove_reference<T>::type B;
        typedef typename std::remove_pointer<T>::type C;
        typedef typename std::remove_const<C>::type X;

        enum {
            value = std::is_enum<X>::value || (sizeof(*(metastream*)(0) || *(X*)(0)) != sizeof(char))
        };
    };
}

template <typename T>
struct has_metastream_operator {
    static constexpr bool value = check::helper<T>::value;
};

COID_NAMESPACE_END



////////////////////////////////////////////////////////////////////////////////
///Helper macros for structs

#define COID_METABIN_OP1(TYPE,P0) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0); });\
        return m; }}

#define COID_METABIN_OP2(TYPE,P0,P1) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0); m.member(#P1, v.P1); });\
        return m; }}

#define COID_METABIN_OP3(TYPE,P0,P1,P2) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0); m.member(#P1, v.P1); m.member(#P2, v.P2); });\
        return m; }}

#define COID_METABIN_OP4(TYPE,P0,P1,P2,P3) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0); m.member(#P1, v.P1); m.member(#P2, v.P2); m.member(#P3, v.P3); });\
        return m; }}




#define COID_METABIN_OP1D(TYPE,P0,P1,D0,D1) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0, D0); });\
        return m; }}

#define COID_METABIN_OP2D(TYPE,P0,P1,D0,D1) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0, D0); m.member(#P1, v.P1, D1); });\
        return m; }}

#define COID_METABIN_OP3D(TYPE,P0,P1,P2,D0,D1,D2) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0, D0); m.member(#P1, v.P1, D1); m.member(#P2, v.P2, D2); });\
        return m; }}

#define COID_METABIN_OP4D(TYPE,P0,P1,P2,P3,D0,D1,D2,D3) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member(#P0, v.P0, D0); m.member(#P1, v.P1, D1); m.member(#P2, v.P2, D2); m.member(#P3, v.P3, D3); });\
        return m; }}




#define COID_METABIN_OP1A(TYPE,ELEM) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member_fixed_size_array_ptr<1>("col", &v[0], false); });\
        return m; }}

#define COID_METABIN_OP2A(TYPE,ELEM) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member_fixed_size_array_ptr<2>("col", &v[0], false); });\
        return m; }}

#define COID_METABIN_OP3A(TYPE,ELEM) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member_fixed_size_array_ptr<3>("col", &v[0], false); });\
        return m; }}

#define COID_METABIN_OP4A(TYPE,ELEM) namespace coid {\
    inline metastream& operator || (metastream& m, TYPE& v) {\
        m.compound_type(v, [&]() { m.member_fixed_size_array_ptr<4>("col", &v[0], false); });\
        return m; }}

#endif //__COID_COMM_METASTREAM__HEADER_FILE__
