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

#ifndef __COMM_HEXSTREAM__HEADER_FILE__
#define __COMM_HEXSTREAM__HEADER_FILE__

#include "../namespace.h"

#include "binstream.h"
#include "../txtconv.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Binary streaming class working over a memory buffer
class hexstream : public binstream
{
public:

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        uint f = fATTR_OUTPUT_FORMATTING;
        if( _in )
            f |= _in->binstream_attributes(in0out1) & fATTR_READ_UNTIL;
        return f;
    }

/*		format  0x00,0x11,0x22,0x33,
    virtual binstream& write_raw( const void* p, uint& len )
    {
        RASSERTX( _out!=0, "output stream not bound" );
		char tmp[8] = "0x??,";
		uint tmp_len = 5;
		for( uint i=0; i<len; i++, _counter++ ) {
			tmp[2] = (((uchar *) p)[i] & 0xF0) >> 4;
			tmp[2] += (tmp[2] < 10 ? '0' : 'A'-10 );
			tmp[3] = ((uchar *) p)[i] & 0x0F;
			tmp[3] += (tmp[3] < 10 ? '0' : 'A'-10 );

			_out->write_raw( tmp, tmp_len );
			if( _counter % 24 == 23 ) {
				char tmp2[] = "\r\n";
				_out->write_raw( tmp2, 2 );
			}
		}
        return *this;
    }

    virtual binstream& read_raw( void* p, uint& len )
    {
        RASSERTX( _in!=0, "input stream not bound" );
		char tmp[8];
		for( uint i=0; i<len; i++ ) {
			_in->read_raw( tmp, 5 );
			if( tmp[0] == '\r' || tmp[0] == '\n' ) {
				tmp[2] = tmp[4];
				_in->read_raw( tmp + 3, 2 );
			}
			if( tmp[2] < '0' || tmp[2] > '9' && tmp[2] < 'A' || tmp[2] > 'Z') {len = i; throw ersNO_MORE;}
			if( tmp[3] < '0' || tmp[3] > '9' && tmp[3] < 'A' || tmp[3] > 'Z') {len = i; throw ersNO_MORE;}

			((uchar *) p)[i] = (tmp[2] >= 'A' ? tmp[2] - 'A' + 10 : tmp[2] - '0') << 4;
			((uchar *) p)[i] |= tmp[3] >= 'A' ? tmp[3] - 'A' + 10 : tmp[3] - '0';
		}
        return *this;
    }
*/
/*
//		format  "00112233"
	/// before writing this stream somewhere be sure to call flush() (writes the last quotes ("))
    virtual opcd write_raw( const void* p, uint& len )
    {
        DASSERTX( _out!=0, "output stream not bound" );
		char tmp[4] = "\"";
		if( !_counter ) {
			uint tmp_len = 1;
			_out->write_raw( tmp, tmp_len );
		}
        
		for( uint i=0; i<len; i++, _counter++ )
        {
			tmp[0] = (((uchar *) p)[i] & 0xF0) >> 4;
			tmp[0] += (tmp[0] < 10 ? '0' : 'A'-10 );
			tmp[1] = ((uchar *) p)[i] & 0x0F;
			tmp[1] += (tmp[1] < 10 ? '0' : 'A'-10 );

			uint tmp_len = 2;
			_out->write_raw( tmp, tmp_len );

			if( _counter % 48 == 47 )
            {
#ifdef SYSTYPE_WIN32
				char tmp2[] = "\"\r\n\"";
#else
				char tmp2[] = "\"\n\"";
#endif
                uint ts = sizeof(tmp2)-1;
				_out->write_raw( tmp2, ts );
			}
		}
        
        len = 0;
        return 0;
    }

    virtual opcd read_raw( void* p, uint& len )
    {
        DASSERTX( _in!=0, "input stream not bound" );
		char tmp2[8];
		char * tmp = tmp2 + 2;
		for( uint i=0; i<len; i++ )
        {
			_in->read_raw( tmp, 2 );
			if( tmp[0] == '\"' )
            {
				tmp[0] = tmp[1];
				_in->read_raw( tmp + 1, 1 );
				if( tmp[0] == '\r' ) _in->read_raw( tmp - 1, 3 );
				else if( tmp[0] == '\n' ) _in->read_raw( tmp, 2 );
			}
			if( tmp[0] < '0' || tmp[0] > '9' && tmp[0] < 'A' || tmp[0] > 'Z') {len -= i; return ersNO_MORE;}
			if( tmp[1] < '0' || tmp[1] > '9' && tmp[1] < 'A' || tmp[1] > 'Z') {len -= i; return ersNO_MORE;}

			((uchar *) p)[i] = (tmp[0] >= 'A' ? tmp[0] - 'A' + 10 : tmp[0] - '0') << 4;
			((uchar *) p)[i] |= tmp[1] >= 'A' ? tmp[1] - 'A' + 10 : tmp[1] - '0';
		}
        
        len = 0;
        return 0;
    }
*/


    ///	format  "00112233"
	/// before writing this stream somewhere be sure to call flush() (writes the trailing quotes ("))
    virtual opcd write_raw( const void* p, uints& len )
    {
        DASSERTX( _out!=0, "output stream not bound" );

        if( _offs == 0 )
            ++_offs;

        opcd e;
        uints lenb = len*2;
        uints n = _offs-1;
        if( n + lenb > _line )
        {
            uints f = (_line - n)/2;
            if(f) {
                char* dst = (char*)_buf.ptr()+_offs;
                charstrconv::bin2hex( p, dst, f, 1, 0 );
            }

            uints bn = _buf.len();
            e = _out->write_raw( _buf.ptr(), bn );
            if(e)  return e;

            _offs = 0;
            len -= f;
            return write_raw( (char*)p+f, len );
        }

        char* dst = (char*)_buf.ptr()+_offs;
        charstrconv::bin2hex( p, dst, len, 1, 0 );
        _offs = dst - _buf.ptr();

        len = 0;
        return 0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        DASSERTX( _in!=0, "input stream not bound" );

        opcd e;

        for( uints i=0; i<len; )
        {
            if( _ibuf.size() - _ioffs < 2 ) {
                e = fill_ibuf();
                if(e)  return e;
            }

            char c = _ibuf[_ioffs++];
            if( c == '\"' || c == '\r' || c == '\n' || c == '\t' || c == ' ' )
                continue;

            uchar v;
            if( c >= '0' && c <= '9' )      v = (c-'0')<<4;
            else if( c >= 'a' && c <= 'f' ) v = (c-'a'+10)<<4;
            else if( c >= 'A' && c <= 'F' ) v = (c-'A'+10)<<4;
            else return ersINVALID_PARAMS;

            c = _ibuf[_ioffs++];
            if( c >= '0' && c <= '9' )      v |= (c-'0');
            else if( c >= 'a' && c <= 'f' ) v |= (c-'a'+10);
            else if( c >= 'A' && c <= 'F' ) v |= (c-'A'+10);
            else return ersINVALID_PARAMS;

            ((uchar*)p)[i] = v;
            ++i;
        }

        len = 0;
        return 0;
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX )
    {
        return _in->read_until( ss, bout, max_size );
    }

    virtual opcd bind( binstream& bin, int io=0 )
    {
        if( io<0 )
            _in = &bin;
        else if( io>0 )
            _out = &bin;
        else
            _in = _out = &bin;
        return 0;
    }

    virtual bool is_open() const        { return _in->is_open(); }
    virtual void flush()
    {
        if(_offs)
        {
            _buf[_offs++] = '"';

            uints bn = _offs;
            opcd e = _out->write_raw( _buf.ptr(), bn );
            if(e)  throw e;

            _offs = 0;
        }
		_out->flush();
	}
    
    virtual void acknowledge( bool eat=false )
    {
        if(!eat)
        {
            for( ; _ioffs < _ibuf.size(); ++_ioffs )
            {
                char c = _ibuf[_ioffs];
                if( c == '\"' || c == '\r' || c == '\n' || c == '\t' || c == ' ' )
                    continue;

                throw ersIO_ERROR;
            }
        }

        _ioffs = 0;
        _in->acknowledge(eat);
    }

    virtual void reset()
    {
        _out->reset();
        _in->reset();

        setup(_line);
    }

    hexstream() : _in(0), _out(0)
    {
        setup(48);
    }

    hexstream( binstream* bin, binstream* bout )
    {
        _in = bin;
        _out = bout;
        setup(48);
    }


    void setup( uints line )
    {
        DASSERT( line>=4 );

        _line = line & ~1;
#ifdef SYSTYPE_WIN32
        token nl = "\r\n";
#else
        token nl = "\n";
#endif

        _buf.reset();
        _buf.get_append_buf(_line+2);
        _buf += nl;
        _buf[0] = '"';
        _buf[1+_line] = '"';
        _offs = 0;

        _ibuf.reserve(32,false);
        _ioffs = 0;
    }

protected:
    opcd fill_ibuf()
    {
        uints n = _ibuf.size() - _ioffs;
        if(n)
            xmemcpy( _ibuf.ptr(), _ibuf.ptre()-n, n );

        uints s = 32-n;
        opcd e = _in->read_raw_full( _ibuf.ptr()+n, s );
        if(e)  return e;

        s = 32-n - s;

        _ibuf.set_size(n+s);
        _ioffs = 0;
        return 0;
    }


    binstream* _in;
    binstream* _out;

    charstr _buf;
    uints _offs;
    uints _line;

    dynarray<char> _ibuf;
    uints _ioffs;
};

COID_NAMESPACE_END

#endif //__COMM_HEXSTREAM__HEADER_FILE__

