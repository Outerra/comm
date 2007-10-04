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
 * Portions created by the Initial Developer are Copyright (C) 2003-2007
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
#include "../str.h"
#include "../binstream/binstream.h"

#include "metavar.h"

COID_NAMESPACE_BEGIN

/** \class metastream
    @see http://coid.sourceforge.net/doc/metastream.html for description
**/

////////////////////////////////////////////////////////////////////////////////
///Base class for streaming structured data
class metastream
{
public:
    metastream() { init(); }
    explicit metastream( binstream& bin )
    {
        init();
        bind_formatting_stream(bin);
    }

    virtual ~metastream()    {}


    typedef bstype::kind            type;

    void init()
    {
        _root.desc = 0;

        _current = _cachestack.need(1);
        _current->buf = &_cache;

        _cachequit = 0;
        _cachevar = 0;
        _cacheskip = 0;
        _cacheentries = 0;
        _cachelevel = UMAX;

        _sesopen = 0;
        _beseparator = false;
        _current_var = 0;
        _curvar.var = 0;
        _cur_variable_name = 0;

        _disable_meta_write = _disable_meta_read = false;
        _templ_arg_rdy = false;

        _hook.set_meta( *this );

        _tmetafnc = 0;
        _fmtstream = 0;
    }

    void bind_formatting_stream( binstream& b )
    {
        _fmtstream = &b;
        stream_reset( 0, cache_prepared() );
        _sesopen = 0;
        _beseparator = false;
    }

    binstream& get_formatting_stream() const    { return *_fmtstream; }


    ///Return the transport stream
    binstream& get_binstream()                  { return _hook; }

    void enable_meta_write( bool en )           { _disable_meta_write = !en; }
    void enable_meta_read( bool en )            { _disable_meta_read = !en; }

    ////////////////////////////////////////////////////////////////////////////////
    //@{ methods to physically stream the data utilizing the metastream

    template<class T>
    opcd prepare_type_read(T&, const token& name)
    {
        _root.desc = 0;
        _current_var = 0;

        opcd e=0;
        DASSERT( _sesopen <= 0 );   //or else unflushed write
        if( _sesopen == 0 ) {
            _err.reset();
            //_fmtstream->reset_read();
            _sesopen = -1;
            _curvar.kth = 0;
        }
        
        if(_disable_meta_read) {
            _tmetafnc = 0;
            if(_curvar.kth)
                e = _fmtstream->read_separator();

            if(e) {
                _err << "error reading separator";
                throw e;
            }
            return e;
        }

        _beseparator = false;

        *this << *(const T*)0;     // build description tree

        _tmetafnc = &metacall<T>::stream;

        _curvar.var = &_root;
        _curvar.var->varname = name;
        _curvar.var->nameless_root = name.is_empty();

        return movein_current<READ_MODE>(true);
    }

    template<class T>
    opcd prepare_type_write(T&, const token& name)
    {
        _root.desc = 0;
        _current_var = 0;

        opcd e=0;
        DASSERT( _sesopen >= 0 );   //or else unacked read
        if( _sesopen == 0 ) {
            _err.reset();
            //_fmtstream->reset_write();
            _sesopen = 1;
            _curvar.kth = 0;
        }
        
        if(_disable_meta_write) {
            _tmetafnc = 0;
            if(_curvar.kth)
                e = _fmtstream->write_separator();

            if(e) {
                _err << "error writing separator";
                throw e;
            }
            return e;
        }

        _beseparator = false;

        *this << *(const T*)0;     // build description

        _tmetafnc = &metacall<T>::stream;

        _curvar.var = &_root;
        _curvar.var->varname = name;
        _curvar.var->nameless_root = name.is_empty();

        return movein_current<WRITE_MODE>(true);
    }

    template<class T>
    opcd prepare_type_read_array( T&, uints n, const token& name )
    {
        _root.desc = 0;
        _current_var = 0;

        opcd e=0;
        DASSERT( _sesopen <= 0 );   //or else unflushed write
        if( _sesopen == 0 ) {
            _err.reset();
            //_fmtstream->reset_read();
            _sesopen = -1;
            _curvar.kth = 0;
        }
        
        if(_disable_meta_read) {
            _tmetafnc = 0;
            if(_curvar.kth)
                e = _fmtstream->read_separator();

            if(e) {
                _err << "error reading separator";
                throw e;
            }
            return e;
        }

        _beseparator = false;

        meta_array(n);
        *this << *(const T*)0;     // build description

        _tmetafnc = &metacall<T>::stream;

        _curvar.var = &_root;
        _curvar.var->varname = name;
        _curvar.var->nameless_root = name.is_empty();

        return movein_current<READ_MODE>(true);
    }

    template<class T>
    opcd prepare_type_write_array( T&, uints n, const token& name )
    {
        _root.desc = 0;
        _current_var = 0;

        opcd e=0;
        DASSERT( _sesopen >= 0 );   //or else unacked read
        if( _sesopen == 0 ) {
            _err.reset();
            //_fmtstream->reset_write();
            _sesopen = 1;
            _curvar.kth = 0;
        }
        
        if(_disable_meta_write) {
            _tmetafnc = 0;
            if(_curvar.kth)
                e = _fmtstream->write_separator();
            
            if(e) {
                _err << "error writing separator";
                throw e;
            }
            return e;
        }

        _beseparator = false;

        meta_array(n);
        *this << *(const T*)0;     // build description

        _tmetafnc = &metacall<T>::stream;

        _curvar.var = &_root;
        _curvar.var->varname = name;
        _curvar.var->nameless_root = name.is_empty();

        return movein_current<WRITE_MODE>(true);
    }

    ///Read object of type T from the currently bound formatting stream
    template<class T>
    opcd stream_in( T& x, const token& name = token::empty() )
    {
        stream_reset(0,false);

        opcd e;
        try {
            e = prepare_type_read(x,name);
            if(e) return e;

            _hook >> x;
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Read object of type T from the currently bound formatting stream into the cache
    template<class T>
    opcd cache_in( const token& name = token::empty() )
    {
        stream_reset(0,true);

        opcd e;
        try {
            e = prepare_type_read( *(const T*)0, name );
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Write object of type T to the currently bound formatting stream
    template<class T>
    opcd stream_out( const T& x, const token& name = token::empty() )
    {
        return stream_or_cache_out(x,false,name);
    }

    ///Write object of type T to the cache
    template<class T>
    opcd cache_out( const T& x, const token& name = token::empty() )
    {
        return stream_or_cache_out(x,true,name);
    }

    ///Write object of type T to the currently bound formatting stream
    ///@param cache true if the object should be trapped in the cache instead of sending it out through the formatting stream
    template<class T>
    opcd stream_or_cache_out( const T& x, bool cache, const token& name = token::empty() )
    {
        stream_reset(0,cache);

        opcd e;
        try {
            e = prepare_type_write(x,name);
            if(e) return e;

            if(!cache)
                _hook << x;
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Read array of objects of type T from the currently bound formatting stream
    template<class T>
    opcd stream_array_in( binstream_containerT<T>& C, const token& name = token::empty() )
    {
        stream_reset(0,false);

        opcd e;
        try {
            e = prepare_type_read_array( *(const T*)0, C._nelements, name );
            if(e) return e;

            return _hook.read_array(C);
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Read array of objects of type T from the currently bound formatting stream into the cache
    template<class T>
    opcd cache_array_in( const token& name = token::empty(), uints n=UMAX )
    {
        stream_reset(0,true);

        opcd e;
        try {
            e = prepare_type_read_array( *(const T*)0, n, name );
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Write array of objects of type T to the currently bound formatting stream
    template<class T>
    opcd stream_array_out( binstream_containerT<T>& C, const token& name = token::empty() )
    {
        return stream_or_cache_array_out(C,false,name);
    }

    ///Write array of objects of type T to the currently bound formatting stream
    template<class T>
    opcd cache_array_out( binstream_containerT<T>& C, bool cache=false, const token& name = token::empty() )
    {
        return stream_or_cache_array_out(C,true,name);
    }

    ///Write array of objects of type T to the currently bound formatting stream
    ///@param cache true if the array should be trapped in the cache instead of sending it out through the formatting stream
    template<class T>
    opcd stream_or_cache_array_out( binstream_containerT<T>& C, bool cache, const token& name = token::empty() )
    {
        stream_reset(0,cache);

        opcd e;
        try {
            e = prepare_type_write_array( *(const T*)0, C._nelements, name );
            if(e) return e;

            if(!cache)
                return _hook.write_array(C);
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Read container of objects of type T from the currently bound formatting stream
    template<class CONT>
    opcd stream_container_in( CONT& C, const token& name = token::empty() )
    {
        typedef typename binstream_adapter_writable<CONT>::TBinstreamContainer     BC;

        BC bc = binstream_container_writable<CONT,BC>::create(C);
        return stream_array_in( bc, name );
    }

    ///Read container of objects of type T from the currently bound formatting stream into the cache
    template<class CONT>
    opcd cache_container_in( CONT& C, const token& name = token::empty() )
    {
        typedef typename binstream_adapter_writable<CONT>::TBinstreamContainer     BC;

        BC bc = binstream_container_writable<CONT,BC>::create(C);
        return cache_array_in( bc, name );
    }

    ///Write container of objects of type T to the currently bound formatting stream
    template<class CONT>
    opcd stream_container_out( CONT& C, const token& name = token::empty() )
    {
        typedef typename binstream_adapter_readable<CONT>::TBinstreamContainer     BC;

        BC bc = binstream_container_readable<CONT,BC>::create(C);
        return stream_array_out( bc, name );
    }

    ///Write container of objects of type T to the cache
    template<class CONT>
    opcd cache_container_out( CONT& C, const token& name = token::empty() )
    {
        typedef typename binstream_adapter_readable<CONT>::TBinstreamContainer     BC;

        BC bc = binstream_container_readable<CONT,BC>::create(C);
        return cache_array_out( bc, name );
    }


    template<class T>
    void xstream_in( T& x, const token& name = token::empty() )
    {   opcd e = stream_in(x,name);  if(e) throw e; }

    template<class T>
    void xcache_in( T& x, const token& name = token::empty() )
    {   opcd e = cache_in(x,name);  if(e) throw e; }

    template<class T>
    void xstream_out( T& x, const token& name = token::empty() )
    {   opcd e = stream_out(x,name);  if(e) throw e; }

    template<class T>
    void xcache_out( T& x, const token& name = token::empty() )
    {   opcd e = stream_out(x,name);  if(e) throw e; }


    void stream_acknowledge( bool eat = false )
    {
        DASSERT( _sesopen <= 0 );
        if( _sesopen < 0 ) {
            _sesopen = 0;
            _beseparator = false;
            _fmtstream->acknowledge(eat);
        }
    }

    void stream_flush()
    {
        DASSERT( _sesopen >= 0 );
        if( _sesopen > 0 ) {
            _sesopen = 0;
            _beseparator = false;
            _fmtstream->flush();
        }
    }

    ///@param fmts_reset >0 reset reading pipe, <0 reset writing pipe
    ///@param cache_open leave cache open or closed
    void stream_reset( int fmts_reset, bool cache_open )
    {
        if(fmts_reset) {
            _sesopen = 0;
            _beseparator = false;
        }

        _stack.reset();
        cache_reset(cache_open);

        _tmetafnc = 0;
        _curvar.var = 0;
        _cur_variable_name = 0;

        _err.reset();
        if( fmts_reset>0 )
            _fmtstream->reset_read();
        else if( fmts_reset<0 )
            _fmtstream->reset_write();
    }

//@}

    ///Reset cache and lead it into an active or inactive state
    void cache_reset( bool open )
    {
        _cache.reset();
        _current = _cachestack.need(1);
        _current->offs = open ? 0 : UMAX;
        _cachevar = 0;
        _cacheskip = 0;
    }

    const MetaDesc::Var& get_root_var() const               { return _root; }
    const uchar* get_cache() const                          { return _cache.ptr(); }

    const charstr& error_string() const                     { return _err; }

    template<class T>
    static type get_type(const T&)                          { return bstype::t_type<T>(); }


    metastream& operator << (const bool&a)                  {meta_primitive( "bool", get_type(a) ); return *this;}
    metastream& operator << (const int8&a)                  {meta_primitive( "int8", get_type(a) ); return *this;}
    metastream& operator << (const uint8&a)                 {meta_primitive( "uint8", get_type(a) ); return *this;}
    metastream& operator << (const int16&a)                 {meta_primitive( "int16", get_type(a) ); return *this;}
    metastream& operator << (const uint16&a)                {meta_primitive( "uint16", get_type(a) ); return *this;}
    metastream& operator << (const int32&a)                 {meta_primitive( "int32", get_type(a) ); return *this;}
    metastream& operator << (const uint32&a)                {meta_primitive( "uint32", get_type(a) ); return *this;}
    metastream& operator << (const int64&a)                 {meta_primitive( "int64", get_type(a) ); return *this;}
    metastream& operator << (const uint64&a)                {meta_primitive( "uint64", get_type(a) ); return *this;}

    metastream& operator << (const char&a)                  {meta_primitive( "char", get_type(a) ); return *this;}

#ifdef _MSC_VER
    metastream& operator << (const ints&a)                  {meta_primitive( "int", get_type(a) ); return *this;}
    metastream& operator << (const uints&a)                 {meta_primitive( "uint", get_type(a) ); return *this;}
#else
    metastream& operator << (const long&a)                  {meta_primitive( "long", get_type(a) ); return *this;}
    metastream& operator << (const ulong&a)                 {meta_primitive( "ulong", get_type(a) ); return *this;}
#endif

    metastream& operator << (const float&a)                 {meta_primitive( "float", get_type(a) ); return *this;}
    metastream& operator << (const double&a)                {meta_primitive( "double", get_type(a) ); return *this;}
    metastream& operator << (const long double&a)           {meta_primitive( "long double", get_type(a) ); return *this;}


    metastream& operator << (const char * const&a)          {meta_array(); meta_primitive( "char", bstype::t_type<char>() ); return *this;}
    //metastream& operator << (const unsigned char* const&a)  {meta_primitive( "const unsigned char *", binstream::t_type<char>() ); return *this;}

    metastream& operator << (const timet&a)                 {meta_primitive( "time", get_type(a) ); return *this;}

    metastream& operator << (const opcd&)                   {meta_primitive( "opcd", bstype::t_type<opcd>() ); return *this;}

    metastream& operator << (const charstr&a)               {meta_array(); meta_primitive( "char", bstype::t_type<char>() ); return *this;}
    metastream& operator << (const token&a)                 {meta_array(); meta_primitive( "char", bstype::t_type<char>() ); return *this;}

    /// arrays:
    //template <class T>
    //metastream& operator << (const T* p)                    {set_array(); return *this << *p;}


    
    ////////////////////////////////////////////////////////////////////////////////
    //@{ meta_* functions deal with building the description tree
protected:
    MetaDesc::Var* meta_find( const token& name )
    {
        MetaDesc* d = _map.find(name);
        if(!d)
            return 0;

        return meta_fill_parent_variable(d);
    }

    bool meta_insert( const token& name )
    {
        _last_var = meta_find(name);
        if( _last_var ) {
            meta_exit();
            return true;
        }

        MetaDesc* d = _map.create( name, type(), _cur_streamfrom_fnc, _cur_streamto_fnc );

        _current_var = meta_fill_parent_variable(d);
        _map.push( _current_var );

        return false;
    }

    ///Insert array when its elements are arrays too
    void meta_insert_array( uints n )
    {
        MetaDesc* d = _map.create_array_desc( n, _cur_streamfrom_fnc, _cur_streamto_fnc );

        _current_var = meta_fill_parent_variable(d);
        _map.push( _current_var );
    }

    MetaDesc::Var* meta_fill_parent_variable( MetaDesc* d, bool is_hidden=false )
    {
        MetaDesc::Var* var;

        //remember the first descriptor, it's the root type requested for streaming
        if(!_root.desc)
        {
            _root.desc = d;
            var = &_root;
        }
        else
            var = _current_var->add_child( d, _cur_variable_name );

        _cur_variable_name = 0;

        return var;
    }

    bool is_template_name_mode()            { return _templ_name_stack.size() > 0; }
    
    bool handle_template_name_mode( const char* name )
    {
        charstr& k = *_templ_name_stack.last();

        if(_templ_arg_rdy)
        {
            charstr targ;
            targ.takeover(k);
            _templ_name_stack.resize(-1);

            _templ_arg_rdy = false;

            if( _templ_name_stack.size() == 0 )
            {
                //final name
                _struct_name = name;
                _struct_name += targ;

                return meta_insert(_struct_name);
            }

            charstr& k1 = *_templ_name_stack.last();
            if( k1.last_char() != '<' )
                k1.append(',');
            k1.append(name);
            k1.append(targ);
        }
        else
        {
            if( k.last_char() != '<' )
                k.append(',');
            k.append(name);
        }

        return true;
    }

public:
    ///Called before metastream << on member variable
    template<class T>
    void meta_variable( const char* varname, const T& dummy )
    {
        _cur_variable_name = varname;
        _cur_streamfrom_fnc = &binstream::streamfunc<T>::from_stream;
        _cur_streamto_fnc = &binstream::streamfunc<T>::to_stream;
    }

    ///Called before metastream << on member variable
    template<class T>
    void meta_variable( const char* varname )
    {
        _cur_variable_name = varname;
        _cur_streamfrom_fnc = &binstream::streamfunc<T>::from_stream;
        _cur_streamto_fnc = &binstream::streamfunc<T>::to_stream;
    }

    template<class T>
    void meta_cache_default( const T& var, const T& defval )
    {
        _curvar.var = _last_var;

        _current = _cachestack.push();
        _current->buf = &_curvar.var->defval;
        _current->offs = 0;

        //insert a dummy address field
        _current->set_addr( _current->insert_address(), sizeof(uints) );

        _tmetafnc = &metacall<T>::stream;

        movein_current<WRITE_MODE>(true);
        _hook << defval;

        _cachestack.pop();
        _current = _cachestack.last();

        _tmetafnc = 0;
        _curvar.var = 0;
    }

    bool meta_struct_open( const char* name )
    {
        if( is_template_name_mode() )
            return handle_template_name_mode(name);

        return meta_insert(name);
    }

    void meta_struct_close()
    {
        _last_var = _map.pop();
        _current_var = _map.last();

        meta_exit();
    }

    void meta_template_open()
    {
        charstr& k = *_templ_name_stack.add();
        k.append('<');
    }

    void meta_template_close()
    {
        charstr& k = *_templ_name_stack.last();
        k.append('>');

        _templ_arg_rdy = true;
    }

    ///Signal that the primitive or compound type coming is an array
    ///@param n array element count, UMAX if unknown or varying
    void meta_array( uints n = UMAX )
    {
        meta_insert_array(n);
    }

    ///Only for primitive types
    void meta_primitive( const char* type_name, type t )
    {
        DASSERT( t.is_primitive() );

        //if we are in template name assembly mode, take the type name and get out
        if( is_template_name_mode() )
        {
            handle_template_name_mode(type_name);
            return;
        }

        MetaDesc* d = _map.find_or_create( type_name, t, _cur_streamfrom_fnc, _cur_streamto_fnc );
        _last_var = meta_fill_parent_variable(d);

        meta_exit();
    }

    ///Get back from multiple array decl around current type
    void meta_exit()
    {
        while( _current_var && _current_var->is_array() ) {
            _last_var = _map.pop();
            _current_var = _map.last();
        }
    }

    //@} meta_*

    ///Get type descriptor for given type
    template<class T>
    const MetaDesc::Var* get_type_desc( const T* )
    {
        _root.desc = 0;
        _current_var = 0;

        *this << *(const T*)0;     // build description
        return &_root;
    }

    template<class T>
    struct TypeDesc {
        static const MetaDesc::Var* get(metastream& meta)   { return meta.get_type_desc( (const T*)0 ); }
        static charstr get_str(metastream& meta)
        {
            const MetaDesc::Var* var = meta.get_type_desc( (const T*)0 );
            charstr res;
            var->type_string(res);
            return res;
        }
    };

    const MetaDesc* get_type_info( const token& type ) const
    {
        return _map.find(type);
    }

    void get_type_info_all( dynarray<const MetaDesc*>& dst )
    {
        return _map.get_all_types(dst);
    }

private:

    ///Internal binstream class that linearizes out-of-order input data for final
    /// streaming
    class binstreamhook : public binstream
    {
        metastream* _meta;
    public:
        binstreamhook() : _meta(0) {}

        void set_meta( metastream& m ) {_meta = &m;}

        virtual opcd write( const void* p, type t )             { return _meta->data_write( p, t ); }
        virtual opcd read( void* p, type t )                    { return _meta->data_read( p, t ); }

        virtual opcd write_raw( const void* p, uints& len )     { return _meta->data_write_raw(p,len); }
        virtual opcd read_raw( void* p, uints& len )            { return _meta->data_read_raw(p,len); }

        virtual opcd write_array_content( binstream_container& c, uints* count ) {
            return _meta->data_write_array_content(c,count);
        }

        virtual opcd read_array_content( binstream_container& c, uints n, uints* count ) {
            return _meta->data_read_array_content(c,n,count);
        }

        opcd generic_read_array_content( binstream_container& c, uints n, uints* count ) {
            return binstream::read_array_content(c,n,count);
        }

        virtual opcd write_array_separator( type t, uchar end ) { return _meta->data_write_array_separator(t,end); }
        virtual opcd read_array_separator( type t )             { return _meta->data_read_array_separator(t); }

        virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) {return ersNOT_IMPLEMENTED;}
        virtual opcd bind( binstream& bin ) {return ersNOT_IMPLEMENTED;}

        virtual bool is_open() const                            { return _meta != 0; }
        virtual void flush()                                    { _meta->stream_flush(); }
        virtual void acknowledge( bool eat=false )              { _meta->stream_acknowledge(eat); }

        virtual void reset_read()                               { _meta->stream_reset(1, _meta->cache_prepared()); }
        virtual void reset_write()                              { _meta->stream_reset(-1, _meta->cache_prepared()); }

        virtual uint binstream_attributes( bool in0out1 ) const {return 0;} //fATTR_SIMPLEX
    };

    friend class binstreamhook;


    ////////////////////////////////////////////////////////////////////////////////
    ///Interface for holding built descriptors
    struct StructureMap
    {
        MetaDesc* find( const token& k ) const;
        MetaDesc* create_array_desc( uints n, binstream::fnc_from_stream fnfrom, binstream::fnc_to_stream fnto );

        MetaDesc* create( const token& n, type t, binstream::fnc_from_stream fnfrom, binstream::fnc_to_stream fnto )
        {
            MetaDesc d(n);
            d.btype = t;
            d.stream_from = fnfrom;
            d.stream_to = fnto;
            return insert(d);
        }

        MetaDesc* find_or_create( const token& n, type t, binstream::fnc_from_stream fnfrom, binstream::fnc_to_stream fnto )
        {
            MetaDesc* d = find(n);
            return d ? d : create( n, t, fnfrom, fnto );
        }

        void get_all_types( dynarray<const MetaDesc*>& dst ) const;

        MetaDesc::Var* last() const         { MetaDesc::Var** p = _stack.last();  return p ? *p : 0; }
        MetaDesc::Var* pop()                { MetaDesc::Var* p;  return _stack.pop(p) ? p : 0; }
        void push( MetaDesc::Var* v )       { _stack.push(v); }

        StructureMap();
        ~StructureMap();

    protected:
        MetaDesc* insert( const MetaDesc& v );

        dynarray<MetaDesc::Var*> _stack;
        void* pimpl;
    };

    ///
    template<class T>
    struct metacall {
        static metastream& stream( metastream& m, void* t )
        {   return m << *(const T*)t;   }
    };

    typedef metastream& (*type_metacall)( metastream&, void* );


    ////////////////////////////////////////////////////////////////////////////////
    type_metacall _tmetafnc;

    StructureMap _map;

    charstr _err;
    
    // description parsing:
    MetaDesc::Var _root;
    MetaDesc::Var* _current_var;
    MetaDesc::Var* _last_var;
    const char* _cur_variable_name;
    binstream::fnc_from_stream _cur_streamfrom_fnc;
    binstream::fnc_to_stream _cur_streamto_fnc;

    int _sesopen;                       ///< flush(>0) or ack(<0) session currently open

    ///Entry for variable stack
    struct VarEntry {
        MetaDesc::Var* var;             ///< currently processed variable
        int kth;                        ///< its position within parent
    };

    dynarray<VarEntry> _stack;          ///< stack for current variable

    VarEntry _curvar;                   ///< currently processed variable (read/write)            

    charstr _rvarname;                  ///< name of variable that follows in the input stream
    charstr _struct_name;               ///< used during the template name building step

    dynarray<charstr> _templ_name_stack;
    bool _templ_arg_rdy;
 
    bool _beseparator;                  ///< true if separator between members should be read or written

    bool _disable_meta_write;           ///< disable meta functionality for writting
    bool _disable_meta_read;            ///< disable meta functionality for reading

    binstream* _fmtstream;              ///< bound formatting front-end binstream
    binstreamhook _hook;                ///< internal data binstream

    dynarray<uchar> _cache;             ///< cache for unordered input data or written data in the write-to-cache mode

    //workaround for M$ compiler, instantiate dynarray<uint> first so it doesn't think these are the same
    typedef dynarray<uint>              __Tdummy;

    ///Helper struct for cache traversal
    struct CacheEntry
    {
        dynarray<uchar>* buf;           ///< cache buffer associated with the cache entry
        uints offs;                     ///< offset to the current entry in stored class table
        uints ofsz;                     ///< offset to the count field (only for arrays)

        CacheEntry() : buf(0), offs(UMAX), ofsz(UMAX) {}

        uints size() const              { return buf->size(); }

        //@{ return ptr to data pointed to by the cache entry
        const uchar* data() const       { return buf->ptr() + offs; }
        uchar* data()                   { return buf->ptr() + offs; }

        const uchar* data( uints o ) const { return buf->ptr() + o; }
        uchar* data( uints o )          { return buf->ptr() + o; }
        //@}

        //@{ return ptr to data pointed to indirectly by the cache entry
        const uchar* indirect() const   { uints k = addr();  DASSERT(k<size());  return buf->ptr() + k; }
        uchar* indirect()               { uints k = addr();  DASSERT(k<size());  return buf->ptr() + k; }
        //@}

        ///Retrieve address (offset) stored at the current offset
        uints addr() const              { return offs + *(const uints*)(buf->ptr() + offs); }
        uints addr( uints v ) const     { return v + *(const uints*)(buf->ptr() + v); }

        ///Set address (offset) at the current offset 
        void set_addr( uints v )        { *(uints*)(buf->ptr() + offs) = v - offs; }
        void set_addr( uints adr, uints v ) { *(uints*)(buf->ptr() + adr) = v - adr; }

        bool valid_addr() const         { return offs != UMAX  &&  0 != *(const uints*)(buf->ptr() + offs); }
        bool valid_addr( uints adr ) const  { return 0 != *(const uints*)(buf->ptr() + adr); }

        ///Extract offset-containing field, moving to the next entry
        uints extract_offset()
        {
            uints v = addr();
            offs += sizeof(uints);
            return v;
        }

        void insert_offset( uints v )
        {
            set_addr(v);
            offs += sizeof(uints);
        }

        uints get_asize() const         { return *(const uints*)data(ofsz); }
        void set_asize( uints n )       { *(uints*)data(ofsz) = n; }

        ///Extract size-containing field, moving to the next entry
        void extract_asize_field()
        {
            ofsz = offs;
            offs += sizeof(uints);
        }

        ///Insert size-containing field
        uints* insert_asize_field()
        {
            ofsz = insert_void( sizeof(uints) );
            return (uints*)data(ofsz);
        }

        uints insert_table( uints n )
        {
            uints k = buf->size();
            buf->addc( n*sizeof(uints) );
            return k;
        }

        ///Retrieve value stored at given address
        template <class T>
        const T& extract( uints o ) const { return *(const T*)(buf->ptr() + o); }

        ///Pad cache to specified granularity (but not greater than 8 bytes)
        uints pad( uints size = sizeof(uints) )
        {
            uints k = buf->size();
            uints t = align_offset( k, size>8 ? 8 : size );
            buf->add( t - k );
            return t;
        }

        ///Allocate space in the buffer, aligning the buffer position according to the \a size
        ///@return position where inserted
        uints insert_void( uints size )
        {
            uints k = pad();

            buf->add(size);
            return k;
        }

        ///Append a field that will contain an address (offset)
        ///@note expects padded data
        uints insert_address()
        {
            uints k = buf->size();
            DASSERT( k%sizeof(uints) == 0 );    //should be padded

            buf->add( sizeof(uints) );
            return k;
        }

        ///Prepare for insertion of a member entry
        void insert_member()            { insert_offset( pad() ); }

        ///Prepare for extraction of a member entry
        uints extract_member()          { return extract_offset(); }

        ///Read cache entry data, moving to the next cache entry
        void read_cache( void* p, uints size )
        {
            const uchar* src = indirect();
            offs += sizeof(uints);

            ::memcpy( p, src, size );
        }

        ///Allocate cache entry data, moving to the next cache entry
        void* alloc_cache( uints size )
        {
            uints of = insert_void(size);
            set_addr(of);
            offs += sizeof(uints);
            return data(of);
        }
    };

    dynarray<CacheEntry>
        _cachestack;                    ///< cache table stack
    CacheEntry* _current;               ///< currently processed cache entry

    MetaDesc::Var* _cachequit;          ///< root variable read from cache
    MetaDesc::Var* _cachevar;           ///< variable being cached from input
    MetaDesc::Var* _cacheskip;          ///< the variable not present in input, that will be filled with default

    uints _cachelevel;                  ///< level where the cache was initialized
    uints _cacheentries;                ///< no.of valid members in cache (for fast discarding of cache when everything was read)

private:

    ////////////////////////////////////////////////////////////////////////////////
    MetaDesc::Var* last_var() const     { return _stack.last()->var; }
    void pop_var()                      { EASSERT( _stack.pop(_curvar) ); }
    void push_var()                     { _stack.push(_curvar);  _curvar.var = _curvar.var->desc->first_child();  _curvar.kth = 0; }

    MetaDesc::Var* parent_var() const
    {
        VarEntry* v = _stack.last();
        return v ? v->var : 0;
    }

    bool is_first_var() const           { return _curvar.kth == 0; }


    charstr& dump_stack( charstr& dst, int depth, MetaDesc::Var* var=0 ) const
    {
        if(!dst.is_empty())
            dst << char('\n');

        if( depth <= 0 )
            depth = (int)_stack.size() + depth;

        if( depth > (int)_stack.size() )
            depth = (int)_stack.size();

        for( int i=0; i<depth; ++i )
        {
            //dst << char('/');
            _stack[i].var->dump(dst);
        }

        if(var) {
            //dst << char('/');
            var->dump(dst);
        }
        return dst;
    }

    ////////////////////////////////////////////////////////////////////////////////
protected:

    enum { WRITE_MODE=0, READ_MODE=1 };

    template<int R>
    void movein_cache_member()
    {
        CacheEntry* ce = _cachestack.push();
        _current = ce-1;

        bool cachewrite = !R || _cachevar;

        //_current->offs points to the actual offset to member data if we are nested in another struct
        // or it is the offset to member data itself if we are in array
        if( _curvar.var->is_array_element() )
        {
            ce->buf = _current->buf;
            ce->offs = cachewrite ? _current->pad() : _current->offs;
        }
        else if(cachewrite)
        {
            _current->insert_member();

            ce->buf = _current->buf;
            ce->offs = _current->pad();
        }
        else
        {
            //check if the member was cached or use the default value cache instead
            if( !_current->valid_addr() ) {
                DASSERT( _curvar.var->has_default() );
                ce->buf = &_curvar.var->defval;
                ce->offs = sizeof(uints);
            }
            else {
                uints v = _current->extract_member();

                ce->buf = _current->buf;
                ce->offs = v;

                DASSERT( ce->offs <= ce->size() );
            }
        }

        _current = ce;
    }

    template<int R>
    opcd movein_struct()
    {
        bool cache = cache_prepared();

        if( !cache || _cachevar )
        {
            bool nameless = _curvar.var->nameless_root;
            opcd e = R
                ? _fmtstream->read_struct_open( nameless, &_curvar.var->desc->type_name )
                : _fmtstream->write_struct_open( nameless, &_curvar.var->desc->type_name );

            if(e) {
                dump_stack(_err,0);
                _err << " - error " << (R?"reading":"writing") << " struct opening token";
                throw e;
                return e;
            }
        }

        if(cache)
        {
            movein_cache_member<R>();

            //append member offset table
            if( !R || _cachevar )
                _current->insert_table( _curvar.var->desc->num_children() );
        }

        push_var();
        return 0;
    }

    template<int R>
    opcd moveout_struct()
    {
        bool cache = cache_prepared();

        pop_var();

        if( !cache || _cachevar )
        {
            bool nameless = _curvar.var->nameless_root;
            opcd e = R
                ? _fmtstream->read_struct_close( nameless, &_curvar.var->desc->type_name )
                : _fmtstream->write_struct_close( nameless, &_curvar.var->desc->type_name );

            if(e) {
                dump_stack(_err,0);
                _err << " - error " << (R?"reading":"writing") << " struct closing token";
                throw e;
                return e;
            }
        }

        if(cache)
            _current = _cachestack.pop();

        return 0;
    }


    template<int R>
    opcd movein_process_key()
    {
        bool ary = _curvar.var->is_array_element();
        if(ary)  return 0;

        //this is the first member so no member separator
        return R
            ? fmts_or_cache_read_key()
            : fmts_or_cache_write_key();
    }

    ///Move onto the nearest primitive or array descendant
    //@param R read(1) or write(0) mode
    template<int R>
    opcd movein_current( bool key )
    {
        if(key)
            movein_process_key<R>();

        while( _curvar.var->is_compound() )
        {
            movein_struct<R>();
            movein_process_key<R>();
        }

        return 0;
    }

    template<int R>
    opcd movein_array()
    {
        push_var();
        return movein_current<R>(false);
    }

    ///Traverse the tree and set up the next target for input streaming
    //@param R reading(1) or writing(0) mode
    template<int R>
    opcd moveto_expected_target()
    {
        //find what should come next
        opcd e;
        MetaDesc::Var* cvar;

        do {
            if( _curvar.var == _cachevar ) {
                //_cachevar = 0;
                _current->offs = UMAX;
                return 0;
            }
            else if( _curvar.var == _cachequit )
                invalidate_cache_entry();

            //get next var
            MetaDesc::Var* par = parent_var();
            if(!par) {
                _tmetafnc = 0;
                return 0;
            }

            cvar = par->desc->next_child(_curvar.var);
            if(!cvar)
            {
                //we are in the (last) child variable, the parent could have been only
                // a compound or an array.
                //arrays have their terminating tokens read elsewhere, but for the
                // structs we have to read the closing token here
                if( par->is_compound() )
                    moveout_struct<R>();
                else {
                    DASSERT( par->is_array() );
                    pop_var();

                    return 0;   //exit from the arrays here, they have their own array-end control tokens
                }
            }
        }
        while(!cvar);

        _curvar.var = cvar;
        movein_process_key<R>();

        return _cacheskip
            ? opcd(0)
            : movein_current<R>(false);
    }


    ///This is called from internal binstream when a primitive data or control token is
    /// written. Possible type can be a primitive one, T_STRUCTBGN or T_STRUCTEND, 
    /// or array cotrol tokens
    opcd data_write( const void* p, type t )
    {
        DASSERT( !t.is_array_control_type() || !_curvar.var || _curvar.var->is_array() );

        if(!_tmetafnc)
        {
            if( !t.is_array_end() && _beseparator ) {
                opcd e = _fmtstream->write_separator();
                if(e) return e;
            }
            else
                _sesopen = 1;

            _beseparator = !t.is_array_start();

            return _fmtstream->write(p,t);
        }

        uchar t2 = t.type;
        //ignore the struct open and close tokens nested in binstream operators,
        // as we follow those within metastream operators instead
        if( t2 == type::T_STRUCTBGN  ||  t2 == type::T_STRUCTEND )
            return 0;

        //write value
        opcd e = fmts_or_cache<WRITE_MODE>( (void*)p, t );
        if(e) return e;

        if( t.is_array_start() )
            return 0;

        return moveto_expected_target<WRITE_MODE>();
    }

    ///This is called from internal binstream when a primitive data or control token is
    /// read. 
    opcd data_read( void* p, type t )
    {
        DASSERT( !t.is_array_control_type() || !_curvar.var || _curvar.var->is_array() );

        if(!_tmetafnc)
        {
            if( !t.is_array_end() && _beseparator )
            {
                opcd e = _fmtstream->read_separator();
                if(e) {
                    dump_stack(_err,0);
                    _err << " - error reading separator: " << opcd_formatter(e);
                    throw e;
                    return e;
                }
            }
            else
                _sesopen = -1;

            _beseparator = !t.is_array_start();

            return _fmtstream->read(p,t);
        }

        uchar t2 = t.type;

        //ignore the struct open and close tokens nested in binstream operators,
        // as we follow those within metastream operators instead
        if( t2 == type::T_STRUCTBGN  ||  t2 == type::T_STRUCTEND )
            return 0;

        //read value
        opcd e = fmts_or_cache<READ_MODE>( p, t );
        if(e) {
            dump_stack(_err,0);
            _err << " - error reading variable '" << _curvar.var->varname << "', error: " << opcd_formatter(e);
            throw e;
            return e;
        }

        if( t.is_array_start() )
            return 0;
        else if( t.is_array_end() )
        {
            if( _curvar.var == _cachequit )  invalidate_cache_entry();
            //if( _curvar.var == &_root )      { _tmetafnc = 0;  return 0; }
        }

        return moveto_expected_target<READ_MODE>();
    }

    opcd data_write_raw( const void* p, uints& len )
    {
        return _fmtstream->write_raw( p, len );
    }

    opcd data_read_raw( void* p, uints& len )
    {
        if(_cachevar)
            p = _current->data( _current->insert_void(len) );

        return _fmtstream->read_raw( p, len );
    }

    opcd data_write_array_content( binstream_container& c, uints* count )
    {
        c.set_array_needs_separators();
        type tae = c._type.get_array_element();
        uints n = c._nelements;

        opcd e=0;
        if( !tae.is_primitive() )
        {
            if( cache_prepared() )  //cached compound array
            {
                //write to cache
                DASSERT( !_curvar.var->is_primitive() );

                uints prevoff, i;
                for( i=0; i<n; ++i )
                {
                    const void* p = c.extract(1);
                    if(!p)
                        break;

                    //compound objects stored in an array are prefixed with offset past their body
                    // off will thus contain offset to the next element
                    uints off = _current->pad();
                    if(i>0)
                        _current->set_addr( prevoff, off );
                    prevoff = _current->insert_address();

                    movein_array<WRITE_MODE>();

                    e = c._stream_out( _hook, (void*)p, c );
                    if(e)  break;
                }

                if(i>0) {
                    uints off = _current->pad();
                    _current->set_addr( prevoff, off );
                }

                *count = i;
            }
            else                    //uncached compound array
                e = _hook.write_compound_array_content(c,count);
        }
        else
            e = _fmtstream->write_array_content(c,count);

        return e;
    }

    opcd data_read_array_content( binstream_container& c, uints n, uints* count )
    {
        c.set_array_needs_separators();
        type tae = c._type.get_array_element();

        opcd e=0;
        if( !tae.is_primitive() )     //handles arrays of compound objects
        {
            if( cache_prepared() )  //cached compound array
            {
                //reading from cache
                DASSERT( _cachevar  ||  n != UMAX );
                DASSERT( !_curvar.var->is_primitive() );

                uints i, prevoff;
                for( i=0; i<n; ++i )
                {
                    if( _cachevar ) {
                        if( !read_array_separator(tae) )
                            break;
                        type::mask_array_element_first_flag(tae);
                    }

                    void* p = c.insert(1);
                    if(!p)
                        return ersNOT_ENOUGH_MEM;

                    //compound objects stored in an array are prefixed with offset past their body
                    // off will thus contain offset to the next element
                    uints off;
                    if(_cachevar) {
                        off = _current->pad();
                        if(i>0)
                            _current->set_addr( prevoff, off );
                        prevoff = _current->insert_address();
                    }
                    else
                        off = _current->extract_offset();

                    movein_array<READ_MODE>();

                    e = c._stream_in( _hook, p, c );
                    if(e)  break;

                    //set offset to the next array element
                    if(!_cachevar)
                        _current->offs = off;
                }

                if( _cachevar  &&  i>0 ) {
                    uints off = _current->pad();
                    _current->set_addr( prevoff, off );
                }

                *count = i;
            }
            else                    //uncached compound array
                e = _hook.read_compound_array_content(c,n,count);
        }
        else if( cache_prepared() ) //cache with a primitive array
        {
            if( !_cachevar  &&  c.is_continuous()  &&  n != UMAX )
            {
                uints na = n * tae.get_size();
                xmemcpy( c.insert(n), _current->data(), na );

                _current->offs += na;
                *count = n;
            }
            else
                e = _hook.generic_read_array_content(c,n,count);
        }
        else                        //primitive uncached array
            e = _fmtstream->read_array_content(c,n,count);

        return e;
    }

    ///
    opcd data_write_array_separator( type t, uchar end )
    {
        if(!_tmetafnc)
            _beseparator = false;

        write_array_separator(t,end);

        return end ? opcd(0) : movein_array<WRITE_MODE>();
    }

    ///Called from binstream to read array separator or detect an array end
    ///This method is called only for uncached data; reads from the cache
    /// use the data_read_array_content() method.
    opcd data_read_array_separator( type t )
    {
        if(!_tmetafnc)
            _beseparator = false;

        if( !read_array_separator(t) )
            return ersNO_MORE;

        return movein_array<READ_MODE>();
    }

    ///
    void write_array_separator( type t, uchar end )
    {
        if( cache_prepared() )
            return;

        opcd e = _fmtstream->write_array_separator(t,end);

        if(e) {
            dump_stack(_err,0);
            _err << " - error writing array separator: " << opcd_formatter(e);
            throw e;
        }
    }

    ///@return false if no more elements
    bool read_array_separator( type t )
    {
        if( cache_prepared() && !_cachevar )
            return true;

        opcd e = _fmtstream->read_array_separator(t);
        if( e == ersNO_MORE )
            return false;

        if(e) {
            dump_stack(_err,0);
            _err << " - error reading array separator: " << opcd_formatter(e);
            throw e;
        }

        return true;
    }


    bool cache_prepared() const         { return _current->offs != UMAX; }


    void invalidate_cache_entry()
    {
        if( --_cacheentries == 0 ) {
            _cache.reset();
        }
        _current->offs = UMAX;
        _cachequit = 0;
    }


    ///Read data from formatstream or cache
    template<int R>
    opcd fmts_or_cache( void* p, bstype::kind t )
    {
        opcd e=0;
        if( cache_prepared() )
        {
            if( t.is_array_start() )
            {
                movein_cache_member<R>();

                if( !R || _cachevar ) {
                    *_current->insert_asize_field() = UMAX;

                    if(_cachevar)
                        e = _fmtstream->read(p,t);
                }
                else {
                    _current->extract_asize_field();

                    uints n = _current->get_asize();
                    DASSERT( n != UMAX  &&  n != 0xcdcdcdcd );

                    *(uints*)p = n;
                }
            }
            else if( t.is_array_end() )
            {
                if( !R || _cachevar ) {
                    if(_cachevar)
                        e = _fmtstream->read(p,t);

                    uints n = *(const uints*)p;
                    DASSERT( n != UMAX  &&  n != 0xcdcdcdcd );

                    _current->set_asize(n);
                }
                else {
                    if( _current->get_asize() != *(const uints*)p )
                        return ersMISMATCHED "elements left in cached array";
                }

                _current = _cachestack.pop();
            }
            else    //data reads
            {
                //only primitive data here
                DASSERT( t.is_primitive() );
                DASSERT( t.type != type::T_STRUCTBGN  &&  t.type != type::T_STRUCTEND );
                DASSERT( _cachevar || !_curvar.var->is_array_element() );

                uints tsize = t.get_size();

                if( R && _cachevar )
                    e = _fmtstream->read( _current->alloc_cache(tsize), t );
                else if(R)
                    _current->read_cache( p, tsize );
                else
                    ::memcpy( _current->alloc_cache(tsize), p, tsize );
            }
        }
        else
            e = R ? _fmtstream->read(p,t) : _fmtstream->write(p,t);

        return e;
    }

    ///Read key from input
    opcd fmts_read_key()
    {
        opcd e;
        if( !is_first_var() )
        {
            e = _fmtstream->read_separator();
            if(e==ersNO_MORE)
                return e;

            if(e) {
                dump_stack(_err,0);
                _err << " - error reading the variable separator: " << opcd_formatter(e);
                throw e;
            }
        }

        e = binstream_read_key( *_fmtstream, _rvarname );
        if( e  &&  e!=ersNO_MORE ) {
            dump_stack(_err,0);
            _err << " - error while seeking for variable '" << _curvar.var->varname << "': " << opcd_formatter(e);
            throw e;
        }

        ++_curvar.kth;

        return e;
    }

    ///
    opcd fmts_or_cache_read_key()
    {
        if(_cachevar) {
            //it's been already cached (during caching)
            if( _current->valid_addr() ) {
                _cacheskip = _curvar.var;
                return 0;
            }
        }
        //already reading from the cache or the required key has been found in the cache
        else if( cache_prepared() || cache_toplevel_lookup() )
            return 0;

        opcd e;
        bool outoforder;

        do {
            e = fmts_read_key();
            if(e) {
                DASSERT( e == ersNO_MORE );

                if(_cachevar)
                    _cacheskip = _curvar.var;
                else if( !cache_use_default() ) {
                    dump_stack(_err,0);
                    _err << " - variable '" << _curvar.var->varname << "' not found and no default value provided";
                    throw e;
                }
                break;
            }

            //cache the next member if the key read belongs to an out of the order sibling
            outoforder = _rvarname != _curvar.var->varname;
            if(outoforder)
            {
                if( _curvar.var == &_root )
                    cache_fill_root();
                else
                    cache_fill_member();

                //return fmts_or_cache_read_key();
            }
        }
        while(outoforder);

        return 0;
    }

    ///
    opcd fmts_or_cache_write_key()
    {
        if( cache_prepared() )
            return 0;

        opcd e;
        if( !is_first_var() )
        {
            e = _fmtstream->write_separator();
            if(e) {
                dump_stack(_err,0);
                _err << " - error writing the variable separator: " << opcd_formatter(e);
                throw e;
            }
        }

        e = _fmtstream->write_key( _curvar.var->varname );
        if(e) {
            dump_stack(_err,0);
            _err << " - error while writing the variable name '" << _curvar.var->varname << "': " << opcd_formatter(e);
            throw e;
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
        MetaDesc* par = parent_var()->desc;

        uints base;
        if( _cache.size() > 0 ) {
            //compute base offset
            uints n = _cachestack.size();
            base = n <= 1
                ? 0
                : _current->offs - par->get_child_pos(_curvar.var)*sizeof(uints);
                //: _current->addr( _cachestack[n-2].offs - sizeof(uints) );
        }
        else {
            //create child offset table for the members of par variable
            _cache.reset();
            _cachelevel = _stack.size();

            _current->insert_table( par->num_children() );
            base = 0;
        }

        MetaDesc::Var* crv = par->find_child(_rvarname);
        if(!crv) {
            dump_stack(_err,0);
            _err << " - member variable: " << _rvarname << " not defined";
            e = ersNOT_FOUND "no such member variable";
            throw e;
            //return e;
        }

        uints k = par->get_child_pos(crv) * sizeof(uints);
        if( _current->valid_addr(base+k) ) {
            dump_stack(_err,0);
            _err << " - data for member: " << _rvarname << " specified more than once";
            e = ersMISMATCHED "redundant member data";
            throw e;
            //return e;
        }

        cache_fill( crv, base + k );
    }

    ///Fill intermediate cache
    void cache_fill_root()
    {
        opcd e;

        DASSERT( _cache.size() == 0 );
        _cachelevel = _stack.size();

        _current->insert_table(1);
        cache_fill( &_root, 0 );
    }

    ///
    void cache_fill( MetaDesc::Var* crv, uints offs )
    {
        MetaDesc::Var* old_var = _curvar.var;
        MetaDesc::Var* old_cvar = _cachevar;
        uints old_offs = _current->offs;

        _current->offs = offs;

        //force reading to cache
        _cachevar = _curvar.var = crv;

        movein_current<READ_MODE>(false);
        streamvar(*crv);

        ++_cacheentries;

        _current->offs = old_offs;
        _cachevar = old_cvar;
        _curvar.var = old_var;
    }


    ///
    struct cache_container : public binstream_container
    {
        const void* extract( uints n )  { return 0; }

        //just fake it as the memory wouldn't be used anyway but it can't be NULL
        void* insert( uints n )         { return (void*)1; }

        bool is_continuous() const      { return true; }    //for primitive types it's continuous


        cache_container( metastream& meta, MetaDesc& desc )
            : binstream_container(desc.array_size, desc.array_type(), 0, &stream_in),
            meta(meta), element(desc.children[0])
        {}

        static opcd stream_in( binstream& bin, void* p, binstream_container& co ) {
            cache_container& me = reinterpret_cast<cache_container&>(co);
            return me.meta.streamvar( me.element );
        }

    protected:
        metastream& meta;
        const MetaDesc::Var& element;
    };


    ///
    opcd streamvar( const MetaDesc::Var& var )
    {
        if( &var == _cacheskip ) {
            _cacheskip = 0;
            return moveto_expected_target<READ_MODE>();
        }

        MetaDesc& desc = *var.desc;
        if( desc.is_primitive() )
            return _hook.read( 0, desc.btype );

        if( desc.is_array() ) {
            cache_container cc( *this, desc );
            return _hook.read_array(cc);
        }

        opcd e=0;
        uints n = desc.children.size();
        for( uints i=0; i<n && !e; ++i )
            e = streamvar( desc.children[i] );

        return e;
    }


    ///Find _curvar.var->varname in cache
    bool cache_toplevel_lookup()
    {
        if( _cache.size() == 0  ||  _cachelevel < _stack.size() )
            return false;

        //get child map
        MetaDesc* par = parent_var()->desc;

        uints k = par->get_child_pos(_curvar.var) * sizeof(uints);
        if( _current->valid_addr(k) )
        {
            //found in the cache, set up a cache read
            _current->offs = k;
            _current->buf = &_cache;
            _cachequit = _curvar.var;

            return true;
        }

        return false;
    }

    ///Setup usage of the default value for reading
    bool cache_use_default()
    {
        if( !_curvar.var->has_default() )  return false;

        DASSERT( _cache.size() > 0 );

        //get child map
        MetaDesc* par = parent_var()->desc;

        //_current->offs = par->get_child_pos(_cur_var) * sizeof(uints);
        _current->offs = 0;
        _current->buf = &_curvar.var->defval;
        _cachequit = _curvar.var;

        //not actually in the primary cache, but increment 
        ++_cacheentries;

        return true;
    }
};

////////////////////////////////////////////////////////////////////////////////
/// struct-related defines:
/**
    @def MM(meta,n,v)   specify member metadata providing member name
    @def MMT(meta,n,t)  specify member metadata providing member type
    @def MMD(meta,n,d)  specify member metadata providing a default value of member type
    @def MMTD(meta,n,d) specify member metadata providing a default value of specified type
    @def MMAT(meta,n,t) specify that member is an array of type \a t
    @def MMAF(meta,n,t,s) specify that member is a fixed size array of type \a t
**/
#define MSTRUCT_OPEN(meta, n)       if( !meta.meta_struct_open(n) ) {
#define MM(meta, n, v)              meta.meta_variable(n,v);    meta << v;
#define MMT(meta, n, t)             meta.meta_variable<t>(n);   meta << *(t*)0;
#define MMD(meta, n, v, d)          meta.meta_variable(n,v);    meta << v;          meta.meta_cache_default(v,d);
#define MMTD(meta, n, t, d)         meta.meta_variable<t>(n);   meta << *(t*)0;     meta.meta_cache_default(*(t*)0,d);
#define MMAT(meta, n, t)            meta.meta_variable<t>(n);   meta.meta_array();  meta << *(t*)0;
#define MMAF(meta, n, t, s)         meta.meta_variable<t>(n);   meta.meta_array(s); meta << *(t*)0;
#define MSTRUCT_CLOSE(meta)         meta.meta_struct_close(); } return meta;

#define MSTRUCT_BEGIN               MSTRUCT_OPEN
#define MSTRUCT_END                 MSTRUCT_CLOSE

/// building template name:
#define MTEMPL_OPEN(meta)           meta.meta_template_open();
#define MT(meta, T)                 meta << (*(T*)0);
#define MTEMPL_CLOSE(meta)          meta.meta_template_close();



///TODO: move to dynarray.h:
template <class T>
metastream& operator << ( metastream& m, const dynarray<T>& )
{
    m.meta_array();
    m << *(T*)0;
    return m;
}



COID_NAMESPACE_END


#endif //__COID_COMM_METASTREAM__HEADER_FILE__
