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

//@file formatting stream for V8 javascript engine

COID_NAMESPACE_BEGIN

///token wrapper for v8
class v8_token : public v8::String::ExternalAsciiStringResource
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
            _top->object = val->ToObject();
    }

    ///Get a v8 Value result from streaming
    v8::Handle<v8::Value> get()
    {
        DASSERT(_stack.size() == 1);
        if(!_top) return v8::Null();

        v8::Handle<v8::Value> v = _top->value;
        _top = _stack.pop();

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
    virtual void fmtstream_err( charstr& err )
    {
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
            _top->object->Set(v8::String::New(_top->key.ptr(), _top->key.len()), _top->value);
        }

        _top->key = key;
        return 0;
    }

    opcd read_key( charstr& key, int kmember, const token& expected_key )
    {
        //looks up the variable in the current object
        _top->value = _top->object->Get(v8::String::NewSymbol(expected_key.ptr(), expected_key.len()));

        if(_top->value.IsEmpty())
            return ersNO_MORE;

        key = expected_key;
        return 0;
    }

    ////////////////////////////////////////////////////////////////////////////////
    opcd write( const void* p, type t )
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
            if(t.type != type::T_BINARY && t.type != type::T_CHAR)
                _top->array = v8::Array::New(t.get_count(p));
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
                _top->object->Set(v8::String::New(_top->key.ptr(), _top->key.len()), _top->value);

            DASSERT(_stack.size() > 1);
            _top[-1].value = _top->object;
            _top = _stack.pop();
        }
        else if( t.type == type::T_STRUCTBGN )
        {
            _top = _stack.push();
            _top->object = v8::Object::New();
        }
        else
        {
            switch( t.type )
            {
                case type::T_INT:
                    if(t.get_size() == 8)
                        _top->value = v8::Number::New(double(*(int64*)p));
                    else {
                        int32 v=0;
                        switch( t.get_size() )
                        {
                        case 1: v = *(int8*)p;  break;
                        case 2: v = *(int16*)p; break;
                        case 4: v = *(int32*)p; break;
                        }
                        _top->value = v8::Int32::New(v);
                    }
                    break;

                case type::T_UINT:
                    if(t.get_size() == 8)
                        _top->value = v8::Number::New(double(*(uint64*)p));
                    else {
                        uint32 v=0;
                        switch( t.get_size() )
                        {
                        case 1: v = *(uint8*)p;  break;
                        case 2: v = *(uint16*)p; break;
                        case 4: v = *(uint32*)p; break;
                        }
                        _top->value = v8::Uint32::NewFromUnsigned(v);
                    }
                    break;

                case type::T_CHAR: {

                    if( !t.is_array_element() ) {
                        _top->value = v8::String::New((const char*)p, 1);
                    }
                    else {
                        DASSERT(0); //should not get here - optimized in write_array
                    }

                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_FLOAT: {
                    double v=0;
                    switch( t.get_size() ) {
                    case 4:
                        v = *(const float*)p;
                        break;
                    case 8:
                        v = *(const double*)p;
                        break;
                    }
                    _top->value = v8::Number::New(v);
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BOOL:
                    _top->value = v8::Boolean::New(*(bool*)p);
                break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_TIME: {
                    _top->value = v8::Date::New( (double) ((const timet*)p)->t );
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ANGLE: {
                    _top->value = v8::Number::New(*(const double*)p);
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ERRCODE:
                    {
                        opcd e = (const opcd::errcode*)p;
                        _top->value = v8::Integer::New(e.code());
                    }
                break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BINARY: {
                    _bufw.reset();
                    write_binary( p, t.get_size() );

                    _top->value = v8::String::New((const char*)_bufw.ptr(), _bufw.len());
                } break;

                case type::T_COMPOUND:
                    break;

                default: return ersSYNTAX_ERROR "unknown type"; break;
            }
        }

        return 0;
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    opcd read( void* p, type t )
    {
        opcd e=0;
        if( t.is_array_start() )
        {
            if( t.type == type::T_BINARY ) {
                DASSERT(0);
            }
            else if( t.type == type::T_CHAR ) {
                v8::String::Utf8Value text(_top->value);
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
                    _top->array = v8::Local<v8::Array>(v8::Array::Cast(*_top->value));
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
            if(!_top->value->IsObject())
                e = ersSYNTAX_ERROR "expected object";
            else {
                entry* le = _stack.add(1);
                _top = le-1;

                le->object = _top->value->ToObject();
                _top = le;
            }
        }
        else
        {
            switch( t.type )
            {
                case type::T_INT:
                    {
                        int64 v;
                        try {
                            v = _top->value->IntegerValue();

                            if( !valid_int_range(v,t.get_size()) )
                                return ersINTEGER_OVERFLOW;

                            switch( t.get_size() )
                            {
                            case 1: *(int8*)p = (int8)v;  break;
                            case 2: *(int16*)p = (int16)v;  break;
                            case 4: *(int32*)p = (int32)v;  break;
                            case 8: *(int64*)p = (int64)v;  break;
                            }
                        }
                        catch(v8::Exception) {
                            e = ersSYNTAX_ERROR "expected number";
                        }
                    }
                    break;
                
                case type::T_UINT:
                    {
                        int64 v;
                        try {
                            v = _top->value->IntegerValue();

                            if( !valid_uint_range(v,t.get_size()) )
                                return ersINTEGER_OVERFLOW;

                            switch( t.get_size() )
                            {
                            case 1: *(uint8*)p = (uint8)v;  break;
                            case 2: *(uint16*)p = (uint16)v;  break;
                            case 4: *(uint32*)p = (uint32)v;  break;
                            case 8: *(uint64*)p = (uint64)v;  break;
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
                            v8::String::Utf8Value str(_top->value);

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
                
                case type::T_FLOAT:
                    {
                        double v;
                        try {
                            v = _top->value->NumberValue();

                            switch( t.get_size() )
                            {
                            case 4: *(float*)p = (float)v;  break;
                            case 8: *(double*)p = v;  break;
                            }
                        } catch(v8::Exception) {
                            e = ersSYNTAX_ERROR "expected number";
                        }
                    }
                    break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BOOL: {
                        try {
                            *(bool*)p = _top->value->BooleanValue();
                        } catch(v8::Exception) {
                            e = ersSYNTAX_ERROR "expected boolean";
                        }
                    } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_TIME: {
                    try {
                        *(timet*)p = timet((int64)_top->value->NumberValue());
                    } catch(v8::Exception) {
                        return ersSYNTAX_ERROR "expected time";
                    }
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ANGLE: {
                    double a;
                    if(_top->value->IsString()) {
                        v8::String::Utf8Value str(_top->value);
                        token tok(*str, str.length());
                        a = tok.toangle();
                    }
                    else {
                        try {
                            a = _top->value->NumberValue();
                        } catch(v8::Exception) {
                            e = ersSYNTAX_ERROR "expected angle";
                        }
                    }

                    if(!e)
                        *(double*)p = a;
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_ERRCODE: {
                    int64 v;
                    try {
                        v = _top->value->IntegerValue();
                        opcd e;
                        e.set(uint(v));

                        *(opcd*)p = e;
                    }
                    catch(v8::Exception) {
                        e = ersSYNTAX_ERROR "expected number";
                    }
                } break;

                /////////////////////////////////////////////////////////////////////////////////////
                case type::T_BINARY: {
                    try {
                        v8::String::Utf8Value str(_top->value);
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


    virtual opcd write_array_separator( type t, uchar end )
    {
        if(_top->element > 0)
            _top->array->Set(_top->element-1, _top->value);
        ++_top->element;

        return 0;
    }

    virtual opcd read_array_separator( type t )
    {
        if(_top->element >= _top->array->Length())
            return ersNO_MORE;

        _top->value = _top->array->Get(_top->element++);
        return 0;
    }

    virtual opcd write_array_content( binstream_container_base& c, uints* count )
    {
        type t = c._type;
        uints n = c._nelements;
        //c.set_array_needs_separators();

        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY && t.type != type::T_BINARY )
            return write_compound_array_content(c,count);

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
                _top->value = v8::String::New(tok.ptr(), tok.len());
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

    virtual opcd read_array_content( binstream_container_base& c, uints n, uints* count )
    {
        type t = c._type;
        
        if( t.type != type::T_CHAR  &&  t.type != type::T_BINARY ) {
            uint na = _top->array->Length();

            if(n == UMAXS)
                n = na;
            else if(n != na)
                return ersMISMATCHED "array size";

            return read_compound_array_content(c,n,count);
        }

        //if(!_top->value->IsString())
        //    return ersSYNTAX_ERROR "expected string";

        v8::String::Utf8Value str(_top->value);
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


COID_NAMESPACE_END

#endif  // ! __COID_COMM_FMTSTREAM_V8__HEADER_FILE__
