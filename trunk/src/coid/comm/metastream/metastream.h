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

#ifndef __COID_COMM_METASTREAM__HEADER_FILE__
#define __COID_COMM_METASTREAM__HEADER_FILE__

#include "../namespace.h"
#include "../dynarray.h"
#include "../str.h"
#include "../binstream/binstream.h"

//#include "fmtstreamcxx.h"



COID_NAMESPACE_BEGIN

/** \class metastream
    @see http://coid.sourceforge.net/doc/metastream.html for description
**/

////////////////////////////////////////////////////////////////////////////////
///Base class for streaming structured data
class metastream
{
public:
    //metastream( binstream* b=0 ) :  _txtfmt(0, 0)  {init(); bind(b);}
    //metastream( binstream& b )   :  _txtfmt(0, 0)  {init(); bind(b);}
    metastream() { init(); }
    virtual ~metastream()    {}


    typedef bstype::type                type;


    void init()
    {
        _root._desc = 0;
        _root._is_array = false;
        _root._array_size = UMAX;

        _cachetbloffs = UMAX;
        _cachetbloffsquit = 0;
        _cache_arysize = 0;
        _cache_aryoffs = UMAX;
        _cacherootentries = 0;
        _cachelevel = UMAX;

        _sesopen = 0;
        _arraynm = 0;
        _current_desc = 0;
        _cur_var = 0;
        _cur_variable_name = 0;
        //_cur_varnum = 0;

        _disable_meta_write = _disable_meta_read = false;
        _templ_arg_rdy = false;
        //_hidarray_id = 0;
        //_is_array = false;
        //_array_size = UMAX;
        _hook.set_meta( *this );

        _tmetafnc = 0;
        _fmtstream = 0;//&_txtfmt;
    }

    void bind_formatting_stream( binstream& b )
    {
        _fmtstream = &b;
        stream_reset();
        //_cur_varnum = 0;
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
        _root._desc = _current_desc = 0;
        //_root._varname = name;

        opcd e;
        DASSERT( _sesopen <= 0 );   //or else unflushed write
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset();
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
        _is_array = false;
        _array_size = UMAX;

        *this << *(const T*)0;     // build description
        _cur_var = &_root;

        if( !name.is_empty() ) {
            charstr tn;
            e = binstream_read_key( *_fmtstream, tn );
            if(!e && tn != name ) {
                _err << "expected variable '" << name << "', found " << tn;
                e = ersMISMATCHED "key name";
            }
            if(e) {
                _err << "expected variable '" << name << "', error: " << opcd_formatter(e);
                return e;
            }
        }

        if( _cur_var->is_compound() )
            e = moveto_expected_targetr();
        return e;
    }

    template<class T>
    opcd prepare_type_write(T&, const token& name)
    {
        if(_disable_meta_write) { _tmetafnc = 0;  return 0; }
        _tmetafnc = &metacall<T>::stream;
        _root._desc = _current_desc = 0;
        //_root._varname = name;

        opcd e;
        DASSERT( _sesopen >= 0 );   //or else unacked read
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset();
            _sesopen = 1;
        }
        else {
            e = _fmtstream->write_separator();
            if(e) return e;
        }

        _arraynm = 0;
        _is_array = false;
        _array_size = UMAX;

        *this << *(const T*)0;     // build description
        _cur_var = &_root;

        if( !name.is_empty() ) {
            e = _fmtstream->write_key( name );
            if(e) return e;
        }

        return _cur_var->is_compound()
            ? moveto_expected_targetw()
            : e;
    }

    template<class T>
    opcd prepare_type_read_array( T&, uints n, const token& name )
    {
        if(_disable_meta_read) { _tmetafnc = 0;  return 0; }
        _tmetafnc = &metacall<T>::stream;
        _root._desc = _current_desc = 0;
        _root._is_hidden = false;
        //_root._varname = name;

        opcd e;
        DASSERT( _sesopen <= 0 );   //or else unflushed write
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset();
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

        if( !name.is_empty() ) {
            charstr tn;
            e = binstream_read_key( *_fmtstream, tn );
            if(!e && tn != name ) {
                _err << "expected variable '" << name << "', found: " << tn;
                e = ersMISMATCHED "key name";
            }
            else if(e)
                _err << "expected variable '" << name << "', error: " << opcd_formatter(e);
            if(e)  return e;
        }

        _cur_var = &_root;
        return 0;
    }

    template<class T>
    opcd prepare_type_write_array( T&, uints n, const token& name )
    {
        if(_disable_meta_write) { _tmetafnc = 0;  return 0; }
        _tmetafnc = &metacall<T>::stream;
        _root._desc = _current_desc = 0;
        _root._is_hidden = false;
        //_root._varname = name;

        opcd e;
        DASSERT( _sesopen >= 0 );   //or else unacked read
        if( _sesopen == 0 ) {
            _err.reset();
            _fmtstream->reset();
            _sesopen = 1;
        }
        else {
            e = _fmtstream->write_separator();
            if(e) return e;
        }

        meta_array(n);
        *this << *(const T*)0;     // build description

        if( !name.is_empty() ) {
            e = _fmtstream->write_key( name );
            if(e)  return e;
        }

        _cur_var = &_root;
        return e;
    }


    template<class T>
    opcd stream_in( T& x, const token& name = token::empty() )
    {
        opcd e = prepare_type_read(x,name);
        if(e) return e;

        _hook >> x;
        return 0;
    }

    template<class T>
    opcd stream_out( const T& x, const token& name = token::empty() )
    {
        opcd e = prepare_type_write(x,name);
        if(e) return e;

        _hook << x;
        return 0;
    }

    template<class T>
    opcd stream_in_array( binstream_containerT<T>& C, const token& name = token::empty() )
    {
        opcd e = prepare_type_read_array( *(const T*)0, C._nelements, name );
        if(e) return e;

        return _hook.read_array(C);
    }

    template<class T>
    opcd stream_out_array( binstream_containerT<T>& C, const token& name = token::empty() )
    {
        opcd e = prepare_type_write_array( *(const T*)0, C._nelements, name );
        if(e) return e;

        return _hook.write_array(C);
    }

    template<class T>
    opcd stream_in_array( binstream_dereferencing_containerT<T>& C, const token& name = token::empty() )
    {
        opcd e = prepare_type_read_array( *(const T*)0, C._nelements, name );
        if(e) return e;

        return _hook.read_array(C);
    }

    template<class T>
    opcd stream_out_array( binstream_dereferencing_containerT<T>& C, const token& name = token::empty() )
    {
        opcd e = prepare_type_write_array( *(const T*)0, C._nelements, name );
        if(e) return e;

        return _hook.write_array(C);
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
        //_cur_varnum = 0;
    }

    void stream_flush()
    {
        DASSERT( _sesopen >= 0 );
        if( _sesopen > 0 ) {
            _sesopen = 0;
            _arraynm = 0;
            _fmtstream->flush();
        }
        //_cur_varnum = 0;
    }

    void stream_reset()
    {
        _sesopen = 0;
        _arraynm = 0;
        //_cur_varnum = 0;
        _stack.reset();
        //_array.reset();
        _is_array = false;
        _data.reset();
        _cachetbl.reset();
        _tmetafnc = 0;
        _cur_var = 0;

        _err.reset();
        _fmtstream->reset();
    }

    //@}

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

#ifdef _MSC_VER
    metastream& operator << (const int&a)                   {meta_primitive( "int", get_type(a) ); return *this;}
    metastream& operator << (const uint&a)                  {meta_primitive( "uint", get_type(a) ); return *this;}
    metastream& operator << (const char&a)                  {meta_primitive( "char", get_type(a) ); return *this;}
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
    }

    bool meta_struct_open( const char* name )
    {
        if( is_template_name_mode() )
            return handle_template_name_mode(name);

        return meta_insert(name);
    }

    void meta_struct_close()
    {
        do { _current_desc = _map.pop(); }
        while( _current_desc && _current_desc->_typename.is_empty() );
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
        if(_is_array)
            meta_insert_array();

        //*_array.add() = n;
        _is_array = true;
        _array_size = n;
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


    ////////////////////////////////////////////////////////////////////////////////
    ///Descriptor structure for type
    struct DESC
    {
        ///Member variable descriptor
        struct VAR
        {
            DESC* _desc;                ///< ptr to descriptor for the type of the variable
            charstr _varname;           ///< name of the variable

            uints _array_size;          ///< array size, UMAX for unknown
            bool _is_array;             ///<
            bool _is_hidden;            ///< hidden, implicit variable

            bool is_array() const       { return _is_array; }//_array.size() > 0; }
            bool is_compound() const    { return !_desc->_btype.is_primitive() && !_is_array; }//_array.size()==0; }
            bool is_hidden() const      { return _is_hidden; }


            charstr& dump( charstr& dst ) const
            {
                if( !_desc->_btype.is_primitive() )
                    dst << "class ";
                dst << _desc->_typename << _varname;
                if( is_array() ) {
                    if( _array_size == UMAX )
                        dst << "[]";
                    else
                        dst << char('"') << _array_size << char('"');
                }
                return dst;
            }

            charstr& type_name( charstr& dst ) const
            {
                if( is_array() ) {
                    if( _desc->_btype._type == type::T_CHAR )
                    { dst << "$string";  return dst; }
                    else
                        dst << char('@');
                }
                return _desc->type_name(dst);
            }
        };

        dynarray<VAR> _children;        ///< member variables

        charstr _typename;              ///< type name (name of a structure)
        type _btype;                    ///< basic type id

        

        uints num_children() const      { return _children.size(); }

        ///Get first member
        VAR* first_child() const
        {
            DASSERT( _children.size() > 0 );
            return (VAR*)_children.ptr();
        }

        ///Get next member from given one
        VAR* next_child( VAR* c ) const
        {
            if( !c || c >= _children.last() )  return 0;
            return c+1;
        }

        ///Linear search for specified member
        VAR* find_child( const token& name ) const
        {
            uints n = _children.size();
            for( uints i=0; i<n; ++i )
                if( _children[i]._varname == name )  return (VAR*)&_children[i];
            return 0;
        }

        uints get_child_pos( VAR* v ) const      { return uints(v-_children.ptr()); }


        charstr& type_name( charstr& dst ) const
        {
            if( _typename.is_empty() )  return _children[0].type_name(dst);
            if( _btype.is_primitive() )
                dst << char('$');
            dst << _typename;
            if(_btype._size)
                dst << (8*_btype._size);
            return dst;
        }

        operator token() const              { return _typename; }
        uints size() const                  { return _children.size(); }


        DESC() {}
        DESC( const token& n ) : _typename(n) {}

        void add_desc_var( DESC* d, const token& n, bool is_array, uints array_size, bool is_hidden )
        {
            VAR* c = _children.add();
            c->_desc = d;
            c->_varname = n;
            //c->_array.takeover(ar);
            c->_is_array = is_array;
            c->_array_size = array_size;
            c->_is_hidden = is_hidden;
        }

    };

    ///Get type descriptor for given type
    template<class T>
    const DESC::VAR* get_type_desc( const T* )
    {
        _root._desc = _current_desc = 0;

        _arraynm = 0;
        _is_array = false;
        _array_size = UMAX;

        *this << *(const T*)0;     // build description
        return &_root;
    }

    template<class T>
    struct TypeDesc {
        static const DESC::VAR* get(metastream& meta)   { return meta.get_type_desc( (const T*)0 ); }
        static charstr get_str(metastream& meta)
        {
            const DESC::VAR* var = meta.get_type_desc( (const T*)0 );
            charstr res;
            var->type_name(res);
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
        virtual void reset()                                    { _meta->stream_reset();}
        virtual uint binstream_attributes( bool in0out1 ) const {return 0;} //fATTR_SIMPLEX
    };

    friend class binstreamhook;


    ////////////////////////////////////////////////////////////////////////////////
    ///Interface for holding built descriptors
    struct StructureMap
    {
        DESC* find( const token& k ) const;
        DESC* create_hidden_desc();

        DESC* create( const token& n, type t )
        {
            DESC d(n);
            d._btype = t;
            return insert(d);
        }

        DESC* find_or_create( const token& n, type t )
        {
            DESC* d = find(n);
            return d ? d : create( n, t );
        }

        void get_all_types( dynarray<const DESC*>& dst ) const;

        DESC* last() const                      { DESC** p = _stack.last();  return p ? *p : 0; }
        DESC* pop()                             { DESC* p;  return _stack.pop(p) ? p : 0; }
        void push( DESC* d )                    { _stack.push(d); }

        StructureMap();
        ~StructureMap();

    protected:
        DESC* insert( const DESC& v );

        dynarray<DESC*> _stack;
        void* pimpl;
    };


    ////////////////////////////////////////////////////////////////////////////////

    StructureMap _map;

    charstr _err;
    
    // description parsing:
    DESC::VAR _root;
    DESC* _current_desc;
    const char* _cur_variable_name;

    //int _cur_varnum;                ///< variable sequential number used for separating non-metadata entries
    int _sesopen;                   ///< flush(>0) or ack(<0) session open
    int _arraynm;                   ///< first array element marker in non-meta mode

    dynarray<DESC::VAR*> _stack;     ///< variable stack for data streaming

    //dynarray<uint> _array;          ///< array size stack, nonempty if under array during description building

    dynarray<charstr> _templ_name_stack;
    bool _templ_arg_rdy;

    bool _disable_meta_write;       ///< disable meta functionality for writting
    bool _disable_meta_read;        ///< disable meta functionality for reading

    bool _is_array;                 ///< array to be defined
    uints _array_size;              ///< size info about defined array, variable if UMAX

    binstream* _fmtstream;          ///< bound formatting front-end binstream

    //fmtstreamcxx _txtfmt;           ///< formatter stream (default)
    binstreamhook _hook;            ///< internal data binstream

    dynarray<uchar> _data;          ///< cache for unordered input data
    dynarray<uints> _cachetbl;      ///< cache table stack
    uints _cachetbloffs;            ///< offset to the current entry in stored class table
    uints _cachetbloffsquit;        ///< when unwinding back, an offset where to stop reading from cache
    uints _cache_arysize;           ///< remaining array size when reading an array from cache
    uints _cache_aryoffs;           ///< offset to current array element
    uints _cachelevel;              ///< level where the cache was initialized
    uints _cacherootentries;        ///< no.of valid root level members in cache

    charstr _rvarname;              ///< name of variable that follows in the input stream

    DESC::VAR* _cur_var;             ///< currently processed variable (read/write)
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
        uints v = *(const uints*)(_data.ptr() + offs);
        offs += sizeof(uints);
        return v;
    }

    ////////////////////////////////////////////////////////////////////////////////
    //@{ meta_* functions deal with building the description tree

    bool meta_find( const token& name )
    {
        DESC* d = _map.find( name );
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
        meta_fill_parent_variable(d);

        _map.push( _current_desc );
        _current_desc = d;
        return false;
    }

    ///Insert array when its elements are arrays too
    void meta_insert_array()
    {
        DESC* d = _map.create_hidden_desc();
        meta_fill_parent_variable(d,true);

        _map.push( _current_desc );
        _current_desc = d;

        _is_array = false;
        _array_size = UMAX;
    }

    void meta_fill_parent_variable( DESC* d, bool is_hidden=false )
    {
        //remember the first descriptor, it's the root type requested for streaming
        if(!_root._desc)
        {
            _root._desc = d;
            _root._is_array = _is_array;
            _root._array_size = _array_size;
            _root._is_hidden = is_hidden;
        }
        else
        {
            //DASSERT( _cur_variable_name  ||  _cur_var->is_hidden() );

            _current_desc->add_desc_var( d, _cur_variable_name, _is_array, _array_size, is_hidden );

            while( !is_hidden && _current_desc && _current_desc->_typename.is_empty() )
                _current_desc = _map.pop();
        }

        _is_array = false;
        _array_size = UMAX;
        _cur_variable_name = 0;
    }
    
    //@} meta_* functions

    bool is_template_name_mode()        { return _templ_name_stack.size() > 0; }
    
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
    DESC::VAR* last_var() const              { return *_stack.last(); }
    DESC::VAR* pop_var()                     { DESC::VAR* p;  return _stack.pop(p) ? p : 0; }
    void push_var( DESC::VAR* d )            { _stack.push(d); }

    DESC::VAR* parent_var() const
    {
        DESC::VAR** v = _stack.last();
        return v ? *v : 0;//&_root;
    }

    bool is_first_var() const
    {
        DESC::VAR* p = last_var();
        return _cur_var == p->_desc->_children.ptr();
    }


    charstr& dump_stack( charstr& dst, int depth, DESC::VAR* var=0 ) const
    {
        if(!dst.is_empty())
            dst << char('\n');

        if( depth <= 0 )
            depth = (int)_stack.size() - depth;

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

    enum { MODE_R=0, MODE_W=1, };

    opcd movein_struct( int w )
    {
        opcd e;
        e = w
            ? _fmtstream->write_struct_open( &_cur_var->_desc->_typename )
            : _fmtstream->read_struct_open( &_cur_var->_desc->_typename );
        if(e)  return e;

        push_var( _cur_var );
        _cur_var = _cur_var->_desc->first_child();
        return e;
    }

    opcd moveout_struct( int w )
    {
        _cur_var = pop_var();

        opcd e;
        e = w
            ? _fmtstream->write_struct_close( &_cur_var->_desc->_typename )
            : _fmtstream->read_struct_close( &_cur_var->_desc->_typename );
        return e;
    }

    ///Traverse the tree and set up the next primitive target for output streaming
    opcd moveto_expected_targetw( int inarray=0 )
    {
        //find what should come next
        // _cur_var was a primitive type, no need to check its children
        opcd e;
        do
        {
            if( inarray>0 || (inarray == 0 && _cur_var->is_compound()) )
            {
                //a compound type, pull in
                e = movein_struct(MODE_W);
                if(e)  return e;

                inarray = 0;
            }
            else
            {
                //get next var
                DESC::VAR* par = parent_var();
                if(!par) {
                    //!par should only happen here if the root type was a primitive type
                    _tmetafnc = 0;
                    return 0;
                }

                DESC::VAR* cvar = par->_desc->next_child(_cur_var);
                while( !cvar )
                {
                    if( par->is_array() )
                    {
                        _cur_var = 0;

                        if( inarray<0 )
                            moveout_struct(MODE_W);

                        return 0;
                    }

                    //pull out
                    e = moveout_struct(MODE_W);
                    if(e)  return e;
                    
                    if( _cur_var == &_root ) {
                        _tmetafnc = 0;
                        return 0;
                    }

                    DASSERT( par == _cur_var );

                    par = parent_var();
                    cvar = par->_desc->next_child(_cur_var);
                }

                _cur_var = cvar;
            }

            opcd e=0;
            if( !is_first_var() )
                e = _fmtstream->write_separator();
            //here just prefix the primitive value types and struct open token by the
            // variable name.
            if(!e) e = _fmtstream->write_key( _cur_var->_varname );
            if(e)  return e;
        }
        while( _cur_var->is_compound() );

        return 0;
    }

    ///Traverse the tree and set up the next target for input streaming
    opcd moveto_expected_targetr( int inarray=0 )
    {
        //find what should come next
        opcd e;
        do
        {
            if( inarray>0 || (inarray == 0 && _cur_var->is_compound()) )
            {
                //a compound type, pull in
                e = movein_struct(MODE_R);
                if(e) {
                    dump_stack(_err,0);
                    _err << " - error reading struct open token";
                    return e;
                }
                //_cur_var is now the expected variable to read

                inarray = 0;
            }
            else
            {
                //get next var
                DESC::VAR* par = parent_var();
                if(!par) {
                    //!par should only happen here if the root type was a primitive type
                    _tmetafnc = 0;
                    return 0;
                }

                DESC::VAR* cvar = par->_desc->next_child(_cur_var);
                while( !cvar )
                {
                    if( par->is_array() )
                    {
                        _cur_var = 0;

                        if( inarray<0 )
                            return moveout_struct(MODE_R);

                        return 0;
                    }

                    e = moveout_struct(MODE_R);
                    if(e) {
                        dump_stack(_err,0);
                        _err << " - error reading struct close token";
                        return e;
                    }

                    if( _cur_var == &_root ) {
                        _tmetafnc = 0;
                        return 0;
                    }

                    DASSERT( par == _cur_var );

                    par = parent_var();
                    cvar = par->_desc->next_child(_cur_var);

                    continue;
                }

                _cur_var = cvar;
            }

            e = fmts_or_cache_read_key();
            if(e) {
                dump_stack(_err,0);
                _err << " - error reading variable name, error: " << opcd_formatter(e);
                return e;
            }
        }
        while( _cur_var->is_compound() );

        return 0;
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
            {
                //push_var( _cur_var );
                //if( _cur_var->is_hidden() )
                //    _cur_var = _cur_var->_desc->first_child();
                DASSERT( _cur_var->_desc->_btype._type == t._type );

                return 0;
            }
            else
            {
                //_cur_var = pop_var();
                DASSERT( _cur_var->_desc->_btype._type == t._type );
            }
        }

        return moveto_expected_targetw();
    }

    ///This is called from internal binstream when a primitive data or control token is
    /// read. 
    opcd data_read( void* p, type t )
    {
        DASSERT( !t.is_array_control_type() || !_cur_var || _cur_var->is_array() );

        if(!_tmetafnc)
        {
            if( _sesopen && !(t._ctrl & type::fARRAY_END) && !_arraynm ) {
                opcd e = _fmtstream->read_separator();
                if(e) {
                    dump_stack(_err,0);
                    _err << " - error reading separator: " << opcd_formatter(e);
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
        //opcd e = _fmtstream->read( p, t );
        opcd e = fmts_or_cache_read( p, t );
        if(e) {
            dump_stack(_err,0);
            _err << " - error reading variable '" << _rvarname << "', error: " << opcd_formatter(e);
            return e;
        }

        if( t.is_array_control_type() )
        {
            if( t._ctrl & type::fARRAY_BEGIN )
            {
                //push_var( _cur_var );
                //if( _cur_var->is_hidden() )
                //    _cur_var = _cur_var->_desc->first_child();
                DASSERT( _cur_var->_desc->_btype._type == t._type );

                return 0;
            }
            else
            {
                //_cur_var = pop_var();
                DASSERT( _cur_var->_desc->_btype._type == t._type );
            }
        }

        return moveto_expected_targetr();
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
/*        if(!_tmetafnc) {
            if( !t.is_primitive() )
                return _hook.write_compound_array_content(c);
            return _fmtstream->write_array_content(c);
        }
*/
        c.set_array_needs_separators();
        type t = c._type;

        if( !t.is_primitive() )
            return _hook.write_compound_array_content(c);
        
        return _fmtstream->write_array_content(c);
    }

    opcd data_read_array_content( binstream_container& c, uints n )
    {
        //if(!_tmetafnc)  return _fmtstream->read_array_content(c,n);

        c.set_array_needs_separators();
        type t = c._type;

        if( !t.is_primitive() )
        {
            if( cache_prepared() )
            {
                DASSERT( n != UMAX );
                for( uints i=0; i<n; ++i )
                {
                    void* p = c.insert(1);
                    if(!p)
                        return ersNOT_ENOUGH_MEM;
                    
                    opcd e = c._stream_in( _hook, p, c );
                    if(e)  return e;

                    --_cache_arysize;
                    _cachetbloffs = read_cache_uint(_cachetbloffs);
                }
                return 0;
            }
            else
                return _hook.read_compound_array_content(c,n);
        }

        //cache contains binary data
        if( cache_prepared() )
        {
            if( t.is_primitive()  &&  c.is_continuous()  &&  n != UMAX )
            {
                uints na = n * t.get_size();
                xmemcpy( c.insert(n), _data.ptr()+_cache_aryoffs, na );
                _cache_arysize -= n;
                //_cache_aryoffs += na;
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

        opcd e;
        if( t.is_next_array_element() && last_var()->is_hidden() )
        {
            _cur_var = pop_var();
        }
        else if( t.is_next_array_element() && !t.is_primitive() )
        {
            //e = moveout_struct(MODE_W);
            e = moveto_expected_targetw(-1);
            if(e)  return e;
        }

        e = _fmtstream->write_array_separator(t,end);
        if(e)  return e;

        if( !end && _cur_var->is_hidden() )
        {
            push_var(_cur_var);
            _cur_var = _cur_var->_desc->first_child();
        }
        else if( !end && !t.is_primitive() )
        {
            //e = movein_struct(MODE_W);
            //if(e)  return e;

            //e = _fmtstream->write_key( _cur_var->_varname );

            e = moveto_expected_targetw(+1);
        }

        return e;
    }

    opcd data_read_array_separator( type t )
    {
        if(!_tmetafnc) {
            _arraynm = 1;
            return _fmtstream->read_array_separator(t);
        }

        opcd e;
        if( t.is_next_array_element() && last_var()->is_hidden() )
        {
            _cur_var = pop_var();
        }
        else if( t.is_next_array_element() && !t.is_primitive() )
        {
            //e = moveout_struct(MODE_R);
            e = moveto_expected_targetr(-1);
            if(e)  return e;
        }

        e = _fmtstream->read_array_separator(t);
        if(e) {
            dump_stack(_err,0);
            _err << " - error reading array separator: " << opcd_formatter(e);
            return e;
        }

        if( _cur_var->is_hidden() )
        {
            push_var(_cur_var);
            _cur_var = _cur_var->_desc->first_child();
        }
        else if( !t.is_primitive() )
        {
            //e = movein_struct(MODE_R);
            //if(e)  return e;

            //e = fmts_or_cache_read_key();
            e = moveto_expected_targetr(+1);
        }

        return e;
    }


    bool cache_prepared() const         { return _cachetbloffs != UMAX; }

    void invalidate_cache_entry()
    {
        if( --_cacherootentries == 0 )
            _data.reset();
        _cachetbloffs = UMAX;
    }

    ///Read data from formatstream or cache
    opcd fmts_or_cache_read( void* p, bstype::type t )
    {
        if( cache_prepared() )
        {
            if( t.is_array_control_type() )
            {
                if( t._ctrl & type::fARRAY_BEGIN )
                {
                    //push previous values of _cache_aryoffs and _cache_arysize
                    _cachetbl.push(_cache_aryoffs);
                    _cachetbl.push(_cache_arysize);
                    _cache_aryoffs = read_cache_uint(_cachetbloffs);
                    _cache_arysize = read_cache_uint(_cache_aryoffs);
                    _cachetbl.push(_cachetbloffs);
                    _cachetbloffs = _cache_aryoffs;

                    *(uints*)p = _cache_arysize;
                }
                else
                {
                    if( _cache_arysize > 0 ) {
                        return ersMISMATCHED "elements left in cached array";
                    }

                    _cachetbl.pop(_cachetbloffs);
                    _cachetbl.pop(_cache_arysize);
                    _cachetbl.pop(_cache_aryoffs);
                }
            }
            else if( t._type == type::T_STRUCTEND )
            {
                _cachetbl.pop(_cachetbloffs);
            }
            else if( t._type == type::T_STRUCTBGN )
            {
                //save current table offset
                uints ct = read_cache_uint(_cachetbloffs);
                _cachetbl.push(_cachetbloffs);

                _cachetbloffs = ct;
            }
            else if( t.is_array_element() )     //array reads
            {
                uints es = t.get_size();
                xmemcpy( p, _data.ptr()+_cache_aryoffs, es );
                _cache_aryoffs += es;
                --_cache_arysize;
            }
            else    //data reads
            {
                uints cio = read_cache_uint(_cachetbloffs);
                xmemcpy( p, _data.ptr()+cio, t.get_size() );
            }

            if( _cachetbloffs == _cachetbloffsquit  &&  (t._ctrl == 0 || (t._ctrl & type::fARRAY_END)) )
                invalidate_cache_entry();

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
                        _err << " - error reading the variable separator, error: " << opcd_formatter(e);
                        return e;
                    }
                }

                e = binstream_read_key( *_fmtstream, _rvarname );
                if(e) {
                    dump_stack(_err,0);
                    _err << " - error while seeking for variable '" << _cur_var->_varname << "', error: " << opcd_formatter(e);
                    return e;
                }

                if( _rvarname != _cur_var->_varname ) {
                    e = cache_fill_member();
                    if(e) {
                        dump_stack(_err,0);
                        _err << " - error while seeking for variable '" << _cur_var->_varname << "', probably not found, error: " << opcd_formatter(e);
                        return e;    //probably the element was not found
                    }
                }
            }
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
            return _data.add(n*_type.get_size());
        }

        const void* extract( uints n )
        {
            const void* p = &_data[ _nactual*_type.get_size() ];
            _nactual +=n;
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
    /// _cur_var->_varname the requested key
    opcd cache_fill_member()
    {
        //here _rvarname can only be another member of _cur_var's parent
        // we should find it and cache its whole structure, as well as any other
        // members until the one pointed to by _cur_var is found
        DESC* par = parent_var()->_desc;

        if( _data.size() > 0 ) {
            DASSERT( _cachelevel == _stack.size() );
        }
        else {
            _data.need_newc( sizeof(uints) * par->num_children(), true );
            _cachelevel = _stack.size();
        }

        do {
            DESC::VAR* crv = par->find_child(_rvarname);
            if(!crv) {
                dump_stack(_err,-1);
                _err << " - member variable: " << _rvarname << " not defined";
                return ersNOT_FOUND "no such member variable";
            }

            uints k = par->get_child_pos(crv);
            if( ((uints*)_data.ptr())[k] != UMAX ) {
                dump_stack(_err,-1);
                _err << " - data for member: " << _rvarname << " specified more than once";
                return ersMISMATCHED "redundant member data";
            }

            ((uints*)_data.ptr())[k] = _data.size();

            opcd e = cache_member(crv);
            if(e) {
                dump_stack(_err,0,crv);
                _err << " - error caching the variable: " << opcd_formatter(e);
                return e;
            }

            ++_cacherootentries;

            e = _fmtstream->read_separator();
            if(e) {
                dump_stack(_err,0);
                _err << " - error reading separator, error: " << opcd_formatter(e);
                return e;
            }

            e = binstream_read_key( *_fmtstream, _rvarname );
            if(e) {
                dump_stack(_err,0);
                _err << " - variable not found '" << _cur_var->_varname << "', error: " << opcd_formatter(e);
                return e;
            }
        }
        while( _rvarname != _cur_var->_varname );

        return 0;
    }

    ///Find _cur_var->_varname in cache
    bool cache_lookup()
    {
        if( _data.size() == 0  ||  _cachelevel < _stack.size() )
            return false;
        
        const uints* dof = (uints*)_data.ptr();
        DESC* par = parent_var()->_desc;

        DESC::VAR* crv = par->find_child(_cur_var->_varname);
        DASSERT(crv);

        uints k = par->get_child_pos(crv);
        if( dof[k] != UMAX )
        {
            //found in cache, set up a cache read
            _cachetbloffs = k * sizeof(uints);
            _cachetbloffsquit = _cachetbloffs + sizeof(uints);
            return true;
        }

        return false;
    }

    ///Read structured data of variable var to the cache.
    opcd cache_member( DESC::VAR* var )
    {
        opcd e;
        if( !var->_desc->_btype.is_primitive() )
        {
            if( var->is_array() )
            {
                type t = var->_desc->_btype;
                //mark the array size first
                uints ona = _data.size();
                _data.add( sizeof(uints) );
                uints na = UMAX;
                uints n = var->_array_size;

                e = _fmtstream->read( &na, t.get_array_begin() );
                if(e) {
                    dump_stack(_err,0,var);
                    _err << " - expecting array start";
                    return e;
                }

                if( na != UMAX  &&  n != UMAX  &&  n != na ) {
                    dump_stack(_err,0);
                    _err << " - array definition expects " << n << " items but found " << na << " items";
                    return ersMISMATCHED "requested and stored count";
                }
                if( na != UMAX )
                    n = na;

                //create a dummy element
                DESC::VAR v;
                //v.is_array = false;
                v._desc = var->_desc;

                type tae = t.get_array_element();

                uints k=0;
                while( n>0 )
                {
                    --n;

                    //peek if there's an element to read
                    if( (e = _fmtstream->read_array_separator(tae)) )
                        break;

                    e = cache_member(&v);
                    if(e) {
                        dump_stack(_err,0,&v);
                        _err << " - error reading the variable: " << opcd_formatter(e);
                        return e;
                    }

                    ++k;

                    type::mask_array_element_first_flag(tae);
                }

                //read trailing mark only for arrays with specified size
                e = _fmtstream->read( 0, t.get_array_end() );
                if(e) {
                    dump_stack(_err,0,var);
                    _err << " - error reading array end, error: " << opcd_formatter(e);
                }
                *(uints*)(_data.ptr()+ona) = k;

                return e;
            }

            bool isarrayelem = var->_varname.is_empty();
            

            //a compound type, pull in
            // write children offset table
            uints n = var->_desc->num_children();
            uints odof = _data.size();
            uints m = isarrayelem ? n+1 : n;
            _data.addc( m * sizeof(uints), true );

            //_rstructname.reset();
            e = _fmtstream->read_struct_open( &var->_desc->_typename );
            if(e) {
                dump_stack(_err,0,var);
                _err << " - expected struct open token, error: " << opcd_formatter(e);
                return e;
            }

            for( uints i=0; i<n; ++i )
            {
                e = _fmtstream->read_separator();

                //read next key
                if(!e) e = binstream_read_key( *_fmtstream, _rvarname );
                if(e) {
                    dump_stack(_err,0);
                    _err << " - error reading variable '" << _rvarname << "', error: " << opcd_formatter(e);
                    return e;
                }

                DESC::VAR* crv = var->_desc->find_child(_rvarname);
                if(!crv) {
                    dump_stack(_err,0,var);
                    _err << " - member variable '" << _rvarname << "' not defined";
                    return ersNOT_FOUND "no such member variable";
                }

                uints k = var->_desc->get_child_pos(crv);
                ((uints*)(_data.ptr()+odof))[k] = _data.size();

                e = cache_member(crv);
                if(e) {
                    dump_stack(_err,0,crv);
                    _err << " - error reading the variable: " << opcd_formatter(e);
                    return e;
                }
            }

            if(isarrayelem)
                ((uints*)(_data.ptr()+odof))[n] = _data.size();

            e = _fmtstream->read_struct_close( &var->_desc->_typename );
            if(e) {
                dump_stack(_err,0,var);
                _err << " - expected struct close token, error: " << opcd_formatter(e);
                return e;
            }
        }
        else
        {
            //a primitive variable, read to the cache directly
            if( var->is_array() )
            {
                //mark the array size first
                uints ona = _data.size();
                _data.add( sizeof(uints) );

                cache_container cc( _data, var->_desc->_btype, var->_array_size );
                e = _fmtstream->read_array(cc);
                *(uints*)(_data.ptr()+ona) = cc._nactual;
            }
            else
            {
                e = _fmtstream->read( _data.add( var->_desc->_btype.get_size() ), var->_desc->_btype );
            }
        }

        return e;
    }

};

////////////////////////////////////////////////////////////////////////////////
/// struct-related defines:
#define MSTRUCT_OPEN(meta, n)       if( !meta.meta_struct_open(n) ) {
#define MM(meta, n, v)              meta.meta_variable(n);  meta << v
#define MMT(meta, n, t)             meta.meta_variable(n);  meta << *(t*)0
#define MMAT(meta, n, t)            meta.meta_array(); meta.meta_variable(n);  meta << *(t*)0
#define MSTRUCT_CLOSE(meta)         meta.meta_struct_close(); }  return meta;

/// building template name:
#define MTEMPL_OPEN(meta)           meta.meta_template_open()
#define MT(meta, T)                 meta << (*(T*)0)
#define MTEMPL_CLOSE(meta)          meta.meta_template_close()



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
