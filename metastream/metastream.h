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

#include "fmtstream.h"
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
    metastream() {
        init();
    }

    explicit metastream( fmtstream& bin )
    {
        init();
        bind_formatting_stream(bin);
    }

    virtual ~metastream() {}


    typedef bstype::kind            type;

    void init()
    {
        _root.desc = 0;

        _current = _cachestack.need(1);
        _current->buf = &_cache;

        _cacheroot = 0;
        _cachequit = 0;
        _cachedefval = 0;
        _cachevar = 0;
        _cacheskip = 0;

        _sesopen = 0;
        _beseparator = false;
        _current_var = 0;
        _curvar.var = 0;
        _cur_variable_name.set_empty();

        _disable_meta_write = _disable_meta_read = false;
        _templ_arg_rdy = false;

        _hook.set_meta( *this );

        _dometa = 0;
        _fmtstreamwr = _fmtstreamrd = 0;
    }

    ///Bind the same stream to both input and output
    void bind_formatting_stream( fmtstream& bin )
    {
        _fmtstreamwr = _fmtstreamrd = &bin;
        stream_reset( 0, cache_prepared() );
        _sesopen = 0;
        _beseparator = false;
    }

    ///Bind formatting streams for input and output
    void bind_formatting_streams( fmtstream& brd, fmtstream& bwr )
    {
        _fmtstreamwr = &bwr;
        _fmtstreamrd = &brd;
        stream_reset( 0, cache_prepared() );
        _sesopen = 0;
        _beseparator = false;
    }

    fmtstream& get_reading_formatting_stream() const {
        return *_fmtstreamrd;
    }

    fmtstream& get_writing_formatting_stream() const {
        return *_fmtstreamwr;
    }

    ///Return the transport stream
    binstream& get_binstream()          { return _hook; }

    ///Set current file name (for error reporting)
    void set_file_name( const token& name ) {
        file_name = name;
    }

    void enable_meta_write( bool en )   { _disable_meta_write = !en; }
    void enable_meta_read( bool en )    { _disable_meta_read = !en; }

    ////////////////////////////////////////////////////////////////////////////////
    //@{ methods to physically stream the data utilizing the metastream

    template<int R>
    bool prepare_type_common( bool cache )
    {
        stream_reset(0,cache);

        _root.desc = 0;
        _current_var = 0;

        DASSERT( R || _sesopen >= 0 );      //or else pending ack on read
        DASSERT( !R || _sesopen <= 0 );     //or else pending flush on write

        if( _sesopen == 0 ) {
            _err.reset();
            _sesopen = R ? -1 : 1;
            _curvar.kth = 0;
        }

        opcd e=0;
        if( !R && _disable_meta_write ) {
            _dometa = 0;
            if(_curvar.kth)
                e = _fmtstreamwr->write_separator();

            if(e) {
                _err << "error writing separator";
                throw e;
            }
            return false;
        }
        else if( R && _disable_meta_read ) {
            _dometa = 0;
            if(_curvar.kth)
                e = _fmtstreamwr->read_separator();

            if(e) {
                _err << "error reading separator";
                throw e;
            }
            return false;
        }

        _beseparator = false;
        return true;
    }

    template<int R>
    opcd prepare_type_final( const token& name, bool cache )
    {
        _dometa = true;

        _curvar.var = &_root;
        _curvar.var->varname = name;
        _curvar.var->nameless_root = name.is_empty();

        if(cache)
            cache_fill_root();
        else if( _curvar.var->nameless_root  &&  _curvar.var->is_compound() )
            movein_struct<R>();

        return 0;
    }


    template<int R, class T>
    opcd prepare_type( T&, const token& name, bool cache )
    {
        if( !prepare_type_common<R>(cache) )  return 0;

        *this << *(const T*)0;     // build description

        return prepare_type_final<R>(name, cache);
    }

    template<int R>
    opcd prepare_named_type( const token& type, const token& name, bool cache )
    {
        if( !prepare_type_common<R>(cache) )  return 0;

        if( !meta_find(type) )
            return ersNOT_FOUND;

        return prepare_type_final<R>(name,cache);
    }

    template<int R, class T>
    opcd prepare_type_array( T&, uints n, const token& name, bool cache )
    {
        if( !prepare_type_common<R>(cache) )  return 0;

        meta_array(n);
        *this << *(const T*)0;     // build description

        return prepare_type_final<R>(name,cache);
    }


    ///Read object of type T from the currently bound formatting stream
    template<class T>
    opcd stream_in( T& x, const token& name = token::empty() )
    {
        opcd e;
        try {
            e = prepare_type<READ_MODE>( x, name, false );
            if(e) return e;

            _hook >> x;

            if( _root.desc->first_child()->is_compound() )
                moveout_struct<READ_MODE>();
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Read object of type T from the currently bound formatting stream into the cache
    template<class T>
    opcd cache_in( const token& name = token::empty() )
    {
        opcd e;
        try {
            e = prepare_type<READ_MODE>( *(const T*)0, name, true );
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Write object of type T to the currently bound formatting stream
    template<class T>
    opcd stream_out( const T& x, const token& name = token::empty() )
    {
        return stream_or_cache_out( x, false, name );
    }

    ///Write object of type T to the cache
    template<class T>
    opcd cache_out( const T& x, const token& name = token::empty() )
    {
        return stream_or_cache_out( x, true, name );
    }

    ///Write object of type T to the currently bound formatting stream
    ///@param cache true if the object should be trapped in the cache instead of sending it out through the formatting stream
    template<class T>
    opcd stream_or_cache_out( const T& x, bool cache, const token& name = token::empty() )
    {
        opcd e;
        try {
            e = prepare_type<WRITE_MODE>( x, name, cache );
            if(e) return e;

            if(!cache) {
                _hook << x;

                if( _root.desc->first_child()->is_compound() )
                    moveout_struct<WRITE_MODE>();
            }
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    ///Prepare streaming of a named type
    opcd stream_out_named( const token& type, const token& name, bool cache=false )
    {
        return prepare_named_type<WRITE_MODE>( type, name, cache );
    }


    ///Read array of objects of type T from the currently bound formatting stream
    template<class T>
    opcd stream_array_in( binstream_containerT<T>& C, const token& name = token::empty() )
    {
        opcd e;
        try {
            e = prepare_type_array<READ_MODE>( *(const T*)0, C._nelements, name, false );
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
        opcd e;
        try {
            e = prepare_type_array<READ_MODE>( *(const T*)0, n, name, true );
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
        opcd e;
        try {
            e = prepare_type_array<WRITE_MODE>( *(const T*)0, C._nelements, name, cache );
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

            _fmtstreamrd->acknowledge(eat);
        }
    }

    void stream_flush()
    {
        DASSERT( _sesopen >= 0 );
        if( _sesopen > 0 ) {
            _sesopen = 0;
            _beseparator = false;

            _fmtstreamwr->flush();
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

        _dometa = 0;
        _curvar.var = 0;
        _cur_variable_name.set_empty();
        _rvarname.reset();

        _err.reset();
        if( fmts_reset>0 )
            _fmtstreamrd->reset_read();
        else if( fmts_reset<0 )
            _fmtstreamwr->reset_write();
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

    const MetaDesc::Var& get_root_var( const uchar*& cachedata ) const
    {
        cachedata = _cache.ptr() + sizeof(uints);
        return _root;
    }

    const charstr& error_string() const         { return _err; }

    template<class T>
    static type get_type(const T&)              { return bstype::t_type<T>(); }


    metastream& operator << (const bool&a)      {meta_primitive( "bool", get_type(a) ); return *this;}
    metastream& operator << (const int8&a)      {meta_primitive( "int8", get_type(a) ); return *this;}
    metastream& operator << (const uint8&a)     {meta_primitive( "uint8", get_type(a) ); return *this;}
    metastream& operator << (const int16&a)     {meta_primitive( "int16", get_type(a) ); return *this;}
    metastream& operator << (const uint16&a)    {meta_primitive( "uint16", get_type(a) ); return *this;}
    metastream& operator << (const int32&a)     {meta_primitive( "int32", get_type(a) ); return *this;}
    metastream& operator << (const uint32&a)    {meta_primitive( "uint32", get_type(a) ); return *this;}
    metastream& operator << (const int64&a)     {meta_primitive( "int64", get_type(a) ); return *this;}
    metastream& operator << (const uint64&a)    {meta_primitive( "uint64", get_type(a) ); return *this;}

    metastream& operator << (const char&a)      {meta_primitive( "char", get_type(a) ); return *this;}

#ifdef _MSC_VER
    metastream& operator << (const ints&a)      {meta_primitive( "int", get_type(a) ); return *this;}
    metastream& operator << (const uints&a)     {meta_primitive( "uint", get_type(a) ); return *this;}
#else
    metastream& operator << (const long&a)      {meta_primitive( "long", get_type(a) ); return *this;}
    metastream& operator << (const ulong&a)     {meta_primitive( "ulong", get_type(a) ); return *this;}
#endif

    metastream& operator << (const float&a)     {meta_primitive( "float", get_type(a) ); return *this;}
    metastream& operator << (const double&a)    {meta_primitive( "double", get_type(a) ); return *this;}
    metastream& operator << (const long double&a)   {meta_primitive( "long double", get_type(a) ); return *this;}


    metastream& operator << (const char * const&a)  {meta_array(); meta_primitive( "char", bstype::t_type<char>() ); return *this;}
    //metastream& operator << (const unsigned char* const&a)  {meta_primitive( "const unsigned char *", binstream::t_type<char>() ); return *this;}

    metastream& operator << (const timet&a)     {meta_primitive( "time", get_type(a) ); return *this;}

    metastream& operator << (const opcd&)       {meta_primitive( "opcd", bstype::t_type<opcd>() ); return *this;}

    metastream& operator << (const charstr&a)   {meta_array(); meta_primitive( "char", bstype::t_type<char>() ); return *this;}
    metastream& operator << (const token&a)     {meta_array(); meta_primitive( "char", bstype::t_type<char>() ); return *this;}

    
    ////////////////////////////////////////////////////////////////////////////////
    //@{ meta_* functions deal with building the description tree
protected:

    MetaDesc::Var* meta_fill_parent_variable( MetaDesc* d, bool is_hidden=false )
    {
        MetaDesc::Var* var;

        //remember the first descriptor, it's the root type requested for streaming
        if(!_root.desc) {
            _root.desc = d;
            var = &_root;
        }
        else
            var = _current_var->add_child( d, _cur_variable_name );

        _cur_variable_name.set_empty();

        return var;
    }

    bool meta_find( const token& name )
    {
        MetaDesc* d = _map.find(name);
        if(!d)
            return false;

        _last_var = meta_fill_parent_variable(d);
        meta_exit();
        return true;
    }

    bool meta_insert( const token& name )
    {
        if( meta_find(name) )
            return true;

        MetaDesc* d = _map.create( name, type(), _cur_streamfrom_fnc, _cur_streamto_fnc );

        _current_var = meta_fill_parent_variable(d);
        _map.push( _current_var );

        return false;
    }


    bool is_template_name_mode() {
        return _templ_name_stack.size() > 0;
    }
    
    bool handle_template_name_mode( const token& name )
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
    ///Define member variable
    template<class T>
    void meta_variable( const token& varname, const T* )
    {
        _cur_variable_name = varname;
        _cur_streamfrom_fnc = &binstream::streamfunc<T>::from_stream;
        _cur_streamto_fnc = &binstream::streamfunc<T>::to_stream;

        *this << *(const T*)0;
    }

    ///Define member array variable
    template<class T>
    void meta_variable_array( const token& varname, const T*, uints n )
    {
        _cur_variable_name = varname;
        _cur_streamfrom_fnc = &binstream::streamfunc<T>::from_stream;
        _cur_streamto_fnc = &binstream::streamfunc<T>::to_stream;

        meta_array(n);

        *this << *(const T*)0;
    }

    ///Define member enum variable
    template<class T>
    void meta_variable_enum( const token& varname, const T* )
    {
        typedef typename EnumType<sizeof(T)>::TEnum TE;

        _cur_variable_name = varname;
        _cur_streamfrom_fnc = &binstream::streamfunc<TE>::from_stream;
        _cur_streamto_fnc = &binstream::streamfunc<TE>::to_stream;

        *this << *(const TE*)0;
    }

    template<class T>
    void meta_variable_pointer( const token& varname, const T* )
    {
        typedef typename type_base<T>::type BT;

        _cur_variable_name = varname;
        _cur_streamfrom_fnc = &binstream::streamfunc<BT>::from_stream;
        _cur_streamto_fnc = &binstream::streamfunc<BT>::to_stream;

        meta_pointer();

        *this << *(T)0;
    }

    template<class T>
    void meta_cache_default( const T* defval )
    {
        _curvar.var = _last_var;

        _current = _cachestack.push();
        _current->buf = &_curvar.var->defval;
        _current->offs = 0;

        //insert a dummy address field
        _current->set_addr( _current->insert_address(), sizeof(uints) );

        _dometa = true;

        //movein_process_key<WRITE_MODE>();
        //movein_current<WRITE_MODE>();
        _hook << *defval;

        _cachestack.pop();
        _current = _cachestack.last();

        _dometa = 0;
        _curvar.var = 0;
    }

    bool meta_struct_open( const token& name )
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
        DASSERT( n != 0 );  //0 is meaningless here, and it's used for pointers elsewhere
        MetaDesc* d = _map.create_array_desc( n, _cur_streamfrom_fnc, _cur_streamto_fnc );

        _current_var = meta_fill_parent_variable(d);
        _map.push( _current_var );
    }

    ///Signal that the primitive or compound type coming is a pointer/reference
    void meta_pointer()
    {
        MetaDesc* d = _map.create_array_desc( 0, _cur_streamfrom_fnc, _cur_streamto_fnc );

        _current_var = meta_fill_parent_variable(d);
        
        //create default value for pointers = a null pointer header
        *(uints*)_current_var->defval.add(sizeof(uints)) = sizeof(uints); //dummy offset
        *_current_var->defval.add() = 0;

        _map.push( _current_var );
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
    const MetaDesc* get_type_desc( const T* )
    {
        _root.desc = 0;
        _current_var = 0;

        *this << *(const T*)0;     // build description
        const MetaDesc* mtd = _root.desc;

        _root.desc = 0;
        return mtd;
    }

    ///
    template<class T>
    struct TypeDesc {
        static const MetaDesc* get( metastream& meta )
        {
            return meta.get_type_desc( (const T*)0 );
        }

        static charstr get_str( metastream& meta )
        {
            const MetaDesc* dsc = meta.get_type_desc( (const T*)0 );
            
            charstr res;

            dsc->type_string(res);
            return res;
        }
    };

    const MetaDesc* get_type_info( const token& type ) const    { return _map.find(type); }

    void get_type_info_all( dynarray<const MetaDesc*>& dst )    { return _map.get_all_types(dst); }

private:

    ///Internal binstream class that linearizes out-of-order input data for final
    /// streaming
    class binstreamhook : public binstream
    {
        metastream* _meta;
    public:
        binstreamhook() : _meta(0) {}

        void set_meta( metastream& m ) {_meta = &m;}

        virtual opcd write( const void* p, type t )             { return _meta->data_write(p, t); }
        virtual opcd read( void* p, type t )                    { return _meta->data_read(p, t); }

        virtual opcd write_raw( const void* p, uints& len )     { return _meta->data_write_raw(p, len); }
        virtual opcd read_raw( void* p, uints& len )            { return _meta->data_read_raw(p, len); }

        virtual opcd write_array_content( binstream_container& c, uints* count ) {
            return _meta->data_write_array_content(c,count);
        }

        virtual opcd read_array_content( binstream_container& c, uints n, uints* count ) {
            return _meta->data_read_array_content(c,n,count);
        }


        opcd generic_write_array_content( binstream_container& c, uints* count ) {
            return binstream::write_array_content(c,count);
        }

        opcd generic_read_array_content( binstream_container& c, uints n, uints* count ) {
            return binstream::read_array_content(c,n,count);
        }

        virtual opcd write_array_separator( type t, uchar end ) { return _meta->data_write_array_separator(t,end); }
        virtual opcd read_array_separator( type t )             { return _meta->data_read_array_separator(t); }

        virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) {return ersNOT_IMPLEMENTED;}

        virtual opcd peek_read( uint timeout )                  { return 0; }
        virtual opcd peek_write( uint timeout )                 { return 0; }

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

    ////////////////////////////////////////////////////////////////////////////////
    bool _dometa;                       ///< true if shoud stream metadata, false if only the values

    StructureMap _map;
    charstr _err;
    
    // description parsing:
    MetaDesc::Var _root;
    MetaDesc::Var* _current_var;
    MetaDesc::Var* _last_var;

    token _cur_variable_name;
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

    fmtstream* _fmtstreamrd;            ///< bound formatting front-end binstream
    fmtstream* _fmtstreamwr;            ///< bound formatting front-end binstream
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

        CacheEntry()
            : buf(0), offs(UMAX), ofsz(UMAX)
        {}

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
        void set_addr_invalid( uints adr )  { *(uints*)(buf->ptr() + adr) = 0; }

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

        void* insert_void_unpadded( uints size )
        {
            return buf->add(size);
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

    MetaDesc::Var* _cacheroot;          ///< root of the cache, whose members are going to be cached
    MetaDesc::Var* _cachequit;          ///< member variable being read from cache
    MetaDesc::Var* _cachedefval;        ///< variable whose default value is currently being read
    MetaDesc::Var* _cachevar;           ///< variable being currently cached from input
    MetaDesc::Var* _cacheskip;          ///< set if the variable was not present in input (can be filled with default) or has been already cached

    charstr file_name;                  ///< current file name (for error reporting)

private:

    ////////////////////////////////////////////////////////////////////////////////
    MetaDesc::Var* last_var() const     { return _stack.last()->var; }
    void pop_var()                      { EASSERT( _stack.pop(_curvar) ); }

    void push_var() {
        _stack.push(_curvar);
        _curvar.var = _curvar.var->desc->first_child();
        _curvar.kth = 0;
    }

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

    void fmt_error()
    {
        _fmtstreamrd->fmtstream_format_err(_err);
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
                _current->extract_member(); //move offset in parent to next member

                ce->buf = &_curvar.var->defval;
                ce->offs = sizeof(uints);
                //_cachedefval = _curvar.var;
            }
            else {
                uints v = _current->extract_member();   //also moves offset to next

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
                ? _fmtstreamrd->read_struct_open( nameless, &_curvar.var->desc->type_name )
                : _fmtstreamwr->write_struct_open( nameless, &_curvar.var->desc->type_name );

            if(e) {
                dump_stack(_err,0);
                _err << " - error " << (R?"reading":"writing") << " struct opening token\n";
                if(R)
                    fmt_error();
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
                ? _fmtstreamrd->read_struct_close( nameless, &_curvar.var->desc->type_name )
                : _fmtstreamwr->write_struct_close( nameless, &_curvar.var->desc->type_name );

            if(e) {
                dump_stack(_err,0);
                _err << " - error " << (R?"reading":"writing") << " struct closing token";
                if(R)
                    fmt_error();
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
        if( !_rvarname.is_empty() ) {
            DASSERT( _rvarname == _curvar.var->varname );
            _rvarname.reset();
            return 0;
        }

        if( _curvar.var->is_array_element() )
            return 0;

        return R
            ? fmts_or_cache_read_key()
            : fmts_or_cache_write_key();
    }

    ///Move onto the nearest primitive or array descendant
    //@param R read(1) or write(0) mode
    template<int R>
    opcd movein_current()
    {
        opcd e = movein_process_key<R>();

        while( _curvar.var->is_compound() )
        {
            movein_struct<R>();
            DASSERT( _curvar.var );

            e = movein_process_key<R>();
            DASSERT( !e || !_curvar.var->is_compound() );
        }

        return e;
    }

    ///Traverse the tree and set up the next target for input/output streaming
    //@param R reading(1) or writing(0) mode
    template<int R>
    opcd moveto_expected_target()
    {
        //find what should come next
        opcd e;
        MetaDesc::Var* cvar;

        do {
            if( _curvar.var == _cachevar ) {
                //end caching - _cachevar was cached completely
                _current->offs = UMAX;
                return 0;
            }
            else {
                if( _curvar.var == _cachedefval ) {
                    _current = _cachestack.pop();
                    _cachedefval = 0;
                }
                
                if( _curvar.var == _cachequit )
                    invalidate_cache_entry();
            }

            //get next var
            MetaDesc::Var* par = parent_var();
            if(!par) {
                _dometa = 0;
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

                    //exit from the arrays here, they have their own array-end control tokens
                    if( !_curvar.var->is_pointer() )
                        return 0;
                }
            }
        }
        while(!cvar);

        _curvar.var = cvar;
        return 0;
    }


    ///This is called from internal binstream when a primitive data or control token is
    /// written. Possible type can be a primitive one, T_STRUCTBGN or T_STRUCTEND, 
    /// or array cotrol tokens
    opcd data_write( const void* p, type t )
    {
        //DASSERT( !t.is_array_control_type() || !_curvar.var || _curvar.var->is_array() );

        if(!_dometa)
            return data_write_nometa(p, t);

        if( t.type == type::T_OPTIONAL ) {
            if( *(const uint8*)p == 0 )
                return moveto_expected_target<WRITE_MODE>();
        }

        if( !t.is_array_end() )
            movein_current<WRITE_MODE>();

        //ignore the struct open and close tokens nested in binstream operators,
        // as we follow those within metastream operators instead
        if( t.type == type::T_STRUCTBGN  ||  t.type == type::T_STRUCTEND )
            return 0;

        //write value
        opcd e = fmts_or_cache<WRITE_MODE>( (void*)p, t );
        if(e) return e;

        if( t.is_array_start()  ||  t.type == type::T_OPTIONAL )
            return 0;

        return moveto_expected_target<WRITE_MODE>();
    }

    ///This is called from internal binstream when a primitive data or control token is
    /// read. 
    opcd data_read( void* p, type t )
    {
        //DASSERT( !t.is_array_control_type() || !_curvar.var || _curvar.var->is_array() );

        if(!_dometa)
            return data_read_nometa(p, t);

        if( !t.is_array_end() ) {
            //move to the first primitive member in input, caching the unordered siblings
            movein_current<READ_MODE>();
        }

        //ignore the struct open and close tokens nested in binstream operators,
        // as we follow those within metastream operators instead
        if( t.type == type::T_STRUCTBGN  ||  t.type == type::T_STRUCTEND )
            return 0;

        return data_read_value(p, t);
    }

    ///
    opcd data_read_value( void* p, type t )
    {
        //read value
        opcd e = fmts_or_cache<READ_MODE>(p, t);
        if(e) {
            dump_stack(_err,0);
            _err << " - error reading variable '" << _curvar.var->varname << "', error: " << opcd_formatter(e);
            fmt_error();
            throw e;
            return e;
        }

        if( t.type == type::T_OPTIONAL  &&  *(const uint8*)p != 0 ) {
            return 0;
        }
        else if( t.is_array_start() ) {
            return 0;
        }/*
        else if( t.is_array_end() )
        {
            if( _curvar.var == _cachedefval ) {
                _current = _cachestack.pop();
                _cachedefval = 0;
            }
            
            if( _curvar.var == _cachequit )
                invalidate_cache_entry();
        }*/

        return moveto_expected_target<READ_MODE>();
    }

    ///
    opcd data_write_nometa( const void* p, type t )
    {
        if( !t.is_array_end() && _beseparator ) {
            opcd e = _fmtstreamwr->write_separator();
            if(e) return e;
        }
        else
            _sesopen = 1;

        _beseparator = !t.is_array_start();

        return _fmtstreamwr->write(p,t);
    }

    ///
    opcd data_read_nometa( void* p, type t )
    {
        if( !t.is_array_end() && _beseparator )
        {
            opcd e = _fmtstreamrd->read_separator();
            if(e) {
                dump_stack(_err,0);
                _err << " - error reading separator: " << opcd_formatter(e);
                fmt_error();
                throw e;
                return e;
            }
        }
        else
            _sesopen = -1;

        _beseparator = !t.is_array_start();

        return _fmtstreamrd->read(p,t);
    }

    opcd data_write_raw( const void* p, uints& len )
    {
        return _fmtstreamwr->write_raw( p, len );
    }

    opcd data_read_raw( void* p, uints& len )
    {
        if(_cachevar)
            p = _current->data( _current->insert_void(len) );

        return _fmtstreamrd->read_raw( p, len );
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

                    push_var();

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
        else if( cache_prepared() ) //cache with a primitive array
        {
            if( !_cachevar  &&  c.is_continuous()  &&  n != UMAX )
            {
                uints na = n * tae.get_size();
                xmemcpy( _current->insert_void_unpadded(na), c.extract(n), na );

                _current->offs += na;
                *count = n;
            }
            else
                e = _hook.generic_write_array_content(c,count);
        }
        else
            e = _fmtstreamwr->write_array_content(c,count);

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

                    push_var();

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
            e = _fmtstreamrd->read_array_content(c,n,count);

        return e;
    }

    ///
    opcd data_write_array_separator( type t, uchar end )
    {
        if(!_dometa)
            _beseparator = false;

        write_array_separator(t,end);

        if(!end)
            push_var();

        return 0;
    }

    ///Called from binstream to read array separator or detect an array end
    ///This method is called only for uncached data; reads from the cache
    /// use the data_read_array_content() method.
    opcd data_read_array_separator( type t )
    {
        if(!_dometa)
            _beseparator = false;

        if( !read_array_separator(t) )
            return ersNO_MORE;

        push_var();
        return 0;
    }

    ///
    void write_array_separator( type t, uchar end )
    {
        if( cache_prepared() )
            return;

        opcd e = _fmtstreamwr->write_array_separator(t,end);

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

        opcd e = _fmtstreamrd->read_array_separator(t);
        if( e == ersNO_MORE )
            return false;

        if(e) {
            dump_stack(_err,0);
            _err << " - error reading array separator: " << opcd_formatter(e);
            fmt_error();
            throw e;
        }

        return true;
    }


    bool cache_prepared() const {
        return _current->offs != UMAX;
    }


    void invalidate_cache_entry()
    {
        MetaDesc* desc = _stack.last()->var->desc;

        uints k = desc->get_child_pos(_cachequit) * sizeof(uints);
        DASSERT( k!=UMAX  &&  _current->valid_addr(k) );

        _current->set_addr_invalid(k);
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
                        e = _fmtstreamrd->read(p,t);
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
                        e = _fmtstreamrd->read(p,t);

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
                    e = _fmtstreamrd->read( _current->alloc_cache(tsize), t );
                else if(R)
                    _current->read_cache( p, tsize );
                else
                    ::memcpy( _current->alloc_cache(tsize), p, tsize );
            }
        }
        else if( t.type == type::T_OPTIONAL ) {
            if(R) {
                *(uint8*)p = 1;
                push_var();
            }
            else if( *(const uint8*)p )
                push_var();
        }
        else
            e = R ? _fmtstreamrd->read(p,t) : _fmtstreamwr->write(p,t);

        return e;
    }

    ///Read key from input
    opcd fmts_read_key()
    {
        opcd e;
        if( !is_first_var() )
        {
            e = _fmtstreamrd->read_separator();

            if( e  &&  e != ersNO_MORE ) {
                dump_stack(_err,0);
                _err << " - error reading the variable separator: " << opcd_formatter(e);
                fmt_error();
                throw e;
            }
        }

        if(!e)
            e = binstream_read_key( *_fmtstreamrd, _rvarname );

        ++_curvar.kth;

        if( e == ersNO_MORE ) {
            //if reading to cache, make it skip reading the variable
            // error will be dealt with later, or a default value will be used (if existing)
            if(_cachevar)
                _cacheskip = _curvar.var;
            //if normal reading (not a reading to cache), set up defval read or fail
            else if( !cache_use_default() ) {
                dump_stack(_err,0);
                _err << " - variable '" << _curvar.var->varname << "' not found and no default value provided";
                fmt_error();
                throw e;
            }
        }
        else if(e) {
            dump_stack(_err,0);
            _err << " - error while seeking for variable '" << _curvar.var->varname << "': " << opcd_formatter(e);
            fmt_error();
            throw e;
        }


        return e;
    }

    ///
    //@return ersNO_MORE if no more keys under current compound variable parent
    opcd fmts_or_cache_read_key()
    {
        //if reading to cache ...
        if(_cachevar) {
            //if it's been already cached (during the caching)
            if( _current->valid_addr() ) {
                _cacheskip = _curvar.var;
                return 0;
            }
        }
        //already reading from the cache or the required key has been found in the cache
        else if( cache_prepared() || cache_lookup() )
            return 0;

        opcd e;
        bool outoforder;

        do {
            e = fmts_read_key();
            if(e) {
                //no more members under current compound
                DASSERT( e == ersNO_MORE );
                break;
            }

            //cache the next member if the key read belongs to an out of the order sibling
            outoforder = _rvarname != _curvar.var->varname;
            if(outoforder)
                cache_fill_member();

            _rvarname.reset();
        }
        while(outoforder);

        return e;
    }

    ///
    opcd fmts_or_cache_write_key()
    {
        if( cache_prepared() )
            return 0;

        opcd e;
        if( !is_first_var() )
        {
            e = _fmtstreamwr->write_separator();
            if(e) {
                dump_stack(_err,0);
                _err << " - error writing the variable separator: " << opcd_formatter(e);
                throw e;
            }
        }

        e = _fmtstreamwr->write_key( _curvar.var->varname );
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
        MetaDesc::Var* par = parent_var();

        uints base;
        if( _cache.size()>0  ||  par == _cacheroot ) { //_cache.size() > 0 ) {
            //compute base offset
            uints n = _cachestack.size();
            base = n <= 1
                ? 0
                : _current->offs - par->desc->get_child_pos(_curvar.var)*sizeof(uints);
        }
        else {
            //create child offset table for the members of par variable
            _cache.reset();
            _cacheroot = par;

            _current->buf = &_cache;
            _current->insert_table( par->desc->num_children() );
            base = 0;
        }

        MetaDesc::Var* crv = par->desc->find_child(_rvarname);
        if(!crv) {
            dump_stack(_err,0);
            _err << " - member variable: " << _rvarname << " not defined";
            fmt_error();
            e = ersNOT_FOUND "no such member variable";
            throw e;
            //return e;
        }

        uints k = par->desc->get_child_pos(crv) * sizeof(uints);
        if( _current->valid_addr(base+k) ) {
            dump_stack(_err,0);
            _err << " - data for member: " << _rvarname << " specified more than once";
            fmt_error();
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
        _cacheroot = &_root;

        _current->insert_table(1);
        cache_fill( &_root, sizeof(uints) );
    }

    ///
    void cache_fill( MetaDesc::Var* crv, uints offs )
    {
        MetaDesc::Var* old_var = _curvar.var;
        MetaDesc::Var* old_cvar = _cachevar;
        uints old_offs = _current->offs;

        _current->offs = offs;

        //pointer-type members
        if( crv->is_pointer() )
            crv = crv->desc->first_child();

        //force reading to cache
        _cachevar = _curvar.var = crv;

        streamvar(*crv);

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
        if(!_curvar.var->nameless_root)
            movein_process_key<READ_MODE>();

        //set if reading to cache, when the current variable has been already read or its default value will be used
        if(_cacheskip) {
            _cacheskip = 0;
            return moveto_expected_target<READ_MODE>();
        }

        MetaDesc& desc = *var.desc;
        if( desc.is_primitive() ) {
            return data_read_value( 0, desc.btype );
        }
        else if( desc.is_array() ) {
            cache_container cc( *this, desc );
            return cache_read_array(cc);
        }

        movein_struct<READ_MODE>();

        uints n = desc.children.size();
        for( uints i=0; i<n; ++i )
        {
            streamvar( desc.children[i] );
        }

        return 0;
    }

    ///Read array to container \a c from this binstream
    opcd cache_read_array( binstream_container& c )
    {
        uints na = UMAX;
        uints n = c._nelements;
        type t = c._type;

        //read bgnarray
        opcd e = data_read_value( &na, t.get_array_begin() );
        if(e)  return e;

        if( na != UMAX  &&  n != UMAX  &&  n != na )
            return ersMISMATCHED "requested and stored count";
        if( na != UMAX )
            n = na;
        else
            t = c.set_array_needs_separators();

        uints count=0;
        e = data_read_array_content(c, n, &count);

        //read endarray
        if(!e)
            e = data_read_value( &count, t.get_array_end() );

        return e;
    }

    ///Find _curvar.var->varname in cache
    bool cache_lookup()
    {
        if( _cache.size() == 0 )
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

        _current = _cachestack.push();
        _current->offs = 0;
        _current->buf = &_curvar.var->defval;
        _cachedefval = _curvar.var;

        return true;
    }
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
struct type_holder {
    const T* operator = (const T& val) {
        return &val;
    }
};

template<class T>
inline type_holder<T> get_type_holder(T*) {
    return type_holder<T>();
}

/// struct-related defines:
/**
    @def MM(meta,n,v)   specify member metadata providing member name
    @def MME(meta,n,v)  specify an enum-type member
    @def MMT(meta,n,t)  specify member metadata providing member type
    @def MMP(meta,n,t)  specify a pointer-type member
    @def MMD(meta,n,d)  specify member metadata providing a default value of member type
    @def MMTD(meta,n,d) specify member metadata providing a default value of specified type
    @def MMAT(meta,n,t) specify that member is an array of type \a t
    @def MMAF(meta,n,t,s) specify that member is a fixed size array of type \a t
**/
#define MSTRUCT_OPEN(meta, n)       if( !meta.meta_struct_open(n) ) {
#define MM(meta, n, v)              { meta.meta_variable(n,&(v)); }
#define MME(meta, n, v)				{ meta.meta_variable_enum(n,&(v)); }
#define MMT(meta, n, t)             { meta.meta_variable<t>(n,0); }
#define MMP(meta, n, v)             { meta.meta_variable_pointer(n,&(v)); }

#define MMD(meta, n, v, d)          { meta.meta_variable(n,&(v)); meta.meta_cache_default( get_type_holder(&(v)) = d ); }
#define MMTD(meta, n, t, d)         { meta.meta_variable<t>(n,0); meta.meta_cache_default( get_type_holder<t>(0) = d ); }

#define MMAT(meta, n, t)            { meta.meta_variable_array<t>(n,0,UMAX); }
#define MMAF(meta, n, t, s)         { meta.meta_variable_array<t>(n,0,s); }

#define MSTRUCT_CLOSE(meta)         meta.meta_struct_close(); } return meta;
#define MSTRUCT_CLOSE_NORET(meta)   meta.meta_struct_close(); }

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
