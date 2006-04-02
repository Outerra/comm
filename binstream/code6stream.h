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

#ifndef __COID_COMM_CODE6STREAM__HEADER_FILE__
#define __COID_COMM_CODE6STREAM__HEADER_FILE__

#include "../namespace.h"
#include "binstream.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
class code6stream : public binstream
{
    binstream* _bin;
    int _rstatus;               ///< reading status: 0-reading 1-all read
    uint32 _wval;               ///< temp.output buffer
    uint32 _rval;               ///< temp.input buffer
    int  _wshft;                ///< status of the write buffer (shift pos.into)
    ints  _rrem;                 ///< status of the read buffer (remaining bytes)

public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        uint f = fATTR_IO_FORMATTING | fATTR_HANDSHAKING;
        if( _bin )
            f |= _bin->binstream_attributes(in0out1) & fATTR_READ_UNTIL;
        return f;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        encode( (const uint8*)p, len );

        return len ? ersNO_MORE : (opcd)0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        decode( (uint8*)p, len );

        return len ? ersNO_MORE : (opcd) 0;
    }

    virtual void flush()
    {
        flush_local();
        _bin->flush();
    }

    virtual void acknowledge( bool eat=false )
    {
        acknowledge_local(eat);
        _bin->acknowledge(eat);
    }


    ///set stream status like if everything was read ok
    void set_all_read()
    {
        _rstatus = 1;
        _rrem = 0;
    }

    virtual bool is_open() const
    {
        return _bin->is_open();
    }

    virtual void reset()
    {
        _wshft = 0;
        _wval = 0;
        _bin->reset();
    }

    code6stream()
    {
        _rstatus = 0;
        _wshft = 0;
        _rrem = 0;
        _wval = 0;
    }

    code6stream( binstream& bin )
    {
        _rstatus = 0;
        _wshft = 0;
        _rrem = 0;
        _wval = 0;
        bind(bin);
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX )
    {
        return _bin->read_until( ss, bout, max_size );
    }

    virtual opcd bind( binstream& bin, int io=0 )
    {
        _bin = &bin;
        return 0;
    }

protected:


    void flush_local()
    {
        encode_final();
        _wshft = 0;
    }

    void acknowledge_local( bool eat )
    {
        if(!eat)
        {
            if( !_rstatus  &&  _rrem == 0 )     //nothing read heretofore
                decode3();

            if( !_rstatus || _rrem > 0 )
                throw ersIO_ERROR "data left in input buffer";
        }
        
        _rstatus = 0;
        _rrem = 0;
    }


    bool encode( const uint8* p, uints& len )
    {
        if( len == 0 )  return false;
        if( _wshft  &&  encode1( p, len ) )  return true;
        if( len == 0 )  return false;
        if( _wshft  &&  encode1( p, len ) )  return true;

        while( len >= 3 )
        {
            uint32 k = p[0] + (p[1]<<8) + (p[2]<<16);
            if( encode3(k) )
                return true;
            p += 3;
            len -= 3;
        }

        if( len  &&  encode1( p, len ) )  return true;
        if( len  &&  encode1( p, len ) )  return true;
        return false;
    }

    int encode1( const uint8*& p, uints& len )
    {
        _wval += (uint32)p[0] << _wshft;
        ++p;
        --len;
        _wshft += 8;
        if( _wshft > 16 )
        {
            if( encode3(_wval) )
                return true;
            _wval = 0;
        }
        return false;
    }

    bool encode3( uint32 k )
    {
        _wshft = 0;
        char buf[4];
        buf[0] = 59 + (uint8)(k&0x3f);
        k >>= 6;
        buf[1] = 59 + (uint8)(k&0x3f);
        k >>= 6;
        buf[2] = 59 + (uint8)(k&0x3f);
        k >>= 6;
        buf[3] = 59 + (uint8)k;

        return encode_write(buf);
    }

    bool encode_final()
    {
        char buf[4];
        int k=1;
        buf[0] = '.';
        if( _wshft>0 )  { buf[k++] = 59 + (uint8)(_wval&0x3f);  _wval >>= 6;  _wshft -= 6;  }
        if( _wshft>0 )  { buf[k++] = 59 + (uint8)(_wval&0x3f);  _wval >>= 6;  _wshft -= 6;  }
        if( _wshft>0 )  { buf[k++] = 59 + (uint8)(_wval&0x3f);  _wval >>= 6;  _wshft -= 6;  }
        while(k<4)  buf[k++] = '.';
        
        return encode_write(buf);
    }

    bool encode_write( char* buf )
    {
        uints n=4;
        _bin->write_raw( buf, n );
        return n>0;
    }

    uints decode( uint8* p, uints& len )
    {
        if( len == 0 )  return 0;
        if( _rrem )    decode1( p, len );
        if( len == 0 )  return 0;
        if( _rrem )    decode1( p, len );

        while( len >= 3 )
        {
            uints n = decode3();
            for( uints i=n; i>0; --i )  { *p++ = (uint8)_rval;  _rval>>=8; }
            len -= n;
            _rrem -= n;
            if( n < 3 )     //not enough
                return len;
        }

        if( len )
        {
            uints n = decode3();
            if( n < len )
                return len;     //not enough remaining bytes
            decode1( p, len );
            if( len == 0 )  return 0;
            decode1( p, len );
        }

        return len;
    }

    void decode1( uint8*& p, uints& len )
    {
        *p++ = (uint8)_rval;
        --len;
        _rval >>= 8;
        --_rrem;
    }

    uints decode3()
    {
        if( _rstatus > 0 )
            return 0;
        
        char buf[4];
        uints n=4;
        _bin->read_raw( buf, n );
        if( n>0 )
        { ++_rstatus;  return 0; }

        if( buf[0] == '.' )     //last token
        {
            ++_rstatus;

            //following can be 0, 2 or 3 characters
            // 0 -> 0 bytes, 2 -> 1 byte, 3 -> 2 byte (3 bytes would make whole 4-char token)

            uint32 v = UMAX;
            int t=1;
            if( buf[t] != '.' )  v = (uchar)buf[t++] - 59;
            if( buf[t] != '.' )  v |= ((uchar)buf[t++] - 59) << 6;
            if( buf[t] != '.' )  v |= ((uchar)buf[t++] - 59) << 12;

            if( v == UMAX ) //corrupted data
                return 0;

            _rval = v;
            _rrem = ((t-1)*6)>>3;
            return _rrem;
        }
        
        uint32 v;
        v = (uchar)buf[0] - 59;
        v |= ((uchar)buf[1] - 59) << 6;
        v |= ((uchar)buf[2] - 59) << 12;
        v |= ((uchar)buf[3] - 59) << 18;

        _rval = v;
        _rrem = 3;
        return 3;
    }

};


COID_NAMESPACE_END

#endif //__COID_COMM_CODE6STREAM__HEADER_FILE__

