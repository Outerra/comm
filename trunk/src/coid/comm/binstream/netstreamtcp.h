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
 * Robo Strycek
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


#ifndef __COID_COMM_NETSTREAMTCP__HEADER_FILE__
#define __COID_COMM_NETSTREAMTCP__HEADER_FILE__

#include "../namespace.h"

#include "netstream.h"
#include "../str.h"
#include "../net.h"


COID_NAMESPACE_BEGIN



////////////////////////////////////////////////////////////////////////////////
/// class similar to netstream, but does not use any header info
class netstreamtcp : public netstream
{
	uint			_timeout;
    netSocket       _socket;

public:
    virtual ~netstreamtcp()     { close(); }

	netstreamtcp() {
		_timeout = 0;
		_socket.setHandleInvalid();
	}
	netstreamtcp( netSocket& s ) {_timeout = 0; assign_socket( s );}
	netstreamtcp( int socket ) {
		_timeout = 0;
		_socket.setHandle( socket );
		_socket.setBlocking( true );
        _socket.setNoDelay( true );
        _socket.setReuseAddr( true );
	}

    opcd connect( const netAddress& addr )
    {
        close();
        _socket.open(true);
		_socket.setBlocking( true );
        _socket.setNoDelay( true );
        _socket.setReuseAddr( true );
        if( 0 == _socket.connect(addr) )  return 0;
        return ersFAILED;
    }

    virtual opcd set_timeout( uint ms )
    {
        _timeout = ms;
        return 0;
    }



    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return fATTR_READ_UNTIL;
    }

	void assign_socket( netSocket& s ) {
		_socket.setHandle( s.getHandle() );
		s.setHandleInvalid();
		_socket.setBlocking( true );
        _socket.setNoDelay( true );
        _socket.setReuseAddr( true );
	}


	virtual opcd close( bool linger=false )
    {
        if(linger)  return lingering_close(1000);

		_socket.close();
		_socket.setHandleInvalid();
        return 0;
	}


	opcd lingering_close( uint mstimeout=0 )
    {
		_socket.lingering_close();
		_socket.setHandleInvalid();
        return 0;
	}

	 /// continue reading until 'term' character is read or 'max_size' bytes received
	opcd read_until( charstr & buf, char term, uints max_size=UMAX )
    {
		uints buf_len = max_size < 512 ? max_size : 512;
		char * p = buf.get_append_buf( buf_len );
		p[buf_len] = 0;

		uints len = buf_len;
		uints total_size = 0;
		while(1)
        {
			opcd e = read_raw( p, len );
			if(e) return e;

            uints n = buf_len - len;

			p[n] = 0;
			if( strchr(p, term) ) break;
			if( total_size + n < buf_len )
            {
				total_size += n;
				p += n;
				len = buf_len - total_size;
				continue;
			}

			max_size -= buf_len;
			if( max_size == 0 ) break;
			if( max_size < buf_len ) buf_len = max_size;

			p = buf.get_append_buf( buf_len );
			p[buf_len] = 0;
			len = buf_len;
			total_size = 0;
		}

		buf.correct_size();

        return 0;
	}

	virtual opcd write_raw( const void* p, uints& len )
	{
        if( !_socket.isValid() )  return ersDISCONNECTED;

        int blk = 0;
        for (; len; )
        {
            int n = _socket.send( p, (int)len );
            if (n == -1)
            {
                if( errno == EAGAIN )
                    continue;
                close();
                return ersDISCONNECTED "while sending data";
            }

            if( n == 0 )
            {
                if (blk++) return ersUNAVAILABLE "connection closed";
                // give m$ $hit a chance
                _socket.setBlocking( true );
            }
            len -= n;
            p = (const char *) p + n;
        }

		return 0;
	}

	virtual opcd read_raw( void* p, uints& len )
	{
        if( !_socket.isValid() )  return ersDISCONNECTED;

        if(_timeout)
        {
            int ns = _socket.wait_read(_timeout);
            if( ns == 0 )
                return ersTIMEOUT;
            if( ns < 0 )
                return ersDISCONNECTED;
        }

        int n = _socket.recv( p, (int)len );
        if( n == -1 ) {
            if( errno == EAGAIN )
                return ersRETRY;

            close();
            return ersDISCONNECTED "socket error";
        }
        if( n == 0 ) {
            _socket.close();
			return ersUNAVAILABLE "connection closed";
		}
        
        len -= n;
        return len ? ersNO_MORE : opcd(0);
	}

	virtual bool is_open() const                { return _socket.getHandle() != -1; }

	virtual void flush()                        { }
	virtual void acknowledge( bool eat=false )  { }
    virtual void reset()		                { }

    virtual netAddress* get_remote_address( netAddress* addr ) const
    {
        return _socket.getRemoteAddress(addr);
    }

    virtual bool is_socket_connected()
    {
        return 0 < _socket.wait_write(0);
    }

};



COID_NAMESPACE_END

#endif //__COID_COMM_NETSTREAMTCP__HEADER_FILE__
