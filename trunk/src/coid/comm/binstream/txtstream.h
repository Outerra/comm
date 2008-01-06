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

#ifndef __COID_COMM_TXTSTREAM__HEADER_FILE__
#define __COID_COMM_TXTSTREAM__HEADER_FILE__

#include "../namespace.h"

#include "binstreambuf.h"
#include "../local.h"
#include "../str.h"
#include "../txtconv.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Formatting binstream class to output plain text
class txtstream : public binstream
{
protected:
    binstream*  _binr;
    binstream*  _binw;
    charstr _buf;
    local<binstreambuf> _readbuf;
    token _flush;

    char get_separator() const          { return char(' '); }

public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        uint f = fATTR_IO_FORMATTING;
        if(_binr)
            f |= _binr->binstream_attributes(in0out1) & fATTR_READ_UNTIL;
        return f;
    }

    ///@param s specifies string that should be appended to output upon flush()
    void set_flush_token( const token& s )
    {
        _flush = s;
    }

    virtual opcd write( const void* p, type t )
    {
        ASSERT_RET( _binw, ersUNAVAILABLE "underlying binstream not set" );

        //does no special formatting of arrays
        if( t.is_array_control_type() )
            return 0;

        switch( t.type )
        {
        case type::T_BINARY:
            {
				uint bytes = t.get_size();
                char* d = _buf.get_append_buf( bytes*2 );
                charstrconv::bin2hex( p, d, 1, bytes, get_separator() );
                break;
            }
        case type::T_INT:
            {
                _buf.append_num_int( 10, p, t.get_size() );
                break;
            }
        case type::T_UINT:
            {
                _buf.append_num_uint( 10, p, t.get_size() );
                break;
            }
        case type::T_FLOAT:
            {
                switch( t.get_size() )
                {
                case 4: _buf << *(float*)p;
                        break;
                case 8: _buf << *(double*)p;
                        break;
                default:
                    return ersINVALID_TYPE "unsupported size";
                }
                break;
            }
        case type::T_KEY:
        case type::T_CHAR:
            _buf.add_from( (const char*)p, t.get_size() );
            break;

        case type::T_ERRCODE:
            _buf << char('[') << opcd_formatter((const opcd::errcode*)p) << char(']');
            break;
        }

        if( !_buf.is_empty() )
        {
            uints bl = _buf.len();
            opcd e = _binw->write_raw( _buf.ptr(), bl );
            if(e)  return e;
            _buf.reset();
        }

        return 0;
    }

    virtual opcd read( void* p, type t )
    {
        ASSERT_RET( _binr, ersUNAVAILABLE "underlying binstream not set" );

        //does no special formatting of arrays
        if( t.is_array_control_type() )
            return 0;

        //since the text output is plain, without any additional marks that
        // can be used to determine the type, we can only read text
        //for anything more sophisticated use class textparstream instead

        if( t.type == type::T_CHAR  ||  t.type == type::T_KEY )
            return _binr->read( p, t );
        else
            return ersUNAVAILABLE;
    }

    virtual opcd write_raw( const void* p, uints& len )      { return _binw->write_raw( p, len ); }
    virtual opcd read_raw( void* p, uints& len )             { return _binr->read_raw( p, len ); }

    virtual opcd write_array_content( binstream_container& c, uints* count )
    {
        type t = c._type;
        uints n = c._nelements;

        //types other than char and key must be written by elements
        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY )
            return write_compound_array_content(c,count);

        opcd e;
        if( c.is_continuous()  &&  n != UMAX )
        {
            //n *= t.get_size();
            e = write_raw( c.extract(n), n );

            if(!e)  *count = n;
        }
        else
            e = write_compound_array_content(c,count);

        return e;
    }

    virtual opcd read_array_content( binstream_container& c, uints n, uints* count )
    {
        type t = c._type;
        //uints n = c._nelements;

        if( t.type != type::T_CHAR  &&  t.type != type::T_KEY )
            return ersUNAVAILABLE;

        opcd e=0;
        if( c.is_continuous()  &&  n != UMAX )
        {
            //uints na = n * t.get_size();
            e = read_raw( c.insert(n), n );

            if(!e)  *count = n;
        }
        else
        {
            uints es=1, k=0;
            char ch;
            while( n-- > 0  &&  0 == read_raw( &ch, es ) ) {
                char* p = (char*)c.insert(1);
                if(!p)  return ersNOT_ENOUGH_MEM;

                *p = ch;
                es = 1;
                ++k;
            }

            *count = k;
        }

        return e;
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX )
    {
        return _binr->read_until( ss, bout, max_size );
    }

    virtual opcd peek_read( uint timeout )  { return _binr->peek_read(timeout); }
    virtual opcd peek_write( uint timeout ) { return _binw->peek_write(timeout); }

    virtual opcd bind( binstream& bin, int io=0 )
    {
        if( io<0 )
            _binr = &bin;
        else if( io>0 )
            _binw = &bin;
        else
            _binr = _binw = &bin;
        return 0;
    }

    virtual opcd open( const token& arg )
    {
        return _binw->open(arg);
    }

    virtual opcd close( bool linger=false )
    {
        return _binw->close(linger);
    }

    virtual bool is_open() const    { return _binr->is_open (); }
    virtual void flush()
    {
        if(_binw)
        {
            if( !_flush.is_empty() )
                _binw->xwrite_token_raw(_flush);
            _binw->flush();
        }
    }
    virtual void acknowledge (bool eat=false)
    {
        if(_binr)
            _binr->acknowledge(eat);
    }

    virtual void reset_read()
    {
        _binr->reset_read();
    }

    virtual void reset_write()
    {
        _buf.reset();
        _binw->reset_write();
    }

    void assign (binstream* br, binstream* bw=0)
    {
        _binr = br;
        _binw = bw ? bw : br;
    }

    void assign (charstr& str, binstream* bw=0)
    {
        _readbuf = new binstreambuf (str);
        _binr = _readbuf;
        _binw = bw ? bw : _binr;
    }

    void assign_read_buffer( charstr& str )
    {
        _readbuf = new binstreambuf(str);
        _binr = _readbuf;
    }


    ///Set size of internal write buffer
    void reserve_buf( uints size )
    {
        _buf.reserve(size);
    }


    binstreambuf* get_read_buffer() const   { return _readbuf; }


    void create_internal_buffer( binstream* bw = 0 )
    {
        _readbuf = new binstreambuf;
        _binr = _readbuf;
        _binw = bw ? bw : _binr;
    }

    txtstream (binstream& b) : _binr(&b), _binw(&b)
    {   _flush = ""; }
    txtstream (binstream* br, binstream* bw=0) : _binr(br), _binw(bw == 0 ? br : bw)
    {   _flush = ""; }
    txtstream ()
    {   _binr = _binw = 0;  _flush = ""; }

    txtstream (charstr& str, binstream* bw = 0)
    {
        _readbuf = new binstreambuf(str);
        _binr = _readbuf;
        _binw = bw ? bw : _binr;
    }

    txtstream (const token& str, binstream* bw = 0)
    {
        _readbuf = new binstreambuf(str);
        _binr = _readbuf;
        _binw = bw ? bw : _binr;
    }

    ~txtstream ()
    {
//        flush ();
    }
};

COID_NAMESPACE_END

#endif //__COID_COMM_TXTSTREAM__HEADER_FILE__

