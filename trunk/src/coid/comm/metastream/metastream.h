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
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Robert Strycek
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
    virtual ~metastream()    {}


    typedef bstype::type            type;
    typedef MetaDesc                DESC;
    typedef MetaDesc::Var           VAR;

    void init()
    {
        _root.desc = 0;

        _current = _cachestack.push();
        _current->offs = UMAX;
        _current->size = 0;

        _cachequit = 0;
        _cacherootentries = 0;
        _cachelevel = UMAX;

        _read_to_cache = false;

        _sesopen = 0;
        _arraynm = 0;
        _current_var = 0;
        _cur_var = 0;
        _cur_variable_name = 0;
        _cur_variable_default.reset();

        _disable_meta_write = _disable_meta_read = false;
        _templ_arg_rdy = false;

        _hook.set_meta( *this );

        _tmetafnc = 0;
        _fmtstream = 0;
    }

    void bind_formatting_stream( binstream& b )
    {
        _fmtstream = &b;
        stream_reset(0);
    }

    binstream& get_formatting_stream() const    { return *_fmtstream; }

    //void bind( binstream& b, int io=0 )         { _fmtstream->bind( *b, io ); }


    ///Return transport stream
    binstream& get_binstream()                  { return _hook; }

    void enable_meta_write( bool en )           { _disable_meta_write = !en; }
    void enable_meta_read( bool en )            { _disable_meta_read = !en; }

    ////////////////////////////////////////////////////////////////////////////////
    //@{ methods to physically stream the data utilizing the metastream

    template<class T>
    opcd prepare_type_read(T&, const token& name)
    {
        if(_disable_meta_read) { _tmetafnc = 0;  return 0; }
        _tmetafnc = &metacall<T>::stream;
        _root.desc = 0;
        _current_var = 0;

        opcd e;
        DASSERT( _sesopen <= 0 );   //or else unflushed write
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset_read();
            _sesopen = -1;
        }
        else {
            e = _fmtstream->read_separator();
            if(e) {
                _err << "error reading separator";
                return e;
            }
        }

        _arraynm = 0;

        *this << *(const T*)0;     // build description tree

        _cur_var = &_root;
        _cur_var->varname = name;

        return movein_current<READ_MODE>( !name.is_empty() );
    }

    template<class T>
    opcd prepare_type_write(T&, const token& name)
    {
        if(_disable_meta_write) { _tmetafnc = 0;  return 0; }
        _tmetafnc = &metacall<T>::stream;
        _root.desc = 0;
        _current_var = 0;

        opcd e;
        DASSERT( _sesopen >= 0 );   //or else unacked read
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset_write();
            _sesopen = 1;
        }
        else {
            e = _fmtstream->write_separator();
            if(e) return e;
        }

        _arraynm = 0;

        *this << *(const T*)0;     // build description

        _cur_var = &_root;
        _cur_var->varname = name;

        return movein_current<WRITE_MODE>( !name.is_empty() );
    }

    template<class T>
    opcd prepare_type_read_array( T&, uints n, const token& name )
    {
        if(_disable_meta_read) { _tmetafnc = 0;  return 0; }
        _tmetafnc = &metacall<T>::stream;
        _root.desc = 0;
        _current_var = 0;

        opcd e;
        DASSERT( _sesopen <= 0 );   //or else unflushed write
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset_read();
            _sesopen = -1;
        }
        else {
            e = _fmtstream->read_separator();
            if(e) {
                _err << "error reading separator";
                return e;
            }
        }

        meta_array(n);
        *this << *(const T*)0;     // build description

        _cur_var = &_root;
        _cur_var->varname = name;

        return movein_current<READ_MODE>( !name.is_empty() );
    }

    template<class T>
    opcd prepare_type_write_array( T&, uints n, const token& name )
    {
        if(_disable_meta_write) { _tmetafnc = 0;  return 0; }
        _tmetafnc = &metacall<T>::stream;
        _root.desc = 0;
        _current_var = 0;

        opcd e;
        DASSERT( _sesopen >= 0 );   //or else unacked read
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset_write();
            _sesopen = 1;
        }
        else {
            e = _fmtstream->write_separator();
            if(e) return e;
        }

        meta_array(n);
        *this << *(const T*)0;     // build description

        _cur_var = &_root;
        _cur_var->varname = name;

        return movein_current<WRITE_MODE>( !name.is_empty() );
    }


    template<class T>
    opcd stream_in( T& x, const token& name = token::empty() )
    {
        _read_to_cache = false;
        stream_reset(false);

        opcd e;
        try {
            e = prepare_type_read(x,name);
            if(e) return e;

            _hook >> x;
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    template<class T>
    opcd stream_in_cache( const token& name = token::empty() )
    {
        _read_to_cache = true;
        stream_reset(false);

        opcd e;
        try {
            e = prepare_type_read( *(const T*)0, name );
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    template<class T>
    opcd stream_out( const T& x, const token& name = token::empty(), bool tocache=false )
    {
        _write_to_cache = tocache;
        stream_reset(false);

        opcd e;
        try {
            e = prepare_type_write(x,name);
            if(e) return e;

            if(!tocache)
                _hook << x;
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    template<class T>
    opcd stream_array_in( binstream_containerT<T>& C, const token& name = token::empty() )
    {
        _read_to_cache = false;
        stream_reset(false);

        opcd e;
        try {
            e = prepare_type_read_array( *(const T*)0, C._nelements, name );
            if(e) return e;

            return _hook.read_array(C);
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    template<class T>
    opcd stream_array_in_cache( const token& name = token::empty(), uints n=UMAX )
    {
        _read_to_cache = true;
        stream_reset(false);

        opcd e;
        try {
            e = prepare_type_read_array( *(const T*)0, n, name );
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    template<class T>
    opcd stream_array_out( binstream_containerT<T>& C, const token& name = token::empty(), bool tocache=false )
    {
        _read_to_cache = tocache;
        stream_reset(false);

        opcd e;
        try {
            e = prepare_type_write_array( *(const T*)0, C._nelements, name );
            if(e) return e;

            if(!tocache)
                return _hook.write_array(C);
        }
        catch(opcd ee) {e=ee;}
        return e;
    }

    template<class CONT>
    opcd stream_container_in( CONT& C, const token& name = token::empty() )
    {
        typedef typename binstream_adapter_writable<CONT>::TBinstreamContainer     BC;

        BC bc = binstream_container_writable<CONT,BC>::create(C);
        return stream_array_in( bc, name );
    }

    template<class CONT>
    opcd stream_container_out( CONT& C, const token& name = token::empty() )
    {
        typedef typename binstream_adapter_readable<CONT>::TBinstreamContainer     BC;

        BC bc = binstream_container_readable<CONT,BC>::create(C);
        return stream_array_out( bc, name );
    }


    template<class T>
    void xstream_in( T& x, const token& name = token::empty() )
    {   opcd e = stream_in(x,name);  if(e) throw e; }

    template<class T>
    void xstream_out( T& x, const token& name = token::empty() )
    {   opcd e = stream_out(x,name);  if(e) throw e; }


    void stream_acknowledge( bool eat = false )
    {
        DASSERT( _sesopen <= 0 );
        if( _sesopen < 0 ) {
            _sesopen = 0;
            _arraynm = 0;
            _fmtstream->acknowledge(eat);
        }
    }

    void stream_flush()
    {
        DASSERT( _sesopen >= 0 );
        if( _sesopen > 0 ) {
            _sesopen = 0;
            _arraynm = 0;
            _fmtstream->flush();
        }
    }

    void stream_reset( int fmts_reset )
    {
        _sesopen = 0;
        _arraynm = 0;

        _stack.reset();
        cache_reset();

        _tmetafnc = 0;
        _cur_var = 0;
        _cur_variable_name = 0;
        _cur_variable_default.reset();
        defval = 0;

        _err.reset();
        if( fmts_reset>0 )
            _fmtstream->reset_read();
        else if( fmts_reset<0 )
            _fmtstream->reset_write();
    }

//@}

    void cache_reset()
    {
        _data.reset();
        _cachestack.need(1);
    }

    const VAR& get_root_var() const                         { return _root; }
    const uchar* get_cache() const                          { return _data.ptr(); }

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

    ///Called before metastream << on member variable
    void meta_variable( const char* varname )
    {
        _cur_variable_name = varname;
        _cur_variable_default.reset();
    }

    template<class T>
    void meta_variable_with_default( const char* varname, const T& defval )
    {
        _cur_variable_name = varname;

        _cur_variable_default.need_new( sizeof(T) );
        T* tmp = new( _cur_variable_default.ptr() ) T(defval);
    }

    bool meta_struct_open( const char* name )
    {
        if( is_template_name_mode() )
            return handle_template_name_mode(name);

        return meta_insert(name);
    }

    void meta_struct_close()
    {
        do { _current_var = _map.pop(); }
        while( _current_var && _current_var->desc->type_name.is_empty() );
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
        
        DESC* d = _map.find_or_create( type_name, t );
        meta_fill_parent_variable(d);
    }

    //@} meta_*

    ///Get type descriptor for given type
    template<class T>
    const VAR* get_type_desc( const T* )
    {
        _root.desc = 0;
        _current_var = 0;

        _arraynm = 0;

        *this << *(const T*)0;     // build description
        return &_root;
    }

    template<class T>
    struct TypeDesc {
        static const VAR* get(metastream& meta)   { return meta.get_type_desc( (const T*)0 ); }
        static charstr get_str(metastream& meta)
        {
            const VAR* var = meta.get_type_desc( (const T*)0 );
            charstr res;
            var->type_string(res);
            return res;
        }
    };

    const DESC* get_type_info( const token& type ) const
    {
        return _map.find(type);
    }

    void get_type_info_all( dynarray<const metastream::DESC*>& dst )
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

        virtual opcd write_raw( const void* p, uints& len )      { return _meta->data_write_raw(p,len); }
        virtual opcd read_raw( void* p, uints& len )             { return _meta->data_read_raw(p,len); }

        virtual opcd write_array_content( binstream_container& c )        { return _meta->data_write_array_content(c); }
        virtual opcd read_array_content( binstream_container& c, uints n ) { return _meta->data_read_array_content(c,n); }

        virtual opcd write_array_separator( type t, uchar end ) { return _meta->data_write_array_separator(t,end); }
        virtual opcd read_array_separator( type t )             { return _meta->data_read_array_separator(t); }

        virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) {return ersNOT_IMPLEMENTED;}
        virtual opcd bind( binstream& bin ) {return ersNOT_IMPLEMENTED;}

        virtual bool is_open() const                            { return _meta != 0; }
        virtual void flush()                                    { _meta->stream_flush(); }
        virtual void acknowledge( bool eat=false )              { _meta->stream_acknowledge(eat); }

        virtual void reset_read()                               { _meta->stream_reset(1); }
        virtual void reset_write()                              { _meta->stream_reset(-1); }

        virtual uint binstream_attributes( bool in0out1 ) const {return 0;} //fATTR_SIMPLEX
    };

    friend class binstreamhook;


    ////////////////////////////////////////////////////////////////////////////////
    ///Interface for holding built descriptors
    struct StructureMap
    {
        DESC* find( const token& k ) const;
        DESC* create_array_desc( uints n );

        DESC* create( const token& n, type t )
        {
            DESC d(n);
            d.btype = t;
            return insert(d);
        }

        DESC* find_or_create( const token& n, type t )
        {
            DESC* d = find(n);
            return d ? d : create( n, t );
        }

        void get_all_types( dynarray<const DESC*>& dst ) const;

        VAR* last() const         { VAR** p = _stack.last();  return p ? *p : 0; }
        VAR* pop()                { VAR* p;  return _stack.pop(p) ? p : 0; }
        void push( VAR* v )       { _stack.push(v); }

        StructureMap();
        ~StructureMap();

    protected:
        DESC* insert( const DESC& v );

        dynarray<VAR*> _stack;
        void* pimpl;
    };


    ////////////////////////////////////////////////////////////////////////////////
    StructureMap _map;

    charstr _err;
    
    // description parsing:
    VAR _root;
    VAR* _current_var;
    const char* _cur_variable_name;
    dynarray<uchar> _cur_variable_default;

    int _sesopen;                   ///< flush(>0) or ack(<0) session currently open
    int _arraynm;                   ///< first array element marker in non-meta mode

    dynarray<VAR*> _stack;    ///< stack for current variable

    dynarray<charstr> _templ_name_stack;
    bool _templ_arg_rdy;

    bool _read_to_cache;
    bool _write_to_cache;

    bool _disable_meta_write;       ///< disable meta functionality for writting
    bool _disable_meta_read;        ///< disable meta functionality for reading

    binstream* _fmtstream;          ///< bound formatting front-end binstream
    binstreamhook _hook;            ///< internal data binstream

    dynarray<uchar> _data;          ///< cache for unordered input data

    //workaround for M$ compiler, instantiate dynarray<uint> first so it doesn't think these are the same
    typedef dynarray<uint>          __Tdummy;

    struct CacheEntry
    {
        uints offs;                 ///< offset to the current entry in stored class table
        uints size;                 ///< remaining array size when reading an array from cache
    };

    dynarray<CacheEntry> _cachestack; ///< cache table stack
    CacheEntry* _current;

    VAR* _cachequit;                ///< root variable read from cache

    uints _cachelevel;              ///< level where the cache was initialized
    uints _cacherootentries;        ///< no.of valid root level members in cache (for fast discarding of cache when everything was read)

    const dynarray<uchar>* defval;  ///< default value of object provided by cache

    charstr _rvarname;              ///< name of variable that follows in the input stream

    VAR* _cur_var;                  ///< currently processed variable (read/write)
    charstr _struct_name;           ///< used during the template name building step


    template<class T>
    struct metacall
    {
        static metastream& stream( metastream& m, void* t )
        {
            return m << *(const T*)t;
        }
    };

    typedef metastream& (*type_metacall)( metastream&, void* );

    type_metacall   _tmetafnc;

private:

    uints read_cache_uint( uints& offs )
    {
        DASSERT( offs < _data.size() );
        uints v = *(const uints*)(_data.ptr() + offs);
        offs += sizeof(uints);
        return v;
    }

    uints read_cache_offset( uints& offs )
    {
        DASSERT( offs < _data.size() );
        uints v = offs + *(const uints*)(_data.ptr() + offs);
        offs += sizeof(uints);
        DASSERT( v <= _data.size() );
        return v;
    }

    ////////////////////////////////////////////////////////////////////////////////
    //@{ meta_* functions deal with building the description tree

    bool meta_find( const token& name )
    {
        DESC* d = _map.find(name);
        if(!d)
            return false;

        meta_fill_parent_variable(d);
        return true;
    }

    bool meta_insert( const token& name )
    {
        if( meta_find(name) )
            return true;

        DESC* d = _map.create( name, type() );
        VAR* v = meta_fill_parent_variable(d);

        _map.push( _current_var );
        _current_var = v;
        return false;
    }

    ///Insert array when its elements are arrays too
    void meta_insert_array( uints n )
    {
        DESC* d = _map.create_array_desc(n);
        VAR* v = meta_fill_parent_variable(d);

        _map.push( _current_var );
        _current_var = v;
    }

    VAR* meta_fill_parent_variable( DESC* d, bool is_hidden=false )
    {
        VAR* var;

        //remember the first descriptor, it's the root type requested for streaming
        if(!_root.desc)
        {
            _root.desc = d;
            var = &_root;
        }
        else
        {
            var = _current_var->add_child( d, _cur_variable_name, _cur_variable_default );

            //get back from multiple array decl around this type
            if( !d->is_array() )
                while( _current_var && _current_var->is_array() )
                    _current_var = _map.pop();
        }

        _cur_variable_name = 0;
        _cur_variable_default.reset();

        return var;
    }
    
    //@} meta_* functions

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

    ////////////////////////////////////////////////////////////////////////////////
    VAR* last_var() const           { return *_stack.last(); }
    void pop_var()                  { EASSERT( _stack.pop(_cur_var) ); }
    void push_var()                 { _stack.push(_cur_var);  _cur_var = _cur_var->desc->first_child(); }

    VAR* parent_var() const
    {
        VAR** v = _stack.last();
        return v ? *v : 0;
    }

    bool is_first_var() const
    {
        if( _stack.size() == 0 )
            return true;            //lets treat all top-level variables as first
        VAR* p = last_var();
        return _cur_var == p->desc->children.ptr();
    }


    charstr& dump_stack( charstr& dst, int depth, VAR* var=0 ) const
    {
        if(!dst.is_empty())
            dst << char('\n');

        if( depth <= 0 )
            depth = (int)_stack.size() + depth;

        if( depth > (int)_stack.size() )
            depth = (int)_stack.size();

        for( int i=0; i<depth; ++i )
        {
            dst << char('/');
            _stack[i]->dump(dst);
        }

        if(var) {
            dst << char('/');
            var->dump(dst);
        }
        return dst;
    }

    ////////////////////////////////////////////////////////////////////////////////
protected:

    enum { WRITE_MODE=0, READ_MODE=1 };

    template<int R>
    opcd movein_struct()
    {
        if( !cache_prepared() )
        {
            opcd e = R
                ? _fmtstream->read_struct_open( &_cur_var->desc->type_name )
                : _fmtstream->write_struct_open( &_cur_var->desc->type_name );

            if(e) {
                dump_stack(_err,0);
                _err << " - error " << (R?"reading":"writing") << " struct opening token";
                throw e;
                return e;
            }
        }
        else {
            CacheEntry* ce = _cachestack.push();
            _current = ce-1;

            if( _cur_var->is_array_element() )
                ce->offs = _current->offs;
            else
                ce->offs = read_cache_offset( _current->offs );

            _current = ce;
        }

        push_var();
        return 0;
    }

    template<int R>
    opcd moveout_struct()
    {
        pop_var();

        if( !cache_prepared() )
        {
            opcd e = R
                ? _fmtstream->read_struct_close( &_cur_var->desc->type_name )
                : _fmtstream->write_struct_close( &_cur_var->desc->type_name );

            if(e) {
                dump_stack(_err,0);
                _err << " - error " << (R?"reading":"writing") << " struct closing token";
                throw e;
                return e;
            }
        }
        else
        {
            uints ooff = _current->offs;

            _current = _cachestack.pop();
            if( _cur_var->is_array_element() )
                _current->offs = ooff;
        }

        return 0;
    }


    template<int R>
    opcd movein_process_key()
    {
        bool ary = _cur_var->is_array_element();
        if(ary)  return 0;

        //this is the first member so no member separator
        return R
            ? fmts_or_cache_read_key()
            : fmts_or_cache_write_key();
    }

    ///Move onto the nearest primitive or array member
    //@param R read(1) or write(0) mode
    //@param key the data should start with a variable name (true), or directly with data
    template<int R>
    opcd movein_current( bool key )
    {
        opcd e;

        if(key)
            movein_process_key<R>();

        while( _cur_var->is_compound() )
        {
            movein_struct<R>();
            movein_process_key<R>();
        }

        return 0;
    }

    template<int R>
    opcd movein_array()
    {
        opcd e;

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

        //get next var
        VAR* par = parent_var();
        if(!par) {
            //!par should only happen here if the root type was a primitive type
            _tmetafnc = 0;
            return 0;
        }
        if( _cur_var == _cachequit )    invalidate_cache_entry();

        VAR* cvar = par->desc->next_child(_cur_var);
        while( !cvar )
        {
            //we are in the (last) child variable, the parent could have been only
            // a compound or an array.
            //arrays have their terminating tokens read elsewhere, but for the
            // structs we have to read the closing token here
            if( par->is_compound() )
                moveout_struct<R>();
            else
                pop_var();

            //exit from arrays 
            if( par->is_array() )
                return 0;

            if( _cur_var == _cachequit )    invalidate_cache_entry();
            if( _cur_var == &_root )        { _tmetafnc = 0;  return 0; }

            DASSERT( par == _cur_var );

            par = parent_var();
            cvar = par->desc->next_child(_cur_var);
        }

        _cur_var = cvar;
        return movein_current<R>(true);
    }


    ///This is called from internal binstream when a primitive data or control token is
    /// written. Possible type can be a primitive one, T_STRUCTBGN or T_STRUCTEND, 
    /// or array cotrol tokens
    opcd data_write( const void* p, type t )
    {
        DASSERT( !t.is_array_control_type() || !_cur_var || _cur_var->is_array() );

        if(!_tmetafnc)
        {
            if( _sesopen && !(t._ctrl & type::fARRAY_END) && !_arraynm ) {
                opcd e = _fmtstream->write_separator();
                if(e) return e;
            }
            else
                _sesopen = 1;

            if( t._ctrl & type::fARRAY_BEGIN )
                _arraynm = 1;
            else
                _arraynm = 0;

            return _fmtstream->write(p,t);
        }

        uchar t2 = t._type;
        //ignore the struct open and close tokens nested in binstream operators,
        // as we follow those within metastream operators instead
        if( t2 == type::T_STRUCTBGN  ||  t2 == type::T_STRUCTEND )
            return 0;

        //write value
        opcd e = _fmtstream->write( p, t );
        if(e) return e;

        if( t.is_array_control_type() )
        {
            if( t._ctrl & type::fARRAY_BEGIN )
                return 0;
        }

        return moveto_expected_target<WRITE_MODE>();
    }

    ///This is called from internal binstream when a primitive data or control token is
    /// read. 
    opcd data_read( void* p, type t )
    {
        DASSERT( !t.is_array_control_type() || !_cur_var || _cur_var->is_array() );

        if(!_tmetafnc)
        {
            if( _sesopen && !(t._ctrl & type::fARRAY_END) && !_arraynm )
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

            if( t._ctrl & type::fARRAY_BEGIN )
                _arraynm = 1;
            else
                _arraynm = 0;

            return _fmtstream->read(p,t);
        }

        uchar t2 = t._type;

        //ignore the struct open and close tokens nested in binstream operators,
        // as we follow those within metastream operators instead
        if( t2 == type::T_STRUCTBGN  ||  t2 == type::T_STRUCTEND )
            return 0;

        //read value
        opcd e = fmts_or_cache_read( p, t );
        if(e) {
            dump_stack(_err,0);
            _err << " - error reading variable '" << _rvarname << "', error: " << opcd_formatter(e);
            return e;
        }

        if( t.is_array_control_type() )
        {
            if( t._ctrl & type::fARRAY_BEGIN )
                return 0;
            else
            {
                if( _cur_var == _cachequit )    invalidate_cache_entry();
                if( _cur_var == &_root )        { _tmetafnc = 0;  return 0; }
            }
        }

        return moveto_expected_target<READ_MODE>();
    }

    opcd data_write_raw( const void* p, uints& len )
    {
        return _fmtstream->write_raw( p, len );
    }

    opcd data_read_raw( void* p, uints& len )
    {
        return _fmtstream->read_raw( p, len );
    }

    opcd data_write_array_content( binstream_container& c )
    {
        c.set_array_needs_separators();
        type t = c._type;

        if( !t.is_primitive() )
            return _hook.write_compound_array_content(c);
        
        return _fmtstream->write_array_content(c);
    }

    opcd data_read_array_content( binstream_container& c, uints n )
    {
        c.set_array_needs_separators();
        type t = c._type;

        if( !t.is_primitive() )
        {
            if( cache_prepared() )
            {
                //reading from cache
                opcd e;
                DASSERT( n != UMAX );

                bool cmpnd = !_cur_var->is_primitive();

                for( uints i=0; i<n; ++i )
                {
                    void* p = c.insert(1);
                    if(!p)
                        return ersNOT_ENOUGH_MEM;

                    movein_array<READ_MODE>();

                    uints off;
                    if(cmpnd)
                        off = read_cache_offset( _current->offs );

                    e = c._stream_in( _hook, p, c );
                    if(e)  return e;

                    --_current->size;

                    if(cmpnd)
                        _current->offs = off;
                }

                return 0;
            }
            else
                return _hook.read_compound_array_content(c,n);
        }

        //cache contains primitive binary data
        if( cache_prepared() )
        {
            if( c.is_continuous()  &&  n != UMAX )
            {
                uints na = n * t.get_size();
                xmemcpy( c.insert(n), _data.ptr()+_current->offs, na );

                _current->size -= n;
                _current->offs += na;
                return 0;
            }
            else
                return _hook.read_array_content(c,n);
        }

        return _fmtstream->read_array_content(c,n);
    }

    opcd data_write_array_separator( type t, uchar end )
    {
        if(!_tmetafnc) {
            _arraynm = 1;
            return _fmtstream->write_array_separator(t,end);
        }

        opcd e = _fmtstream->write_array_separator(t,end);
        if(e)  return e;

        return end ? e : movein_array<WRITE_MODE>();
    }

    ///Called from binstream to read array separator or detect an array end
    ///This method is called only for uncached data; reads from the cache
    /// use the data_read_array_content() method.
    opcd data_read_array_separator( type t )
    {
        if(!_tmetafnc) {
            _arraynm = 1;
            return _fmtstream->read_array_separator(t);
        }

        opcd e = _fmtstream->read_array_separator(t);
        if( e == ersNO_MORE )
            return e;
        if(e) {
            dump_stack(_err,0);
            _err << " - error reading array separator: " << opcd_formatter(e);

            throw e;
            return e;
        }

        return movein_array<READ_MODE>();
    }


    bool default_prepared() const       { return defval != 0; }
    bool cache_prepared() const         { return _current->offs != UMAX; }

    void invalidate_cache_entry()
    {
        if( --_cacherootentries == 0 )
            _data.reset();
        _current->offs = UMAX;
        _cachequit = 0;
    }

    ///Read data from formatstream or cache
    opcd fmts_or_cache_read( void* p, bstype::type t )
    {
        if( default_prepared() )
        {
            if( !t.is_no_size()  &&  t.get_size() != defval->size() )
                return ersMISMATCHED "size of element and the size of provided default value";

            defval->copy_bin_to( (uchar*)p, defval->size() );
            defval = 0;
            return 0;
        }
        else if( cache_prepared() )
        {
            if( t.is_array_control_type() )
            {
                if( t._ctrl & type::fARRAY_BEGIN )
                {
                    CacheEntry* ce = _cachestack.push();
                    _current = ce-1;

                    if( _cur_var->is_array_element() )
                        ce->offs = _current->offs;
                    else
                        ce->offs = read_cache_offset( _current->offs );

                    ce->size = read_cache_uint( ce->offs );

                    *(uints*)p = ce->size;
                    _current = ce;
                }
                else
                {
                    if( _current->size > 0 ) {
                        return ersMISMATCHED "elements left in cached array";
                    }

                    uints ooff = _current->offs;

                    _current = _cachestack.pop();
                    if( _cur_var->is_array_element() )
                        _current->offs = ooff;
                }
            }
            else    //data reads
            {
                //only primitive data here
                DASSERT( t.is_primitive() );
                DASSERT( t._type != type::T_STRUCTBGN  &&  t._type != type::T_STRUCTEND );
                DASSERT( !_cur_var->is_array_element() );

                uints cio = read_cache_offset( _current->offs );
                xmemcpy( p, _data.ptr()+cio, t.get_size() );
            }

            return 0;
        }
        else
            return _fmtstream->read( p, t );
    }

    opcd fmts_or_cache_read_key()
    {
        if( !cache_prepared() )
        {
            if( !cache_lookup() )
            {
                opcd e=0;
                if( !is_first_var() )
                {
                    e = _fmtstream->read_separator();
                    if(e) {
                        dump_stack(_err,0);
                        _err << " - error reading the variable separator: " << opcd_formatter(e);

                        throw e;
                        return e;
                    }
                }

                e = binstream_read_key( *_fmtstream, _rvarname );
                if(e) {
                    dump_stack(_err,0);
                    _err << " - error while seeking for variable '" << _cur_var->varname << "': " << opcd_formatter(e);

                    throw e;
                    return e;
                }

                if( _read_to_cache  ||  _rvarname != _cur_var->varname )
                {
                    if( _cur_var == &_root )
                        e = cache_fill_root();
                    else
                        e = cache_fill_member();

                    if(e) {
                        dump_stack(_err,0);
                        _err << " - error while seeking for variable '" << _cur_var->varname << "', probably not found: " << opcd_formatter(e);

                        //probably the element was not found
                        throw e;
                        return e;
                    }
                }
            }
        }

        return 0;
    }

    opcd fmts_or_cache_write_key()
    {
        opcd e;
        if( !is_first_var() )
        {
            e = _fmtstream->write_separator();
            if(e) {
                dump_stack(_err,0);
                _err << " - error writing the variable separator: " << opcd_formatter(e);

                throw e;
                return e;
            }
        }

        e = _fmtstream->write_key( _cur_var->varname );
        if(e) {
            dump_stack(_err,0);
            _err << " - error while writing the variable name '" << _cur_var->varname << "': " << opcd_formatter(e);

            throw e;
            return e;
        }

        return 0;
    }


    ///Temp container for array cache reads
    struct cache_container : public binstream_container
    {
        dynarray<uchar>& _data;
        uints _nactual;

        cache_container( dynarray<uchar>& data, bstype::type t, uints n ) : binstream_container(n,t,0,0),
            _data(data), _nactual(0)
        {
        }

        void* insert( uints n )
        {
            _nactual += n;
            return _data.add(n*_type.get_size());
        }

        const void* extract( uints n )
        {
            const void* p = &_data[ _nactual*_type.get_size() ];
            _nactual += n;
            return p;
        }

        bool is_continuous() const      { return true; }

        void set_actual_type( bstype::type t, uints n )
        {
            _type = t;
            _nelements = n;
        }
    };


    ///Fill intermediate cache, _rvarname contains the key read, and
    /// _cur_var->varname the key requested
    opcd cache_fill_member()
    {
        opcd e;
        //here _rvarname can only be another member of _cur_var's parent
        // we should find it and cache its whole structure, as well as any other
        // members until the one pointed to by _cur_var is found
        DESC* par = parent_var()->desc;

        if( _data.size() > 0 ) {
            DASSERT( _cachelevel == _stack.size() );
        }
        else {
            //create child offset table for members
            _data.need_newc( sizeof(uints) * par->num_children(), true );
            _cachelevel = _stack.size();
        }

        do {
            VAR* crv = par->find_child(_rvarname);
            if(!crv) {
                dump_stack(_err,0);
                _err << " - member variable: " << _rvarname << " not defined";
                
                e = ersNOT_FOUND "no such member variable";
                throw e;
                return e;
            }

            uints k = par->get_child_pos(crv);
            if( ((uints*)_data.ptr())[k] != UMAX ) {
                dump_stack(_err,0);
                _err << " - data for member: " << _rvarname << " specified more than once";
                
                e = ersMISMATCHED "redundant member data";
                throw e;
                return e;
            }

            //mark the position of cached data block for this member into parent's table
            k *= sizeof(uints);
            *(uints*)(_data.ptr()+k) = _data.size() - k;

            e = cache_member(crv);
            if(e)  return e;

            ++_cacherootentries;

            e = _fmtstream->read_separator();
            if(e) {
                dump_stack(_err,0);
                _err << " - error reading separator, error: " << opcd_formatter(e);
                return e;
            }

            e = binstream_read_key( *_fmtstream, _rvarname );
            if(e) {
                if( e == ersNO_MORE )
                {
                    if( _cur_var->defval.size() > 0 )
                    {
                        defval = &_cur_var->defval;
                        return 0;
                    }
                    else if( _read_to_cache  &&  _cacherootentries == _root.desc->num_children() )
                        return 0;
                }

                dump_stack(_err,0);
                _err << " - variable not found '" << _cur_var->varname << "', error: " << opcd_formatter(e);
                return e;
            }
        }
        while( _rvarname != _cur_var->varname );

        return 0;
    }

    ///Fill intermediate cache
    opcd cache_fill_root()
    {
        opcd e;

        DASSERT( _data.size() == 0 );
        _cachelevel = _stack.size();

        ++_cacherootentries;
        return cache_member(&_root);
    }


    ///Find _cur_var->varname in cache
    bool cache_lookup()
    {
        if( _data.size() == 0  ||  _cachelevel < _stack.size() )
            return false;

        //get child map
        const uints* dof = (const uints*)_data.ptr();
        DESC* par = parent_var()->desc;

        VAR* crv = par->find_child( _cur_var->varname );
        DASSERT(crv);

        uints k = par->get_child_pos(crv);
        if( dof[k] != UMAX )
        {
            //found in cache, set up a cache read
            _current->offs = k * sizeof(uints);
            _cachequit = crv;

            return true;
        }

        return false;
    }

    ///Read structured data of variable var to the cache.
    opcd cache_member( VAR* var )
    {
        opcd e;

        if( var->is_primitive() )
        {
            e = _fmtstream->read( _data.add( var->desc->btype.get_size() ), var->desc->btype );
            if(e) {
                dump_stack(_err,0);
                _err << " - error reading value of variable " << var->varname << ": " << opcd_formatter(e);

                throw e;
                return e;
            }
        }
        else
        {
            bool are = var->is_array_element();

            uints odof;
            if(are) {
                odof = _data.size();
                _data.add( sizeof(uints) );
            }

            if( !var->is_array() )
                e = cache_compound_member(var);
            else
            {
                VAR* element = var->element();;

                if( !element->is_primitive() )
                    e = cache_compound_member_array(var);
                else
                {
                    //mark the array size first
                    uints ona = _data.size();
                    _data.add( sizeof(uints) );

                    cache_container cc( _data, element->desc->btype, var->desc->array_size );
                    e = _fmtstream->read_array(cc);
                    
                    *(uints*)(_data.ptr()+ona) = cc._nactual;
                }
            }

            if(are)
                *(uints*)(_data.ptr()+odof) = _data.size() - odof;
        }

        return e;
    }

    ///Cache compound array
    opcd cache_compound_member_array( VAR* var )
    {
        type t = var->desc->btype;

        //mark the array size first
        uints ona = _data.size();
        _data.add( sizeof(uints) );
        uints na = UMAX;
        uints n = var->desc->array_size;

        opcd e = _fmtstream->read( &na, t.get_array_begin() );
        if(e) {
            dump_stack(_err,0,var);
            _err << " - expecting array start";
            
            throw e;
            return e;
        }

        if( na != UMAX  &&  n != UMAX  &&  n != na ) {
            dump_stack(_err,0);
            _err << " - array definition expects " << n << " items but found " << na << " items";

            e = ersMISMATCHED "requested and stored count";
            throw e;
            return e;
        }
        if( na != UMAX )
            n = na;

        VAR* element = var->element();;
        type tae = element->get_type();

        uints k=0;
        while( n>0 )
        {
            --n;

            //peek if there's an element to read
            if( (e = _fmtstream->read_array_separator(tae)) )
                break;

            e = cache_member(element);
            if(e)  return e;

            ++k;
            type::mask_array_element_first_flag(tae);
        }

        //read trailing mark only for arrays with specified size
        e = _fmtstream->read( 0, t.get_array_end() );
        if(e) {
            dump_stack(_err,0,var);
            _err << " - error reading array end, error: " << opcd_formatter(e);
        }

        //write number of elements actually read
        *(uints*)(_data.ptr()+ona) = k;

        return e;
    }

    ///Cache compound non-array object
    opcd cache_compound_member( VAR* var )
    {
        //a compound type, pull in
        // Write children offset table.
        uints n = var->desc->num_children();
        uints odof = _data.size();
        _data.addc( n * sizeof(uints), true );

        opcd e = _fmtstream->read_struct_open( &var->desc->type_name );
        if(e) {
            dump_stack(_err,0,var);
            _err << " - expected struct open token, error: " << opcd_formatter(e);

            throw e;
            return e;
        }

        for( uints i=0; i<n; ++i )
        {
            e = _fmtstream->read_separator();

            //read the next key
            if(!e) e = binstream_read_key( *_fmtstream, _rvarname );
            if(e) {
                dump_stack(_err,0);
                _err << " - error reading variable '" << _rvarname << "', error: " << opcd_formatter(e);

                throw e;
                return e;
            }

            VAR* crv = var->desc->find_child(_rvarname);
            if(!crv) {
                dump_stack(_err,0,var);
                _err << " - member variable '" << _rvarname << "' not defined";
                
                e = ersNOT_FOUND "no such member variable";
                throw e;
                return e;
            }

            uints k = var->desc->get_child_pos(crv) * sizeof(uints) + odof;
            *(uints*)(_data.ptr()+k) = _data.size() - k;

            e = cache_member(crv);
            if(e)  return e;
        }

        e = _fmtstream->read_struct_close( &var->desc->type_name );
        if(e) {
            dump_stack(_err,0,var);
            _err << " - expected struct close token, error: " << opcd_formatter(e);
            return e;
        }

        return e;
    }

};

////////////////////////////////////////////////////////////////////////////////
/// struct-related defines:
/**
    @def MM(meta,n,v)   specify member metadata providing member name
    @def MMT(meta,n,t)  specify member metadata providing member type
    @def MMD(meta,n,d)  specify member metadata providing a default value of member type
    @def MMAT(meta,n,t) specify that member is an array of type \a t
**/
#define MSTRUCT_OPEN(meta, n)       if( !meta.meta_struct_open(n) ) {
#define MM(meta, n, v)              meta.meta_variable(n);  meta << v;
#define MMT(meta, n, t)             meta.meta_variable(n);  meta << *(t*)0;
#define MMD(meta, n, d)             meta.meta_variable_with_default(n,d);  meta << d;
#define MMAT(meta, n, t)            meta.meta_variable(n);  meta.meta_array();  meta << *(t*)0;
#define MSTRUCT_CLOSE(meta)         meta.meta_struct_close(); }  return meta;

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
