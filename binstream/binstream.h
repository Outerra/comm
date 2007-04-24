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

#ifndef __COID_COMM_BINSTREAM__HEADER_FILE__
#define __COID_COMM_BINSTREAM__HEADER_FILE__

#include "../namespace.h"

#include "container.h"

#include "../retcodes.h"
#include "../assert.h"

COID_NAMESPACE_BEGIN


void* dynarray_new( void* p, uints nitems, uints itemsize, uints ralign = 0 );

////////////////////////////////////////////////////////////////////////////////
///Binstream base class
/**
    virtual opcd write( const void* p, type t );
    virtual opcd read( void* p, type t );
    virtual opcd write_raw( const void* p, uints& len ) = 0;
    virtual opcd read_raw( void* p, uints& len ) = 0;
    virtual opcd write_array_content( binstream_container& c );
    virtual opcd read_array_content( binstream_container& c, uints n );

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) = 0;

    virtual opcd open( const token& arg );
    virtual opcd close( bool linger=false );
    virtual bool is_open() const = 0;
    virtual opcd bind( binstream& bin, int io );
    
    virtual void flush() = 0;
    virtual void acknowledge( bool eat=false ) = 0;

    virtual void reset();
    virtual opcd set_timeout( uints ms );

    virtual opcd seek( int seektype, int64 pos );
    virtual uints get_size() const;
    virtual uints set_size( ints n );
    virtual opcd overwrite_raw( uints pos, const void* data, uints len );
**/
class binstream
{
    template<class T>
    struct stream_wrapper
    {
        static void to_stream( binstream& bin, const void* p )      { bin << *(const T*)p; }
        static void from_stream( binstream& bin, void* p )          { bin >> *(T*)p; }
    };

public:

    typedef bstype::type      type;
    typedef bstype::key       key;

    virtual ~binstream()        { }

    enum {
        fATTR_NO_INPUT_FUNCTION         = 0x01,     ///< cannot use input (read) functions
        fATTR_NO_OUTPUT_FUNCTION        = 0x01,     ///< cannot use output (write) functions

        fATTR_IO_FORMATTING             = 0x02,
        fATTR_OUTPUT_FORMATTING         = 0x02,     ///< formatting stream wrapper (text)
        fATTR_INPUT_FORMATTING          = 0x02,     ///< parsing stream wrapper
        
        fATTR_SIMPLEX                   = 0x04,     ///< cannot simultaneously use input and output
        fATTR_HANDSHAKING               = 0x08,     ///< uses flush/acknowledge semantic

        fATTR_REVERTABLE                = 0x10,     ///< output can be revertable (until flushed)
        fATTR_READ_UNTIL                = 0x20,     ///< supports read_until() function
    };
    virtual uint binstream_attributes( bool in0out1 ) const = 0;


    typedef bstype::STRUCT_OPEN         STRUCT_OPEN;
    typedef bstype::STRUCT_CLOSE        STRUCT_CLOSE;

    typedef bstype::ARRAY_OPEN          ARRAY_OPEN;
    typedef bstype::ARRAY_CLOSE         ARRAY_CLOSE;
    typedef bstype::ARRAY_ELEMENT       ARRAY_ELEMENT;

    typedef bstype::BINSTREAM_FLUSH     BINSTREAM_FLUSH;
    typedef bstype::BINSTREAM_ACK       BINSTREAM_ACK;
    typedef bstype::BINSTREAM_ACK_EAT   BINSTREAM_ACK_EAT;

    //typedef bstype::SEPARATOR           SEPARATOR;


    binstream& operator << (const BINSTREAM_FLUSH&)     { flush();  return *this; }
    binstream& operator >> (const BINSTREAM_ACK&)   	{ acknowledge();  return *this; }
    binstream& operator >> (const BINSTREAM_ACK_EAT&)   { acknowledge(true);  return *this; }

    binstream& operator << (const STRUCT_OPEN&)     	{ return xwrite(0, bstype::t_type<STRUCT_OPEN>() ); }
    binstream& operator << (const STRUCT_CLOSE&)    	{ return xwrite(0, bstype::t_type<STRUCT_CLOSE>() ); }

    binstream& operator >> (const STRUCT_OPEN&)         { return xread(0, bstype::t_type<STRUCT_OPEN>() ); }
    binstream& operator >> (const STRUCT_CLOSE&)        { return xread(0, bstype::t_type<STRUCT_CLOSE>() ); }

    //binstream& operator << (const SEPARATOR a)          { return xwrite( &a._s, bstype::t_type<SEPARATOR>() ); }

    binstream& operator << (opcd x)
    {
		if( !x._ptr )
			xwrite( &x._ptr, bstype::t_type<opcd>() );
		else
			xwrite( x._ptr, bstype::t_type<opcd>() );
        return *this;
    }
/*
    binstream& operator << (const opcd::errcode* x)
    {
        write( x, 1, type::t_ercd );
        return *this;
    }
*/

    binstream& operator << (key x)                      { return xwrite(&x, bstype::t_type<key>() ); }

    binstream& operator << (bool x)                     { return xwrite(&x, bstype::t_type<bool>() ); }

    binstream& operator << (const char* x )             { return xwrite_token(x); }
    binstream& operator << (const unsigned char* x)     { return xwrite_token((const char*)x); }
    binstream& operator << (const signed char* x)       { return xwrite_token((const char*)x); }

    binstream& operator << (uint8 x )                   { return xwrite(&x, bstype::t_type<uint8>() ); }
    binstream& operator << (int8 x )                    { return xwrite(&x, bstype::t_type<int8>() ); }
    binstream& operator << (int16 x )                   { return xwrite(&x, bstype::t_type<int16>() ); }
    binstream& operator << (uint16 x )                  { return xwrite(&x, bstype::t_type<uint16>() ); }
    binstream& operator << (int32 x )                   { return xwrite(&x, bstype::t_type<int32>() ); }
    binstream& operator << (uint32 x )                  { return xwrite(&x, bstype::t_type<uint32>() ); }
    binstream& operator << (int64 x )                   { return xwrite(&x, bstype::t_type<int64>() ); }
    binstream& operator << (uint64 x )                  { return xwrite(&x, bstype::t_type<uint64>() ); }

    binstream& operator << (char x)                     { return xwrite(&x, bstype::t_type<char>() ); }

#ifdef _MSC_VER
    binstream& operator << (ints x )                    { return xwrite(&x, bstype::t_type<int>() ); }
    binstream& operator << (uints x )                   { return xwrite(&x, bstype::t_type<uint>() ); }
#else
    binstream& operator << (long x )                    { return xwrite(&x, bstype::t_type<long>() ); }
    binstream& operator << (ulong x )                   { return xwrite(&x, bstype::t_type<ulong>() ); }
#endif //_MSC_VER    
    
    binstream& operator << (float x )                   { return xwrite(&x, bstype::t_type<float>() ); }
    binstream& operator << (double x )                  { return xwrite(&x, bstype::t_type<double>() ); }
    binstream& operator << (long double x )             { return xwrite(&x, type(type::T_FLOAT,16) ); }

    binstream& operator << (const timet& x)             { return xwrite(&x, bstype::t_type<timet>() ); }

    binstream& operator >> (const char*& x )            { binstream_container_char_array c(UMAX); xread_array(c); x=c.get(); return *this; }
    binstream& operator >> (char*& x )                  { binstream_container_char_array c(UMAX); xread_array(c); x=(char*)c.get(); return *this; }


    binstream& operator >> (key x)                      { return xread(&x, bstype::t_type<key>() ); }

    binstream& operator >> (bool& x )                   { return xread(&x, bstype::t_type<bool>() ); }

    binstream& operator >> (uint8& x )                  { return xread(&x, bstype::t_type<uint8>() ); }
    binstream& operator >> (int8& x )                   { return xread(&x, bstype::t_type<int8>() ); }
    binstream& operator >> (int16& x )                  { return xread(&x, bstype::t_type<int16>() ); }
    binstream& operator >> (uint16& x )                 { return xread(&x, bstype::t_type<uint16>() ); }
    binstream& operator >> (int32& x )                  { return xread(&x, bstype::t_type<int32>() ); }
    binstream& operator >> (uint32& x )                 { return xread(&x, bstype::t_type<uint32>() ); }
    binstream& operator >> (int64& x )                  { return xread(&x, bstype::t_type<int64>() ); }
    binstream& operator >> (uint64& x )                 { return xread(&x, bstype::t_type<uint64>() ); }

    binstream& operator >> (char& x )                   { return xread(&x, bstype::t_type<char>() ); }

#ifdef _MSC_VER
    binstream& operator >> (ints& x )                   { return xread(&x, bstype::t_type<int>() ); }
    binstream& operator >> (uints& x )                  { return xread(&x, bstype::t_type<uint>() ); }
#else
    binstream& operator >> (long& x )                   { return xread(&x, bstype::t_type<long>() ); }
    binstream& operator >> (ulong& x )                  { return xread(&x, bstype::t_type<ulong>() ); }
#endif //_MSC_VER    
    
    binstream& operator >> (float& x )                  { return xread(&x, bstype::t_type<float>() ); }
    binstream& operator >> (double& x )                 { return xread(&x, bstype::t_type<double>() ); }
    binstream& operator >> (long double& x )            { return xread(&x, type(type::T_FLOAT,16) ); }

    binstream& operator >> (timet& x)                   { return xread(&x, bstype::t_type<timet>() ); }


    binstream& operator >> (opcd& x)
    {
		ushort e;
        xread( &e, bstype::t_type<opcd>() );
		x.set(e);
        return *this;
    }

    ///Helper function used find the string lenght, but maximally inspect n characters
    static uints strnlen( const char* str, uints n )
    {
        uints i;
        for( i=0; i<n; ++i )
            if( str[i] == 0 ) break;
        return i;
    }


    ////////////////////////////////////////////////////////////////////////////////
    opcd write_separator()                              { return write( 0, type( type::T_SEPARATOR, 0 ) ); }
    opcd read_separator()                               { return read( 0, type( type::T_SEPARATOR, 0 ) ); }

    void xwrite_separator()                             { xwrite( 0, type( type::T_SEPARATOR, 0 ) ); }
    void xread_separator()                              { xread( 0, type( type::T_SEPARATOR, 0 ) ); }

    ////////////////////////////////////////////////////////////////////////////////
    ///Write character token
    opcd write_token( const token& x )
    {
        binstream_container_fixed_array<char> c((char*)x.ptr(), x.len());
        return write_array(c);
    }
    
    ///Write key token
    opcd write_key( const token& x )
    {
        binstream_container_fixed_array<key> c((key*)x.ptr(), x.len());
        return write_array(c);
    }

    opcd write_token( const char* p, uints len )
    {
        binstream_container_fixed_array<char> c((char*)p, len);
        return write_array(c);
    }
    
    ///Write key token
    opcd write_key( const char* p, uints len )
    {
        binstream_container_fixed_array<key> c((key*)p, len);
        return write_array(c);
    }

    binstream& xwrite_token( const token& x )           { opcd e = write_token(x);  if(e) throw e; return *this; }
    binstream& xwrite_key( const token& x )             { opcd e = write_key(x);  if(e) throw e; return *this; }
    binstream& xwrite_token( const char* p, uints len ) { opcd e = write_token(p,len);  if(e) throw e; return *this; }
    binstream& xwrite_key( const char* p, uints len )   { opcd e = write_key(p,len);  if(e) throw e; return *this; }
    

    ////////////////////////////////////////////////////////////////////////////////
    ///Write single primitive type
    virtual opcd write( const void* p, type t )
    {
        if( t.is_no_size() )
            return 0;

        uints s = t.get_size();
        return write_raw(p,s);
    }

    ///Read single primitive type
    virtual opcd read( void* p, type t )
    {
        if( t.is_no_size() )
            return 0;

        uints s = t.get_size();
        return read_raw_full(p,s);
    }

    ///A write() wrapper throwing exception on error
    binstream& xwrite( const void* p, type t )      { opcd e = write(p,t);  if(e) throw e;  return *this; }

    ///A read() wrapper throwing exception on error
    binstream& xread( void* p, type t )             { opcd e = read(p,t);   if(e) throw e;  return *this; }


    ///Write single primitive type deducing the argument type
    template<class T>
    opcd write_type( const T* v )
    {
        bstype::t_type<T> t;
        if(t.is_primitive())
            return write( v, t );
        else
            try { (*this) << *v; } catch(opcd e) { return e; }
        return 0;
    }


    ///Read single primitive type deducing the argument type
    template<class T>
    opcd read_type( T* v )
    {
        bstype::t_type<T> t;
        if(t.is_primitive())
            return read( v, t );
        else
            try { (*this) >> *v; } catch(opcd e) { return e; }
        return 0;
    }


    template< class T >
    opcd write_ptr( const T* p )                    { return write( (void*)&p, type(type::T_UINT,sizeof(void*)) ); }

    template< class T >
    opcd read_ptr( T*& p )                          { return read( (void*)&p, type(type::T_UINT,sizeof(void*)) ); }


    opcd write_struct_open( const charstr* name=0 )     { type t(type::T_STRUCTBGN);  return write( name, t ); }
    opcd write_struct_close( const charstr* name=0 )    { type t(type::T_STRUCTEND);  return write( name, t ); }

    opcd read_struct_open( charstr* name=0 )        { type t(type::T_STRUCTBGN);  return read( name, t ); }
    opcd read_struct_close( charstr* name=0 )       { type t(type::T_STRUCTEND);  return read( name, t ); }

    opcd write_compound_array_open( const charstr* name=0 )     { type t(type::T_COMPOUND,0,type::fARRAY_BEGIN);  return write( name, t ); }
    opcd write_compound_array_close( const charstr* name=0 )    { type t(type::T_COMPOUND,0,type::fARRAY_END);  return write( name, t ); }

    opcd read_compound_array_open( charstr* name=0 )    { type t(type::T_COMPOUND,0,type::fARRAY_BEGIN);  return read( name, t ); }
    opcd read_compound_array_close( charstr* name=0 )   { type t(type::T_COMPOUND,0,type::fARRAY_END);  return read( name, t ); }

    ////////////////////////////////////////////////////////////////////////////////
    ///Write raw data
    /// @p len contains number of bytes to write, on return the number of bytes remaining to write
    /// @return 0 (no error), ersNO_MORE when not all data could be read
    virtual opcd write_raw( const void* p, uints& len ) = 0;

    ///Read raw data
    /// @p len contains number of bytes to read, on return the number of bytes remaining to read
    /// @return 0 (no error), ersRETRY when only partial chunk was returned and the method should be called again, ersNO_MORE when not
    ///         all data has been read
    virtual opcd read_raw( void* p, uints& len ) = 0;

    ///Write raw data.
    /// @note This method is provided just for the symetry, write_raw specification doesn't allow returning ersRETRY error code
    ///       to specify that only partial data has been written, this may change in the future if it turns out being needed
    opcd write_raw_full( const void* p, uints& len )
    {
        return write_raw(p,len);
    }

    ///Read raw data repeatedly while ersRETRY is returned from read_raw
    opcd read_raw_full( void* p, uints& len )
    {
        for(;;) {
            uints olen = len;
            opcd e = read_raw(p,len);
            if( e != ersRETRY ) return e;

            p = (char*)p + (olen - len);
        }
    }

    ///A write_raw() wrapper throwing exception on error
    ///@return number of bytes remaining to write
    uints xwrite_raw( const void* p, uints len )
    {
        opcd e = write_raw( p, len );
        if(e)  throw e;
        return len;
    }

    ///A read_raw() wrapper throwing exception on error
    ///@return number of bytes remaining to read
    uints xread_raw( void* p, uints len )
    {
        opcd e = read_raw_full( p, len );
        if(e)  throw e;
        return len;
    }


    opcd read_raw_scrap( uints& len )
    {
        uchar buf[256];
        while( len ) {
            uints lb = len>256 ? 256 : len;
            uints lo = lb;
            opcd e = read_raw_full( buf, lb );
            if(e)  return e;

            len -= lo;
        }
        return 0;
    }

    uints xread_raw_scrap( uints len )
    {
        opcd e = read_raw_scrap( len );
        if(e)  throw e;
        return len;
    }

    //@{ Array manipulating methods.
    /**
        Methods to write and read arrays of objects. The write_array() and read_array()
        methods take as the argument a container object.
    **/

    ////////////////////////////////////////////////////////////////////////////////
    ///Write array from container \a c to this binstream
    opcd write_array( binstream_container& c )
    {
        uints n = c._nelements;
        type t = c._type;

        //write bgnarray
        opcd e = write( &n, t.get_array_begin() );
        if(e)  return e;

        //if container doesn't know number of items in advance, require separators
        if( n == UMAX )
            t = c.set_array_needs_separators();

        e = write_array_content(c);

        if(!e)
            e = write( 0, t.get_array_end() );

        return e;
    }

    ///Read array to container \a c from this binstream
    opcd read_array( binstream_container& c )
    {
        uints na = UMAX;
        uints n = c._nelements;
        type t = c._type;

        //read bgnarray
        opcd e = read( &na, t.get_array_begin() );
        if(e)  return e;

        if( na != UMAX  &&  n != UMAX  &&  n != na )
            return ersMISMATCHED "requested and stored count";
        if( na != UMAX )
            n = na;
        else
            t = c.set_array_needs_separators();

        e = read_array_content( c, n );

        //read trailing mark only for arrays with specified size
        if(!e)
            e = read( 0, t.get_array_end() );

        return e;
    }

    virtual opcd write_array_separator( type t, uchar end )
    {
        uints ms = sizeof(uchar);
        opcd e = write_raw( &end, ms );
        return e;
    }

    virtual opcd read_array_separator( type t )
    {
        uchar m;
        uints ms = sizeof(uchar);
        opcd e = read_raw( &m, ms );
        if(e)  return e;
        if(m)  return ersNO_MORE;
        return e;
    }

    ///Overloadable method for writing array objects
    /// to binstream.
    ///The default code is for writing a binary representation
    virtual opcd write_array_content( binstream_container& c )
    {
        type t = c._type;
        uints n = c._nelements;

        if( t.is_primitive()  &&  c.is_continuous()  &&  n != UMAX )
        {
            DASSERT( !t.is_no_size() );

            uints na = n * t.get_size();
            return write_raw( c.extract(n), na );
        }
        else
            return write_compound_array_content(c);
    }

    ///Overloadable method for reading array of objects
    /// from binstream.
    ///The default code is for reading a binary representation
    virtual opcd read_array_content( binstream_container& c, uints n )
    {
        type t = c._type;
        //uints n = c._nelements;

        if( t.is_primitive()  &&  c.is_continuous()  &&  n != UMAX )
        {
            DASSERT( !t.is_no_size() );

            uints na = n * t.get_size();
            return read_raw_full( c.insert(n), na );
        }
        else
            return read_compound_array_content( c, n );
    }


    ///
    opcd write_compound_array_content( binstream_container& c )
    {
        type tae = c._type.get_array_element();
        uints n = c._nelements;
        bool complextype = !c._type.is_primitive();
        bool needpeek = c.array_needs_separators();

        opcd e;
        while( n>0 )
        {
            --n;

            const void* p = c.extract(1);
            if(!p)
                return ersNOT_ENOUGH_MEM;

            if( needpeek && (e = write_array_separator(tae,0)) )
                break;

            if(complextype)
                e = c._stream_out( *this, (void*)p, c );
            else
                e = write( p, tae );

            if(e)
                return e;

            type::mask_array_element_first_flag(tae);
        }

        if(needpeek)
            e = write_array_separator(tae,1);

        return e;
    }

    ///Read array of objects using provided streaming function
    /** Contrary to its name, it can be used to read a primitive elements too, when
        the container isn't continuous or its size is not known in advance
    **/
    opcd read_compound_array_content( binstream_container& c, uints n )
    {
        type tae = c._type.get_array_element();
        bool complextype = !c._type.is_primitive();
        bool needpeek = c.array_needs_separators();

        opcd e;
        while( n>0 )
        {
            --n;

            //peek if there's an element to read
            if( needpeek && (e = read_array_separator(tae)) )
                break;

            void* p = c.insert(1);
            if(!p)
                return ersNOT_ENOUGH_MEM;

            if(complextype)
                e = c._stream_in( *this, p, c );
            else
                e = read( p, tae );

            if(e)
                return e;

            type::mask_array_element_first_flag(tae);
        }

        return 0;
    }


    ///A write_array() wrapper throwing exception on error
    binstream& xwrite_array( binstream_container& s )      { opcd e = write_array(s);  if(e) throw e;  return *this; }

    ///A read_array() wrapper throwing exception on error
    binstream& xread_array( binstream_container& s )       { opcd e = read_array(s);  if(e) throw e;  return *this; }


    template< class T >
    opcd write_fixed_array( const T* p, uints n )
    {
        binstream_container_fixed_array<T> c((T*)p,n);
        return write_array_content(c);
    }

    template< class T >
    opcd read_fixed_array( T* p, uints n )
    {
        binstream_container_fixed_array<T> c(p,n);
        return read_array_content(c,n);
    }

    template< class T >
    opcd write_linear_array( const T* p, uints n )
    {
        binstream_container_fixed_array<T> c((T*)p,n);
        return write_array(c);
    }


    
	////////////////////////////////////////////////////////////////////////////////
    ///Read until @p ss substring is read or @p max_size bytes received
    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) = 0;

    ///A convenient function to read one line of input with binstreams that support read_until()
    opcd read_line( binstream* bout, uints max_size=UMAX )
    {
        static substring ss_newline("\n");
        return read_until( ss_newline, bout, max_size );
    }
    

    ///Open underlying medium
    virtual opcd open( const token& arg )           { return ersNOT_IMPLEMENTED; }
    ///Close underlying medium
    virtual opcd close( bool linger=false )         { return ersNOT_IMPLEMENTED; }
    ///Check if the underlying medium is open
    virtual bool is_open() const = 0;
    
    ///Bind to another binstream (for wrapper and formatting binstreams)
    /// @param io which stream to bind (input or output one), can be ignored if not supported
    virtual opcd bind( binstream& bin, int io=0 )   { return ersNOT_IMPLEMENTED; }
    
    ///io argument to bind method
    /// negative values used to bind input streams, positive values used to bind output streams
    enum {
        BIND_ALL        = 0,            ///< bind all bindable paths to given binstream
        BIND_INPUT      = -1,           ///< bind input path
        BIND_OUTPUT     =  1, };        ///< bind output path

    
    ///Flush pending output data. Binstreams that use flush/ack synchronization use it to signal
    /// the end of the current write packet, that must be matched by an acknowledge() when reading
    /// from the binstream.
    virtual void flush() = 0;

    ///In binstreams that use the flush/ack synchronization, acknowledge that all data from current
    /// packet has been read. Throws an exception ersIO_ERROR if there are any data left unread.
    /// Note that in such streams, reading more data than the packet contains will result in
    /// the read() methods returning error ersNO_MORE, or xread() methods throwing the exception
    /// (opcd) ersNO_MORE.
    ///@param eat forces the binstream to eat all remaining data from the packet
    virtual void acknowledge( bool eat=false ) = 0;

    ///Completely reset the binstream. By default resets both reading and writing pipe, but can do more.
    virtual void reset_all()
    {
        reset_write();
        reset_read();
    }

    ///Reset the binstream to the initial state for reading. Does nothing on stateless binstreams.
    virtual void reset_read() = 0;

    ///Reset the binstream to the initial state for writing. Does nothing on stateless binstreams.
    virtual void reset_write() = 0;

    ///Set read and write operation timeout value. Various, mainly network binstreams use it to return 
    /// ersTIMEOUT error or throw the ersTIMEOUT opcd object
    virtual opcd set_timeout( uint ms )
    {
        return ersNOT_IMPLEMENTED;
    }


    ///Seek type flags
    enum { fSEEK_READ=1, fSEEK_WRITE=2, fSEEK_CURRENT=4, };
    ///Seek within the binstream
    virtual opcd seek( int seektype, int64 pos )    { return ersNOT_IMPLEMENTED; }

    //@{ Methods for revertable streams
    /**
        These methods are supported on binstreams that can manipulate data already pushed into the
        stream. For example, some protocol may require the size of body written in a header, before
        the body alone, but the size may not be known in advance. So one could remember the position
        where the size should be written with get_size(), write a placeholder data there. Before
        writing the body remember the offset with get_size(), write the body itself and compute the 
        size by subtracting the starting offset from current get_size() value.
        Afterwards use overwrite_raw to write actual size at the placeholder position.
    **/
    
    ///Get written amount of bytes
    virtual uint64 get_size() const                 { return UMAX; }
    
    ///Cut to specified length, negative numbers cut abs(len) from the end
    virtual uint64 set_size( int64 n )              { return UMAX; }

    ///Return actual pure data size written from specified offset
    ///This can be overwritten by binstreams that insert additional data into stream (like packet headers etc.)
    virtual uint64 get_size_pure( uint64 from ) const
    {
        return get_size() - from;
    }

    ///Overwrite stream at position \a pos with data from \a data of length \a len
    virtual opcd overwrite_raw( uint64 pos, const void* data, uints len )   { return ersNOT_IMPLEMENTED; }
    
    //@}


    static void* bufcpy( void* dst, const void* src, uints count )
    {
        switch(count) {
            case 1: *(uint8*)dst = *(uint8*)src;  break;
            case 2: *(uint16*)dst = *(uint16*)src;  break;
            case 4: *(uint32*)dst = *(uint32*)src;  break;
            case 8: *(uint64*)dst = *(uint64*)src;  break;
            default:  xmemcpy( dst, src, count );
        }
        return dst;
    }
};

////////////////////////////////////////////////////////////////////////////////

#define BINSTREAM_FLUSH     binstream::BINSTREAM_FLUSH()
#define BINSTREAM_ACK       binstream::BINSTREAM_ACK()
#define BINSTREAM_ACK_EAT   binstream::BINSTREAM_ACK_EAT()

//#define STRUCT_OPEN         binstream::STRUCT_OPEN()
//#define STRUCT_CLOSE        binstream::STRUCT_CLOSE()


////////////////////////////////////////////////////////////////////////////////
struct opcd_formatter
{
    opcd _e;

    opcd_formatter( opcd e ) : _e(e) { }

    charstr& text( charstr& dst ) const;

    friend binstream& operator << (binstream& out, const opcd_formatter& f)
    {
        out << f._e.error_desc();

        if( f._e.text() && f._e.text()[0] )
            out << " : " << f._e.text();
        return out;
    }
};

////////////////////////////////////////////////////////////////////////////////
inline binstream& operator >> (binstream& from, binstream& to)
{
    uchar buf[4096];
    for (;;)
    {
        uints len = 4096;
        from.read_raw_full( buf, len );
        to.xwrite_raw( buf, 4096 - len );
        if( len > 0 )
            break;
    }
    return from;
}

////////////////////////////////////////////////////////////////////////////////
inline binstream& operator << (binstream& to, binstream& from)
{
    from >> to;
    return to;
}

////////////////////////////////////////////////////////////////////////////////
template<bool FLUSHACK=false>
class binstream_redirect
{
	binstream* _ref;
	bool _owned;

public:
    binstream_redirect( binstream& bin ) : _ref(&bin)
    {
		_owned = false;
    }

    binstream_redirect()
    {
        _owned = false; _ref = 0;
    }

    ~binstream_redirect()
    {
        if( _owned && _ref ) delete _ref;
    }

    void set( binstream& bin )
    {
        if( _owned && _ref ) delete _ref;
		_owned = false;
        _ref = &bin;
    }

    void set_ptr( binstream* bin )
    {
        if( _owned && _ref ) delete _ref;
		_owned = true;
        _ref = bin;
    }

    enum {
        BUFFER_SIZE             = 4096,
    };

    friend binstream& operator << (binstream& out, binstream_redirect& o)
    {
        if (!o._ref)
            throw ersUNAVAILABLE "source binstream not set";

        uchar buf[BUFFER_SIZE];
        for(;;)
        {
            uints len = BUFFER_SIZE;
            o._ref->read_raw( buf, len );
            
            uints alen = BUFFER_SIZE - len;
            if(!FLUSHACK)
                out.xwrite_raw( &alen, sizeof(alen) );
            out.xwrite_raw( buf, alen );
            if( len > 0 )
                break;
        }

        if(FLUSHACK)
            out.flush();
        return out;
    }

    friend binstream& operator >> (binstream& in, binstream_redirect& o)
    {
        if (!o._ref)
            throw ersUNAVAILABLE "destination binstream not set";

        uchar buf[BUFFER_SIZE];
        for (;;)
        {
            uints len = BUFFER_SIZE;
            if(!FLUSHACK)
                in.xread_raw( &len, sizeof(len) );

            uints alen = len;
            in.read_raw( buf, len );

            o._ref->xwrite_raw( buf, alen - len );
            if( len > 0 )
                break;
        }

        if(FLUSHACK)
            in.acknowledge();
        return in;
    }
};

COID_NAMESPACE_END

#endif //__COID_COMM_BINSTREAM__HEADER_FILE__
