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
 * Outerra
 * Portions created by the Initial Developer are Copyright (C) 2003
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
/*
 * Copyright (C) 2012 Outerra. All Rights Reserved.
 */

#ifndef __COID_COMM_FMTSTREAM_V8__HEADER_FILE__
#define __COID_COMM_FMTSTREAM_V8__HEADER_FILE__

#include "fmtstream.h"
#include <v8/v8.h>

#include "../range.h"
#include "../str.h"
#include "../ref.h"
#include "metastream.h"

namespace v8 {

#define V8_DECL_ISO(iso)            v8::Isolate* iso = v8::Isolate::GetCurrent()
#define V8_DECL_ISO_ARGS(iso,args)  v8::Isolate* iso = args.GetIsolate()

#ifdef V8_MAJOR_VERSION

    #define V8_UNDEFINED(iso)       v8::Undefined(iso)
    #define V8_NULL(iso)            v8::Null(iso)
    #define V8_NAME                 v8::Name

    #define V8_HANDLE_SCOPE(iso,hs)     v8::HandleScope hs(iso)
    #define V8_ESCAPABLE_SCOPE(iso,hs)  v8::EscapableHandleScope hs(iso)
    #define V8_ESCAPE(hs,h)         hs.Escape(h)

    typedef void                    CBK_RET;
    typedef FunctionCallbackInfo<Value> ARGUMENTS;
    typedef PropertyCallbackInfo<Value> ACCESSOR;
    #define V8_RETURN(args,val)     (void)args.GetReturnValue().Set(val);

    #define V8_NEWTYPE(iso,t)       v8::t::New(iso)
    #define V8_NEWTYPE2(iso,t,a)    v8::t::New(iso, a)
    #define V8_PERSISTENT(iso,o,n)  (o).Reset(iso, n)
    #define V8_RESET(o)             (o).Reset()
    #define V8_LOCAL(iso,o)         (o).Get(iso)
    #define V8_CUR_CONTEXT(iso)     iso->GetCurrentContext()
    #define V8_CUR_CTX_OPT(iso)     iso->GetCurrentContext()
    #define V8_CUR_CTX_INIT(iso)    = iso->GetCurrentContext()
    #define V8_OPTARG(v)            v,
    #define V8_OPTARG1(v)           v
    #define V8_FROMJUST             .FromJust()

    #define V8_TRYCATCH(iso,t)      v8::TryCatch t(iso)
   
    inline Local<String> symbol( const coid::token& tok, Isolate* iso = 0 ) {
        return String::NewFromOneByte(iso ? iso : Isolate::GetCurrent(), (const uint8*)tok.ptr(), NewStringType::kInternalized, tok.len()).ToLocalChecked();
    }

    inline Local<String> string_utf8( const coid::token& tok, Isolate* iso = 0 ) {
        return String::NewFromUtf8(iso ? iso : Isolate::GetCurrent(), tok.ptr(), NewStringType::kNormal, tok.len()).ToLocalChecked();
    }

    template<class T>
    inline auto new_object(Isolate* iso = 0) -> decltype(T::New(iso)) {
        return T::New(iso ? iso : Isolate::GetCurrent());
    }

    template<class T, class P1>
    inline auto new_object( const P1& p1, Isolate* iso = 0 ) -> decltype(T::New(iso, p1)) {
        return T::New(iso ? iso : Isolate::GetCurrent(), p1);
    }

    inline void throw_js( Isolate* iso, v8::Local<v8::Value> (*err)(v8::Local<v8::String>), const coid::token& s ) {
        iso->ThrowException((*err)(v8::String::NewFromUtf8(iso, s.ptr(), v8::String::kNormalString, s.len())));
    }

#else

    #define V8_UNDEFINED(iso)       v8::Undefined()
    #define V8_NULL(iso)            v8::Null()
    #define V8_NAME                 v8::String

    #define V8_HANDLE_SCOPE(iso,hs)     v8::HandleScope hs
    #define V8_ESCAPABLE_SCOPE(iso,hs)  v8::HandleScope hs
    #define V8_ESCAPE(hs,h)         hs.Close(h)

    typedef Handle<Value>           CBK_RET;
    typedef Arguments               ARGUMENTS;
    typedef AccessorInfo            ACCESSOR;
    #define V8_RETURN(args,val)     val

    #define V8_NEWTYPE(iso,t)       v8::t::New()
    #define V8_NEWTYPE2(iso,t,a)    v8::t::New(a)
    #define V8_PERSISTENT(iso,o,n)  o = o.New(iso,n)
    #define V8_RESET(o)             do { if (!(o).IsWeak()) (o).Dispose(); (o).Clear(); } while(0)
    #define V8_LOCAL(iso,o)         o
    #define V8_CUR_CONTEXT(iso)     v8::Context::GetCurrent()
    #define V8_CUR_CTX_OPT(iso)
    #define V8_CUR_CTX_INIT(iso)
    #define V8_OPTARG(v)
    #define V8_OPTARG1(v)
    #define V8_FROMJUST

    #define V8_TRYCATCH(iso,t)      v8::TryCatch t
    
    inline Local<String> symbol( const coid::token& tok, Isolate* iso = 0 ) {
        return String::NewSymbol(tok.ptr(), tok.len());
    }

    inline Local<String> string_utf8( const coid::token& tok, Isolate* iso = 0 ) {
        return String::New(tok.ptr(), tok.len());
    }

    template<class T>
    inline auto new_object(Isolate* iso = 0) -> decltype(T::New()) { return T::New(); }

    template<class T, class P1>
    inline auto new_object( const P1& p1, Isolate* iso = 0 ) -> decltype(T::New(p1)) { return T::New(p1); }

    inline Handle<Value> throw_js( Isolate* iso, Local<Value> (*err)(Handle<String>), const coid::token& s ) {
        return ThrowException(err(String::New(s.ptr(), s.len())));
    }

#endif

} //namespace v8


//@file formatting stream for V8 javascript engine

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
class fmtstream_v8;

///Helper class to stream types to v8 objects
template<class T>
class to_v8
{
public:
    ///Generic generator from a type to its v8 representation
    static v8::Handle<v8::Value> read(const T& v);
};

///Helper class to stream types from v8 objects
template<class T>
class from_v8
{
public:
    ///Generic parser from v8 type representation
    static bool write( v8::Handle<v8::Value> src, T& res );
};


//@{ Fast to_v8/from specializations for base types
#ifdef V8_MAJOR_VERSION

#define V8_FAST_STREAMER(T,V8T,CT) \
template<> class to_v8<T> { public: \
    static v8::Handle<v8::Value> read(const T& v) { return v8::new_object<v8::V8T>(CT(v)); } \
    static v8::Handle<v8::Value> read(const T* v) { if (v) return v8::new_object<v8::V8T>(CT(*v)); return V8_UNDEFINED(v8::Isolate::GetCurrent()); } \
}; \
template<> class from_v8<T> { public: \
    static bool write( v8::Handle<v8::Value> src, T& res ) {\
        auto mv = src->V8T##Value(v8::Isolate::GetCurrent()->GetCurrentContext()); \
        if (mv.IsJust()) res = (T)mv.FromJust(); \
        return mv.IsJust(); } \
}

#else

#define V8_FAST_STREAMER(T,V8T,CT) \
template<> class to_v8<T> { public: \
    static v8::Handle<v8::Value> read(const T& v) { return v8::new_object<v8::V8T>(CT(v)); } \
    static v8::Handle<v8::Value> read(const T* v) { if (v) return v8::new_object<v8::V8T>(CT(*v)); return V8_UNDEFINED(v8::Isolate::GetCurrent()); } \
}; \
template<> class from_v8<T> { public: \
    static bool write( v8::Handle<v8::Value> src, T& res ) {\
        res = (T)src->V8T##Value(); \
        return true; } \
}

#endif

V8_FAST_STREAMER(int8,  Int32, int32);
V8_FAST_STREAMER(int16, Int32, int32);
V8_FAST_STREAMER(int32, Int32, int32);
V8_FAST_STREAMER(int64, Number, double);     //can lose data in conversion

V8_FAST_STREAMER(uint8,  Uint32, uint32);
V8_FAST_STREAMER(uint16, Uint32, uint32);
V8_FAST_STREAMER(uint32, Uint32, uint32);
V8_FAST_STREAMER(uint64, Number, double);    //can lose data in conversion

#ifdef SYSTYPE_WIN
# ifdef SYSTYPE_32
V8_FAST_STREAMER(ints,  Int32,  int32);
V8_FAST_STREAMER(uints, Uint32, uint32);
# else //SYSTYPE_64
V8_FAST_STREAMER(int,   Number, double);
V8_FAST_STREAMER(uint,  Number, double);
# endif
#elif defined(SYSTYPE_32)
V8_FAST_STREAMER(long,  Int32,  int32);
V8_FAST_STREAMER(ulong, Uint32, uint32);
#endif

V8_FAST_STREAMER(float,  Number, double);
V8_FAST_STREAMER(double, Number, double);

V8_FAST_STREAMER(bool, Boolean, bool);

///Date/time
template<> class to_v8<timet> {
public:
    static v8::Handle<v8::Value> read(const timet& v) {
        return v8::new_object<v8::Date>(double(v.t));
    }
};
    
template<> class from_v8<timet> {
public:
    static bool write( v8::Handle<v8::Value> src, timet& res ) {
#if V8_MAJOR_VERSION
        auto mv = src->NumberValue(v8::Isolate::GetCurrent()->GetCurrentContext());
        if (mv.IsJust())
            res = timet((int64)mv.FromJust());
        return mv.IsJust();
#else
        res = timet((int64)src->NumberValue());
        return true;
#endif
    }
};

///Strings
template<> class to_v8<charstr> {
public:
    static v8::Handle<v8::Value> read(const charstr& v) { return v8::string_utf8(v); }
    
    static v8::Handle<v8::Value> read(const token& v) { return v8::string_utf8(v); }
    
    static v8::Handle<v8::Value> read(const char* v) { return v8::string_utf8(v); }
};

template<> class from_v8<charstr> {
public:
    static bool write( v8::Handle<v8::Value> src, charstr& res ) {
        if (src->IsUndefined() || src->IsNull()) {
            res.reset();
            return false;
        }
        v8::Isolate* iso = v8::Isolate::GetCurrent();
        v8::String::Utf8Value str(V8_OPTARG(iso) src);
        res.set_from(*str, str.length());
        return true;
    }
};


template<> class to_v8<token> {
public:
    static v8::Handle<v8::Value> read(const token& v) { return v8::string_utf8(v); }
};


///Helper to fill dynarray from V8 array
template<class T>
inline bool v8_write_dynarray( v8::Handle<v8::Value> src, dynarray<T>& a )
{
    if (!src->IsArray()) {
        a.reset();
        return false;
    }

    V8_DECL_ISO(iso);
    v8::Local<v8::Object> obj = src->ToObject(V8_OPTARG1(iso));
    uint n = obj->Get(v8::symbol("length", iso))->Uint32Value(V8_CUR_CTX_OPT(iso)) V8_FROMJUST;
    a.alloc(n);

    for (uint i=0; i<n; ++i) {
        from_v8<T>::write(obj->Get(i), a[i]);
    }

    return true;
}

#ifdef V8_MAJOR_VERSION

///Helper to fill dynarray from V8 array
template<class T>
inline bool v8_write_dynarray( v8::TypedArray* src, dynarray<T>& a )
{
    uints n = src->Length();
    src->CopyContents(a.alloc(n), n * sizeof(T));

    return true;
}

#endif

///Generic dynarray<T> partial specialization
template<class T> class to_v8<dynarray<T>> {
public:
    static v8::Handle<v8::Value> read(const dynarray<T>& v) {
        uint n = (uint)v.size();
        v8::Local<v8::Array> a = v8::new_object<v8::Array>(n);

        for (uint i = 0; i < n; ++i) {
            a->Set(i, to_v8<T>::read(v[i]));
        }

        return a;
    }
};
    
template<class T> class from_v8<dynarray<T>> {
public:
    static bool write(v8::Handle<v8::Value> src, dynarray<T>& res) {
        return v8_write_dynarray(src, res);
    }
};

///Generic range<T> partial specialization
template<class T> class to_v8<range<T>> {
public:
    static v8::Handle<v8::Value> read(const range<T>& v) {
        uint n = (uint)v.size();
        v8::Local<v8::Array> a = v8::new_object<v8::Array>(n);

        for(uint i=0; i<n; ++i) {
            a->Set(i, to_v8<T>::read(v[i]));
        }

        return a;
    }
};

///Generic iref<T> partial specialization
template<class T> class to_v8<iref<T>> {
public:
    static v8::Handle<v8::Value> read(const iref<T>& v) {
        v8::Isolate * iso = v8::Isolate::GetCurrent();
        typedef v8::Handle<v8::Value>(*ifc_create_wrapper_fn)(T * orig, v8::Handle<v8::Context> context);

        ifc_create_wrapper_fn wrap = reinterpret_cast<ifc_create_wrapper_fn>(v->intergen_wrapper(T::IFC_BACKEND_JS));
        if (!wrap) {
            return V8_NULL(iso);
        }

        return wrap(v.get(), v8::Handle<v8::Context>());
    }
};

template<class T> class from_v8<iref<T>> {
public:
    static bool write(v8::Handle<v8::Value> src, iref<T>& res) {
        res = ::js::template unwrap_object<T>(src);
        return !res.is_empty();
    }

};


///Helper class to stream types to/from v8 objects for volatile data (ifc_volatile)
template<class T>
class to_v8_volatile : public to_v8<T> {
public:
    ///Generic generator from a type to its v8 representation
    static v8::Handle<v8::Value> read(const T& v) {
        return to_v8<T>::read(v);
    }
};

template<class T>
class from_v8_volatile : public from_v8<T> {
public:
    ///Generic parser from v8 type representation
    static bool write( v8::Handle<v8::Value> src, T& res ) {
        return from_v8<T>::write(src, res);
    }

    static void cleanup( v8::Handle<v8::Value> val ) {}
};

/*
// TYPED ARRAYS

template<class T>
struct typed_array
{
    typedef binstream_container_base::fnc_stream    fnc_stream;

    typed_array( const T* ptr, uints size ) : _ptr(ptr), _size(size)
    {}

    const T* ptr() const { return _ptr; }
    uints size() const { return _size; }

    struct typed_array_binstream_container : public binstream_containerT<T>
    {
        virtual const void* extract( uints n )
        {
            DASSERT( _pos+n <= _v.size() );
            const T* p = &_v.ptr()[_pos];
            _pos += n;
            return p;
        }

        virtual void* insert( uints n ) {
            throw exception() << "unsupported";
        }

        virtual bool is_continuous() const { return true; }

        virtual uints count() const { return _v.size(); }

        typed_array_binstream_container( const typed_array<T>& v )
            : _v(const_cast<dynarray<T>&>(v))
        {
            _pos = 0;
        }

        typed_array_binstream_container( const typed_array<T>& v, fnc_stream fout, fnc_stream fin )
            : binstream_containerT<T>(fout,fin)
            , _v(const_cast<typed_array<T>&>(v))
        {
            _pos = 0;
        }

    protected:
        typed_array<T>& _v;
        uints _pos;
    };

private:

    const T* _ptr;
    uints _size;
};*/


#ifdef V8_MAJOR_VERSION

///Helper to map typed array from C++ to V8
template<class T, class V8AT>
inline v8::Handle<v8::ArrayBufferView> v8_map_array_view( const T* ptr, uint count )
{
    v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), (void*)ptr, count * sizeof(T));
    return V8AT::New(ab, 0, count);
}

///Macro for direct array mapping to V8
#define V8_STREAMER_MAPARRAY(T, V8AT) \
template<> class to_v8_volatile<dynarray<T>> : public to_v8<dynarray<T>> {\
public:\
    static v8::Handle<v8::Value> read(const dynarray<T>& v) { \
        return v8_map_array_view<T, V8AT>(v.ptr(), (uint)v.size()); \
    } \
    static void cleanup( v8::Handle<v8::Value> val ) { \
        v8::ArrayBufferView::Cast(*val)->Buffer()->Neuter(); \
    } \
}; \
template<> class from_v8_volatile<dynarray<T>> : public from_v8<dynarray<T>> {\
public:\
    static bool write( v8::Handle<v8::Value> src, dynarray<T>& res ) { \
        return v8_write_dynarray(v8::TypedArray::Cast(*src), res); \
    } \
}; \
template<> class to_v8_volatile<range<T>> : public to_v8<range<T>> {\
public:\
    static v8::Handle<v8::Value> read(const range<T>& v) { \
        return v8_map_array_view<T, V8AT>(v.ptr(), (uint)v.size()); \
    } \
    static void cleanup( v8::Handle<v8::Value> val ) { \
        v8::ArrayBufferView::Cast(*val)->Buffer()->Neuter(); \
    } \
};

V8_STREAMER_MAPARRAY(int8, v8::Int8Array)
V8_STREAMER_MAPARRAY(uint8, v8::Uint8Array)
V8_STREAMER_MAPARRAY(int16, v8::Int16Array)
V8_STREAMER_MAPARRAY(uint16, v8::Uint16Array)
V8_STREAMER_MAPARRAY(int32, v8::Int32Array)
V8_STREAMER_MAPARRAY(uint32, v8::Uint32Array)
V8_STREAMER_MAPARRAY(float, v8::Float32Array)
V8_STREAMER_MAPARRAY(double, v8::Float64Array)

#ifdef SYSTYPE_WIN
# ifdef SYSTYPE_32
V8_STREAMER_MAPARRAY(ints, v8::Int32Array)
V8_STREAMER_MAPARRAY(uints, v8::Uint32Array)
# else //SYSTYPE_64
V8_STREAMER_MAPARRAY(int, v8::Int32Array)
V8_STREAMER_MAPARRAY(uint, v8::Uint32Array)
# endif
#elif defined(SYSTYPE_32)
V8_STREAMER_MAPARRAY(long, v8::Int32Array)
V8_STREAMER_MAPARRAY(ulong, v8::Uint32Array)
#endif

#else

///Helper to map typed array from C++ to V8
template<class T>
inline v8::Handle<v8::Value> v8_map_range( const T* ptr, uint count, v8::ExternalArrayType type )
{
    v8::Handle<v8::Object> a = v8::Object::New();
    a->SetIndexedPropertiesToExternalArrayData((void*)ptr, type, count);
    a->Set(v8::symbol("length"), v8::Uint32::NewFromUnsigned(count));

    return a;
}

///Macro for direct array mapping to V8
#define V8_STREAMER_MAPARRAY(T, V8EXT) \
template<> class to_v8_volatile<dynarray<T>> : public to_v8<dynarray<T>> {\
public:\
    static v8::Handle<v8::Value> read(const dynarray<T>& v) { \
        return v8_map_range(v.ptr(), (uint)v.size(), V8EXT); \
    } \
    static void cleanup( v8::Handle<v8::Value> val ) { \
        val->ToObject(V8_OPTARG1(v8::Isolate::GetCurrent()))->SetIndexedPropertiesToExternalArrayData(0, V8EXT, 0); \
    } \
}; \
template<> class from_v8_volatile<dynarray<T>> : public from_v8<dynarray<T>> {\
public:\
    static bool write( v8::Handle<v8::Value> src, dynarray<T>& res ) { \
        return v8_write_dynarray(src, res); \
    } \
}; \
template<> class to_v8_volatile<range<T>> : public to_v8<range<T>> {\
public:\
    static v8::Handle<v8::Value> read(const range<T>& v) { \
        return v8_map_range(v.ptr(), (uint)v.size(), V8EXT); \
    } \
    static void cleanup( v8::Handle<v8::Value> val ) { \
        val->ToObject(V8_OPTARG1(v8::Isolate::GetCurrent()))->SetIndexedPropertiesToExternalArrayData(0, V8EXT, 0); \
    } \
};

V8_STREAMER_MAPARRAY(int8, v8::kExternalByteArray)
V8_STREAMER_MAPARRAY(uint8, v8::kExternalUnsignedByteArray)
V8_STREAMER_MAPARRAY(int16, v8::kExternalShortArray)
V8_STREAMER_MAPARRAY(uint16, v8::kExternalUnsignedShortArray)
V8_STREAMER_MAPARRAY(int32, v8::kExternalIntArray)
V8_STREAMER_MAPARRAY(uint32, v8::kExternalUnsignedIntArray)
V8_STREAMER_MAPARRAY(float, v8::kExternalFloatArray)
V8_STREAMER_MAPARRAY(double, v8::kExternalDoubleArray)

#ifdef SYSTYPE_WIN
# ifdef SYSTYPE_32
V8_STREAMER_MAPARRAY(ints, v8::kExternalIntArray)
V8_STREAMER_MAPARRAY(uints, v8::kExternalUnsignedIntArray)
# else //SYSTYPE_64
V8_STREAMER_MAPARRAY(int, v8::kExternalIntArray)
V8_STREAMER_MAPARRAY(uint, v8::kExternalUnsignedIntArray)
# endif
#elif defined(SYSTYPE_32)
V8_STREAMER_MAPARRAY(long, v8::kExternalIntArray)
V8_STREAMER_MAPARRAY(ulong, v8::kExternalUnsignedIntArray)
#endif

#endif


///V8 values
template<typename V> class to_v8<v8::Handle<V>> {
public:
    typedef v8::Handle<V> T;
    static v8::Handle<v8::Value> read(T v) { return v; }
};

template<typename V> class from_v8<v8::Handle<V>> {
public:
    typedef v8::Handle<V> T;
    static bool write( v8::Handle<v8::Value> src, T& res ) { res = src.Cast<T>(src); return true; }
};

template<> class to_v8<v8::Handle<v8::Function>> {
public:
    typedef v8::Handle<v8::Function> T;
    static v8::Handle<v8::Value> read(T v) { return v; }
};

template<> class from_v8<v8::Handle<v8::Function>> {
public:
    typedef v8::Handle<v8::Function> T;
    static bool write(v8::Handle<v8::Value> src, T& res) { res = v8::Handle<v8::Function>::Cast(src); return true; }
};

//dummy metastream operator for v8::Handle
template<typename V>
inline metastream& operator || (metastream& m, v8::Handle<V>) { RASSERT(0); return m; }

//@}


////////////////////////////////////////////////////////////////////////////////
///token wrapper for v8
class v8_token : public v8::String::ExternalOneByteStringResource
{
    token _tok;

public:

    ~v8_token() {}

    v8_token()
    {}
    v8_token( const token& tok ) : _tok(tok)
    {}

    void set( const token& tok ) {
        _tok = tok;
    }

    const char* data() const { return _tok.ptr(); }

    size_t length() const { return _tok.len(); }

protected:

    void Dispose() {}
};

////////////////////////////////////////////////////////////////////////////////
class fmtstream_v8 : public fmtstream
{
protected:

    ///Stack entry
    struct entry
    {
        v8::Local<v8::Object> object;
        v8::Handle<v8::Value> value;
        v8::Local<v8::Array> array;

        uint element;
        token key;

        entry() : element(0)
        {}
    };

    dynarray<entry> _stack;
    entry* _top;

public:
    fmtstream_v8()
    {
        _stack.reserve(32, true);

        _top = _stack.alloc(1);
        _top->element = 0;
    }

    ~fmtstream_v8() {}


    ///Set v8 Value to read from
    void set(v8::Handle<v8::Value> val) {
        _top = _stack.alloc(1);
        _top->value = val;
        
        if(val->IsObject())
            _top->object = val->ToObject(V8_OPTARG1(v8::Isolate::GetCurrent()));
    }

    ///Get a v8 Value result from streaming
    v8::Handle<v8::Value> get()
    {
        DASSERT(_stack.size() == 1);
        if(!_top) return v8::Null(v8::Isolate::GetCurrent());

        v8::Handle<v8::Value> v = _top->value;
        _top = _stack.alloc(1);
        _top->element = 0;

        return v;
    }


    virtual token fmtstream_name() { return "fmtstream_v8"; }

    virtual void fmtstream_file_name( const token& file_name )
    {
        //_tokenizer.set_file_name(file_name);
    }

    ///Return formatting stream error (if any) and current line and column for error reporting purposes
    //@param err [in] error text
    //@param err [out] final (formatted) error text with line info etc.
    virtual void fmtstream_err( charstr& err, bool add_context = true )
    {
        err.ins(0, "[js] ");
    }

    void acknowledge( bool eat=false )
    {
        _top = _stack.alloc(1);
        _top->element = 0;
    }

    void flush()
    {
        _top = _stack.alloc(1);
        _top->element = 0;
    }


    ////////////////////////////////////////////////////////////////////////////////
    opcd write_key( const token& key, int kmember )
    {
        if(kmember > 0) {
            //attach the previous property
            _top->object->Set(v8::symbol(_top->key), _top->value);
        }

        _top->key = key;
        return 0;
    }

    opcd read_key( charstr& key, int kmember, const token& expected_key )
    {
        if(_top->object.IsEmpty())
            return ersNO_MORE;

        //looks up the variable in the current object
        _top->value = _top->object->Get(v8::symbol(expected_key));

        if(_top->value->IsUndefined())
            return ersNO_MORE;

        key = expected_key;
        return 0;
    }

    ////////////////////////////////////////////////////////////////////////////////
    opcd write( const void* p, type t ) override
    {
        if( t.is_array_start() )
        {
            /*if( t.type == type::T_BINARY )
                DASSERT(0);
            else if( t.type == type::T_CHAR )
                //_bufw << "\\\"";
                ;//postpone writing until the escape status is known
            else
                _bufw << char('[');*/
            if(t.type != type::T_BINARY && t.type != type::T_CHAR) {
                _top->array = v8::new_object<v8::Array>((int)t.get_count(p));
                _top->element = 0;
            }
        }
        else if( t.is_array_end() )
        {
            if(t.type != type::T_BINARY && t.type != type::T_CHAR)
                _top->value = _top->array;
        }
        else if( t.type == type::T_STRUCTEND )
        {
            //attach the last property
            if(_top->key)
                _top->object->Set(v8::symbol(_top->key), _top->value);

            DASSERT(_stack.size() > 1);
            _top[-1].value = _top->object;
            _top = _stack.pop();
        }
        else if( t.type == type::T_STRUCTBGN )
        {
            _top = _stack.push();
            _top->object = v8::new_object<v8::Object>();
        }
        else
        {
            switch( t.type )
            {
                case type::T_INT:
                    switch( t.get_size() )
                    {
                    case 1: _top->value = to_v8<int8>() .read(*(int8*)p); break;
                    case 2: _top->value = to_v8<int16>().read(*(int16*)p); break;
                    case 4: _top->value = to_v8<int32>().read(*(int32*)p); break;
                    case 8: _top->value = to_v8<int64>().read(*(int64*)p); break;
                    }
                    break;

                case type::T_UINT:
                    switch( t.get_size() )
                    {
                    case 1: _top->value = to_v8<uint8>() .read(*(uint8*)p); break;
                    case 2: _top->value = to_v8<uint16>().read(*(uint16*)p); break;
                    case 4: _top->value = to_v8<uint32>().read(*(uint32*)p); break;
                    case 8: _top->value = to_v8<uint64>().read(*(uint64*)p); break;
                    }
                    break;

                case type::T_CHAR: {

                    if( !t.is_array_element() ) {
                        _top->value = v8::symbol(token((const char*)p, 1));
                    }
                    else {
                        DASSERT(0); //should not get here - optimized in write_array
                    }

                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_FLOAT:
                    switch( t.get_size() ) {
                    case 4: _top->value = to_v8<float>().read(*(float*)p); break;
                    case 8: _top->value = to_v8<double>().read(*(double*)p); break;
                    }
                    break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BOOL:
                    _top->value = v8::new_object<v8::Boolean>(*(bool*)p);
                break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_TIME: {
                    _top->value = to_v8<timet>().read(*(timet*)p);
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ANGLE: {
                    _top->value = to_v8<double>().read(*(double*)p); break;
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ERRCODE:
                    {
                        opcd e = (const opcd::errcode*)p;
                        _top->value = v8::new_object<v8::Integer>(e.code());
                    }
                break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BINARY: {
                    _bufw.reset();
                    write_binary( p, t.get_size() );

#ifdef V8_MAJOR_VERSION
                    v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(v8::Isolate::GetCurrent(),
                        (void*)_bufw.ptr(), _bufw.len());
                    _top->value = v8::Uint8Array::New(ab, 0, _bufw.len());
#else
                    _top->value = v8::String::New((const char*)_bufw.ptr(), _bufw.len());
#endif
                } break;

                case type::T_COMPOUND:
                    break;

                default: return ersSYNTAX_ERROR "unknown type"; break;
            }
        }

        return 0;
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    opcd read( void* p, type t ) override
    {
        opcd e=0;
        if( t.is_array_start() )
        {
            if( t.type == type::T_BINARY ) {
                DASSERT(0);
            }
            else if( t.type == type::T_CHAR ) {
                v8::String::Utf8Value text(V8_OPTARG(v8::Isolate::GetCurrent()) _top->value);
                if(*text == 0)
                    e = ersSYNTAX_ERROR "expected string";
                else
                    t.set_count(text.length(), p);
            }
            else if( t.type == type::T_KEY ) {
                DASSERT(0);
            }
            else
            {
                if(!_top->value->IsArray())
                    e = ersSYNTAX_ERROR "expected array";
                else {
                    _top->array = v8::Local<v8::Array>::New(v8::Isolate::GetCurrent(), _top->value.As<v8::Array>());
                    _top->element = 0;
                }
            }
        }
        else if( t.is_array_end() )
        {
        }
        else if( t.type == type::T_STRUCTEND )
        {
            _top = _stack.pop();
        }
        else if( t.type == type::T_STRUCTBGN )
        {
            bool undef = _top->value->IsUndefined();
            if(!undef && !_top->value->IsObject())
                e = ersSYNTAX_ERROR "expected object";
            else {
                entry* le = _stack.add(1);
                _top = le-1;

                le->object = _top->value->ToObject(V8_OPTARG1(v8::Isolate::GetCurrent()));
                _top = le;
            }
        }
        else
        {
            switch( t.type )
            {
                case type::T_INT:
                    {
                        try {
                            switch( t.get_size() )
                            {
                            case 1: from_v8<int8>() .write(_top->value, *(int8*)p ); break;
                            case 2: from_v8<int16>().write(_top->value, *(int16*)p); break;
                            case 4: from_v8<int32>().write(_top->value, *(int32*)p); break;
                            case 8: from_v8<int64>().write(_top->value, *(int64*)p); break;
                            }
                        }
                        catch(v8::Exception) {
                            e = ersSYNTAX_ERROR "expected number";
                        }
                    }
                    break;
                
                case type::T_UINT:
                    {
                        try {
                            switch( t.get_size() )
                            {
                            case 1: from_v8<uint8>() .write(_top->value, *(uint8*)p ); break;
                            case 2: from_v8<uint16>().write(_top->value, *(uint16*)p); break;
                            case 4: from_v8<uint32>().write(_top->value, *(uint32*)p); break;
                            case 8: from_v8<uint64>().write(_top->value, *(uint64*)p); break;
                            }
                        }
                        catch(v8::Exception) {
                            e = ersSYNTAX_ERROR "expected number";
                        }
                    }
                    break;

                case type::T_KEY:
                    return ersUNAVAILABLE;// "should be read as array";
                    break;

                case type::T_CHAR:
                    {
                        if( !t.is_array_element() )
                        {
                            v8::String::Utf8Value str(V8_OPTARG(v8::Isolate::GetCurrent()) _top->value);

                            if(*str)
                                *(char*)p = str.length() ? **str : 0;
                            else
                                e = ersSYNTAX_ERROR "expected string";
                        }
                        else
                            //this is optimized in read_array(), non-continuous containers not supported
                            e = ersNOT_IMPLEMENTED;
                    }
                    break;
                
                case type::T_FLOAT: {
                    bool rv = false;
                    switch (t.get_size())
                    {
                    case 4: rv = from_v8<float>().write(_top->value, *(float*)p); break;
                    case 8: rv = from_v8<double>().write(_top->value, *(double*)p); break;
                    }
                    if (!rv)
                        e = ersSYNTAX_ERROR "expected number";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BOOL: {
                    if (!from_v8<bool>().write(_top->value, *(bool*)p))
                        e = ersSYNTAX_ERROR "expected boolean";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_TIME: {
                    int64 t;
                    if (from_v8<int64>().write(_top->value, t))
                        *(timet*)p = t;
                    else
                        e = ersSYNTAX_ERROR "expected time value";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ANGLE: {
                    if(_top->value->IsString()) {
                        v8::String::Utf8Value str(V8_OPTARG(v8::Isolate::GetCurrent()) _top->value);
                        token tok(*str, str.length());
                        *(double*)p = tok.toangle();
                    }
                    else {
                        if (!from_v8<double>().write(_top->value, *(double*)p))
                            e = ersSYNTAX_ERROR "expected angle";
                    }
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ERRCODE: {
                    int64 v;
                    if (from_v8<int64>().write(_top->value, v)) {
                        opcd e;
                        e.set(uint(v));

                        *(opcd*)p = e;
                    }
                    else
                        e = ersSYNTAX_ERROR "expected number";
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BINARY: {
                    try {
                        v8::String::Utf8Value str(V8_OPTARG(v8::Isolate::GetCurrent()) _top->value);
                        token tok(*str, str.length());

                        uints i = charstrconv::hex2bin( tok, p, t.get_size(), ' ' );
                        if(i>0)
                            return ersMISMATCHED "not enough array elements";
                        tok.skip_char(' ');
                        if( !tok.is_empty() )
                            return ersMISMATCHED "more characters after array elements read";
                    } catch(v8::Exception) {
                        return ersSYNTAX_ERROR "expected hex string";
                    }
                } break;


                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_COMPOUND:
                    break;


                default:
                    return ersSYNTAX_ERROR "unknown type";
                    break;
            }
        }

        return e;
    }


    virtual opcd write_array_separator( type t, uchar end ) override
    {
        if(_top->element > 0)
            _top->array->Set(_top->element-1, _top->value);
        ++_top->element;

        return 0;
    }

    virtual opcd read_array_separator( type t ) override
    {
        if(_top->element >= _top->array->Length())
            return ersNO_MORE;

        _top->value = _top->array->Get(_top->element++);
        return 0;
    }

    virtual opcd write_array_content( binstream_container_base& c, uints* count, metastream* m ) override
    {
        type t = c._type;
        uints n = c.count();
        //c.set_array_needs_separators();

        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY && t.type != type::T_BINARY )
            return write_compound_array_content(c, count, m);

        opcd e;
        //optimized for character and key strings
        if( c.is_continuous()  &&  n != UMAXS )
        {
            token tok;

            if( t.type == type::T_BINARY ) {
                _bufw.reset();
                e = write_binary( c.extract(n), n );
                tok = _bufw;
            }
            else {
                tok.set( (const char*)c.extract(n), n );
            }

            if(!e) {
                _top->value = v8::string_utf8(tok);
                *count = n;
            }
        }
        else {
            RASSERT(0); //non-continuous string containers not supported here
            e = ersNOT_IMPLEMENTED;
            //e = write_compound_array_content(c,count);
        }

        return e;
    }

    virtual opcd read_array_content( binstream_container_base& c, uints n, uints* count, metastream* m ) override
    {
        type t = c._type;
        
        if( t.type != type::T_CHAR  &&  t.type != type::T_BINARY ) {
            uint na = _top->array->Length();

            if(n == UMAXS)
                n = na;
            else if(n != na)
                return ersMISMATCHED "array size";

            return read_compound_array_content(c, n, count, m);
        }

        //if(!_top->value->IsString())
        //    return ersSYNTAX_ERROR "expected string";

        v8::String::Utf8Value str(V8_OPTARG(v8::Isolate::GetCurrent()) _top->value);
        token tok(*str, str.length());

        opcd e=0;
        if( t.type == type::T_BINARY )
            e = read_binary(tok,c,n,count);
        else
        {
            if( n != UMAXS  &&  n != tok.len() )
                e = ersMISMATCHED "array size";
            else if( c.is_continuous() )
            {
                xmemcpy( c.insert(n), tok.ptr(), tok.len() );

                *count = tok.len();
            }
            else
            {
                const char* p = tok.ptr();
                uints nc = tok.len();
                for(; nc>0; --nc,++p )
                    *(char*)c.insert(1) = *p;

                *count = nc;
            }
        }

        return e;
    }


protected:
};

////////////////////////////////////////////////////////////////////////////////
///
struct v8_streamer_context
{
    metastream meta;
    fmtstream_v8 fmtv8;

    v8_streamer_context() {
        meta.bind_formatting_stream(fmtv8);
        meta.set_file_name("[js]");
    }

    ~v8_streamer_context() {
        meta.stream_reset(true, false);
    }

    void reset() {
        meta.stream_reset(true, false);
    }
};

////////////////////////////////////////////////////////////////////////////////
template<bool ISENUM, class T>
struct v8_enum_helper {
    int operator << (const T&) const { return 0; }
    T operator >> (int) const { return T(); }
};

template<class T>
struct v8_enum_helper<true,T> {
    int operator << (const T& v) const { return int(v); }
    T operator >> (int v) const { return T(v); }
};

template<class T>
inline v8::Handle<v8::Value> to_v8<T>::read(const T& v)
{
    v8_enum_helper<std::is_enum<T>::value, T> en;
    if (std::is_enum<T>::value)
        return V8_NEWTYPE2(v8::Isolate::GetCurrent(), Int32, en << v);

    auto& streamer = THREAD_SINGLETON(v8_streamer_context);
    streamer.meta.xstream_out(v);
    return streamer.fmtv8.get();
}


template<class T>
inline bool from_v8<T>::write( v8::Handle<v8::Value> src, T& res )
{
    v8_enum_helper<std::is_enum<T>::value, T> en;
    if (std::is_enum<T>::value)
        res = en >> src->Int32Value(V8_CUR_CTX_OPT(v8::Isolate::GetCurrent())) V8_FROMJUST;
    else {
        auto& streamer = THREAD_SINGLETON(v8_streamer_context);
        streamer.fmtv8.set(src);
        streamer.meta.xstream_in(res);
    }
    return true;
}

template<class T>
inline bool write_from_v8(v8::Handle<v8::Value> src, T& t) {
    return from_v8<T>::write(src, t);
}

template<class T>
inline bool write_from_v8( v8::Handle<v8::Value> src, threadcached<T>& tc ) {
    return from_v8<typename threadcached<T>::storage_type>::write(src, *tc);
}

template<class T>
inline v8::Handle<v8::Value> read_to_v8( const T& v ) {
    return to_v8<typename threadcached<T>::storage_type>::read(v);
}


COID_NAMESPACE_END

#endif  // ! __COID_COMM_FMTSTREAM_V8__HEADER_FILE__
