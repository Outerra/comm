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
///Formatter stream encoding binary data into text by processing 6-bit input
/// binary sequences and representing them as one 8-bit character on output
class enc_base64stream : public binstream
{
    enum {
        WBUFFER_SIZE = 64,      ///< @note MIME requires lines with <76 characters
        RBUFFER_SIZE = 64,
    };

    binstream* _bin;            ///< bound io binstream

    uchar _wbuf[WBUFFER_SIZE+4];///< output buffer, last 4 bytes are for newline characters
    uchar* _wptr;               ///< current position in output buffer
    union {
        uchar _wtar[4];         ///< temp.output buffer
        uint _wval;
    };
    uint _nreq;                 ///< requested bytes for _wval


    uint _rrem;                 ///< remaining bytes in input
    uchar _rbuf[RBUFFER_SIZE];  ///< output buffer, last 4 bytes are for newline characters
    uchar* _rptr;               ///< current position in output buffer
    union {
        uchar _rtar[4];         ///< temp.input buffer
        uint _rval;
    };
    uint _ndec;

public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        uint f = fATTR_IO_FORMATTING | fATTR_HANDSHAKING;
        if(_bin)
            f |= _bin->binstream_attributes(in0out1) & fATTR_READ_UNTIL;
        return f;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        if( len == 0 )
            return 0;

        try { encode( (const uint8*)p, len ); }
        catch(opcd e) { return e; }

        len = 0;
        return (opcd)0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        if( len == 0 )
            return 0;

        opcd e;
        try { e = decode( (uint8*)p, len ); }
        catch(opcd xe) { return xe; }

        if(!e)
            len = 0;
        return e;
    }

    virtual void flush()
    {
        encode_final();
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
        _rrem = 0;
    }

    virtual bool is_open() const
    {
        return _bin->is_open();
    }

    virtual void reset()
    {
        _nreq = 3;
        _wptr = _wbuf;

        _ndec = 0;
        _rptr = _rbuf + RBUFFER_SIZE;
        _rrem = UMAX;

        _bin->reset();
    }

    enc_base64stream()
    {
        _nreq = 3;
        _wptr = _wbuf;

        _ndec = 0;
        _rptr = _rbuf + RBUFFER_SIZE;
        _rrem = UMAX;
    }

    enc_base64stream( binstream& bin )
    {
        _nreq = 3;
        _wptr = _wbuf;

        _ndec = 0;
        _rptr = _rbuf + RBUFFER_SIZE;
        _rrem = UMAX;

        bind(bin);
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX )
    {
        return ersNOT_IMPLEMENTED; //_bin->read_until( ss, bout, max_size );
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
    }

    void acknowledge_local( bool eat )
    {
        if(!eat)
        {
            if( _rrem == UMAX )     //correct end cannot be decided, behave like if it was ok
                _rrem = 0;

            if( _rrem > 0 )
                throw ersIO_ERROR "data left in input buffer";
        }
        
        _rrem = UMAX;
        _rptr = _rbuf + RBUFFER_SIZE;
        _ndec = 0;
    }


private:

    void encode( const uint8* p, uints len )
    {
        for( ; _nreq>0 && len>0; --len )
        {
            --_nreq;
            _wtar[_nreq] = *p++;
        }

        if(_nreq)  return;       //not enough

        encode3();
        _nreq = 3;

        while( len >= 3 )
        {
            _wtar[2] = *p++;
            _wtar[1] = *p++;
            _wtar[0] = *p++;
            len -= 3;

            encode3();
        }

        //here there is less than 3 bytes of input
        switch(len) {
            case 2: _wtar[--_nreq] = *p++;
            case 1: _wtar[--_nreq] = *p++;
        }
    }

    static char enctable( uchar k )
    {
        static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        return table[k];
    }

    void encode3()
    {
        *_wptr++ = enctable( (_wval>>18)&0x3f );
        *_wptr++ = enctable( (_wval>>12)&0x3f );
        *_wptr++ = enctable( (_wval>>6)&0x3f );
        *_wptr++ = enctable( _wval&0x3f );

        if( _wptr >= _wbuf + WBUFFER_SIZE ) {
            _wptr = _wbuf;
            _bin->xwrite_raw( _wbuf, WBUFFER_SIZE );
        }
    }

    void encode_final()
    {
        if( _nreq == 2 )    //2 bytes missing
        {
            _wtar[1] = 0;
            *_wptr++ = enctable( (_wval>>18)&0x3f );
            *_wptr++ = enctable( (_wval>>12)&0x3f );
            *_wptr++ = '=';
            *_wptr++ = '=';
        }
        else if( _nreq == 1 )
        {
            _wtar[0] = 0;
            *_wptr++ = enctable( (_wval>>18)&0x3f );
            *_wptr++ = enctable( (_wval>>12)&0x3f );
            *_wptr++ = enctable( (_wval>>6)&0x3f );
            *_wptr++ = '=';
        }

        if( _wptr > _wbuf ) {
            _bin->xwrite_raw( _wbuf, _wptr-_wbuf );
            _wptr = _wbuf;
        }
        _nreq = 3;
    }

    bool encode_write( char* buf )
    {
        uints n=4;
        _bin->write_raw( buf, n );
        return n>0;
    }


    opcd decode( uint8* p, uints len )
    {
        if( len > _rrem )
            return ersNO_MORE;

        _rrem -= (uint)len;

        for( ; _ndec>0 && len>0; --len )
        {
            --_ndec;
            *p++ = _rtar[_ndec];
        }

        if(_ndec)  return 0;

        while( len >= 3 )
        {
            decode3();

            *p++ = _rtar[2];
            *p++ = _rtar[1];
            *p++ = _rtar[0];
            len -= 3;
        }

        if(len)
        {
            decode_final( (uint)len );
            _ndec = 3;

            //here there is less than 3 bytes required
            switch(len) {
                case 2: *p++ = _rtar[--_ndec];
                case 1: *p++ = _rtar[--_ndec];
            }
        }

        return 0;
    }

    void decode3()
    {
        decode_prefetch();
        if( _rrem < 3 )
            throw ersNO_MORE;
    }

    void decode_final( uint len )
    {
        decode_prefetch();
        if( _rrem < len )
            throw ersNO_MORE;
    }


    ///@return remaining bytes
    int decode_prefetch( uint n=4 )
    {
        if( _rptr >= _rbuf + RBUFFER_SIZE )
        {
            if( _rrem < RBUFFER_SIZE )
                throw ersNO_MORE;
            _rptr = _rbuf;

            uint n = RBUFFER_SIZE;
            if( _bin->read_raw_full( _rbuf, n ) )
                _rrem = ((RBUFFER_SIZE-n)/4)*3;
        }

        _rval = 0;

        for( ; n>0 && _rptr<_rbuf+RBUFFER_SIZE; )
        {
            --n;
            char c = *_rptr++;

            if( c >= 'A' && c <= 'Z' )      _rval |= (c-'A')<<(n*6);
            else if( c >= 'a' && c <= 'z' ) _rval |= (c-'a'+26)<<(n*6);
            else if( c >= '0' && c <= '9' ) _rval |= (c-'0'+2*26)<<(n*6);
            else if( c == '+' ) _rval |= 62<<(n*6);
            else if( c == '/' ) _rval |= 63<<(n*6);
            else if( c == '=' )
                return decode_end(n);
            else
                ++n;
        }

        if(n)
            return decode_prefetch(n);
        return _rrem;
    }

    int decode_end( uint n )
    {
        if(!n)
            _rrem = 2;
        else
        {
            //there should be a second '=' immediatelly
            ++_rptr;
            _rrem = 1;
        }

        return _rrem;
    }

};



COID_NAMESPACE_END

#endif //__COID_COMM_CODE6STREAM__HEADER_FILE__

