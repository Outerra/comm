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

#ifndef __COID_COMM_NETSTREAMUDP__HEADER_FILE__
#define __COID_COMM_NETSTREAMUDP__HEADER_FILE__

#include "../namespace.h"

#include "../comm.h"
#include "../str.h"
#include "../retcodes.h"
#include "binstream.h"
//#include "codes.h"
#include "../net.h"
#include "../pthreadx.h"

COID_NAMESPACE_BEGIN

/**
    Divided into packets of max 8192 bytes, with 4B header.
    Header contains two bytes "Bs" followed by 2B actual size of packet.
    When the size is 0xffff, this means a full packet (8192-4 bytes) followed by more packets.
*/

class netstreamudp : public binstream
{
public:
    virtual ~netstreamudp() {
        close();
    }

    virtual uint binstream_attributes( bool in0out1 ) const
    {
        return fATTR_HANDSHAKING;
    }

    virtual opcd write_raw( const void* p, uints& len )
    {
        if(len)
            add_to_packet ((const uchar*)p, len);
        return 0;
    }

    virtual opcd read_raw( void* p, uints& len )
    {
        if(len)
            return get_from_packet( (uchar*)p, len );

        return 0;
    }

    virtual opcd read_until( const substring& ss, binstream* bout, uints max_size=UMAX ) { return ersUNAVAILABLE; }

    virtual bool is_open() const                    { return true; }
    virtual void flush()
    {
        if (_tpckid != 0xff)
        {
            send();
        }
    }

    virtual void reset ()
    {
        //implement this
        throw ersNOT_IMPLEMENTED;
    }

    virtual void acknowledge( bool eat = false )
    {
        if (_rpckid+1 < _rnseg  ||  _roffs < _rsize)
        {
            if (eat)  eat_input ();
            else  throw ersIO_ERROR "data left in received packet";
        }
        _rpckid = 0;
        _recvbuf.reset();
        _flg = 0;
    }

    bool data_available( uint timeout )
    {
        if (_rpckid == 0xff)
            return true;

        return recvpack(timeout);
    }

    netAddress* get_local_address (netAddress* addr) const
    {
        return netAddress::getLocalHost( addr );
    }

protected:

    struct udp_hdr
    {
        ushort  _pckid;     ///< packet id - size
        ushort  _trsmid;    ///< transmission id

        enum {
            PASSING_PACKET          = 0xa2c0,
            TRAILING_PACKET         = 0x8640,
            xPACKET_TYPE            = 0xffe0,

            xVALUE                  = 0x001f,
        };

        void set_trailing_packet (uchar i)  { _pckid = TRAILING_PACKET | i; }
        void set_passing_packet (uchar i)   { _pckid = PASSING_PACKET | i; }

        bool is_valid_packet () const       { return (_pckid & xPACKET_TYPE) == TRAILING_PACKET  ||  (_pckid & xPACKET_TYPE) == PASSING_PACKET; }
        bool is_trailing_packet () const    { return (_pckid & xPACKET_TYPE) == TRAILING_PACKET; }

        uchar get_packet_id () const        { return (uchar)_pckid & xVALUE; }

        uchar get_next_seg () const         { return (uchar)_trsmid; }
        void set_next_seg (uchar i)         { _trsmid = i; }

        void set_transmission_id (ushort t) { _trsmid = t; }
    };

    struct udp_seg : udp_hdr
    {
        enum {
            PACKET_LENGTH           = 8192,
            DATA_SIZE               = PACKET_LENGTH - sizeof(udp_hdr),
        };

        uchar   _data[DATA_SIZE];
    };

    void eat_input ()
    {
    }

    void add_to_packet (const uchar* p, uints& size)
    {
        if(!size)  return;

        if(_tpckid == 0xff)
        {
            _tpckid = 0;
            _toffs = 0;
        }

        uints s = _toffs;
        if (s + size > udp_seg::DATA_SIZE)
        {
            xmemcpy (_sendbuf._data+s, p, udp_seg::DATA_SIZE - s);
            p += udp_seg::DATA_SIZE - s;
            size -= udp_seg::DATA_SIZE - s;
            _toffs = udp_seg::DATA_SIZE;

            send();
            add_to_packet (p, size);
        }
        else
        {
            xmemcpy (_sendbuf._data+s, p, size);
            _toffs += (ushort)size;
            size = 0;
        }
    }

    opcd get_from_packet( uchar* p, uints& size )
    {
        if (_rpckid != 0xff)
            throw ersUNAVAILABLE "data not received";

        if (_roffs+size > _rsize)
        {
            uints pa = udp_seg::DATA_SIZE - _roffs;
            xmemcpy (p, _recvbuf[_rfseg]._data+_roffs, pa);
            size -= pa;
            p += pa;

            _rfseg = _recvbuf[_rfseg].get_next_seg();
            if (_rfseg == 0xff)
                return ersNO_MORE "required more than sent";

            _roffs = 0;
            _rsize = _recvbuf[_rfseg].get_next_seg() != 0xff ? (ushort)udp_seg::DATA_SIZE : (ushort)_flg;

            get_from_packet( p, size );
        }
        else
        {
            xmemcpy (p, _recvbuf[_rfseg]._data+_roffs, size);
            _roffs += (ushort)size;
            size = 0;
        }

        return 0;
    }

    void send()
    {
        if (_toffs < udp_seg::DATA_SIZE)
            _sendbuf.set_trailing_packet(_tpckid);
        else
            _sendbuf.set_passing_packet(_tpckid);

        _sendbuf._trsmid = _trsmid;

        long n = _socket.sendto( &_sendbuf, sizeof(udp_hdr) + _toffs, 0, &_address );
        if (n == -1)
            throw ersFAILED "while sending data";

        if (_toffs < udp_seg::DATA_SIZE)
        {
            _tpckid = 0xff;
            ++_trsmid;
        }
        else
            ++_tpckid;
        _toffs = 0;
    }

    ///Receive an udp packet
    ///@return true if message received
    bool recvpack (uint timeout)
    {
        if (timeout != UMAX)
        {
            int ns = _socket.wait_read(timeout);
            if( ns <= 0 )
                return false;
        }

        if (_recvbuf.size() <= _rpckid)
            _recvbuf.need (_rpckid+1);

        long n = _socket.recvfrom( _recvbuf.ptr()+_rpckid, sizeof(udp_seg), 0, &_address );
        if (n == -1)
            return false;

        udp_hdr* hdr = _recvbuf.ptr() + _rpckid;

        if (!hdr->is_valid_packet())
            return recvpack(timeout);          //invalid packet

        if (_flg == 0)                  //first packet of a chain
            _trsmid = hdr->_trsmid;
        else if (_trsmid != hdr->_trsmid)   //not a packet of current transmission
        {
            if ((short)(hdr->_trsmid - _trsmid) < 0)
                return recvpack(timeout);   //discard an old packet

            //this is newer packet than packets from the current transmission
            // what means that a newer transmission has begun
            //discard what we've already received
            _flg = 0;   //no packet recvd
            _rpckid = 0;  //expect first packet
        }

        uchar pck = hdr->get_packet_id();
        if (_flg & (1<<pck))
            return recvpack(timeout);     //the same packet hb already received
        _flg |= 1<<pck;

        //adjust _rpckid to point to the first empty packet
        uint flg = _flg >> (++_rpckid);
        for (; flg&1; ++_rpckid)  flg>>=1;

        if (pck == 0)
            _rfseg = pck;

        //set status
        if (hdr->is_trailing_packet())              //this was the last packet of the transmission
        {
            if (!pck || (_flg & ((1UL<<pck)-1)) == _flg)    //all packets received
                _rpckid = 0xff;                     //msg complete
            else
                _flg |= ~((1UL<<pck)-1);            //prepare for later detection of completenes in 'U'
            _rnseg = pck+1;
            _roffs = (ushort) (n - sizeof(udp_hdr));
        }
        else if (_flg == UMAX)
            _rpckid = 0xff;                         //all packets received

        if (_rpckid != 0xff)
            return recvpack(timeout);

        if (_rnseg > 1)
        {
            //setup segment sequence
            uint flg = _flg & ~(1UL << _rfseg);
            uchar cur = _rfseg;
            uchar min = _rfseg ? 0 : 1;

            for (uchar j=min; j<_rnseg; ++j)
            {
                uint msk = 1 << j;
                if (!(flg & msk))  continue;

                if (_recvbuf[j].get_packet_id() == cur)
                {
                    _recvbuf[cur].set_next_seg(j);
                    flg &= ~msk;
                    cur = j;

                    //check if there did remain some segments before this one
                    --msk;
                    if (flg & msk)
                        j = min - 1;
                }
            }
            _recvbuf[cur].set_next_seg (0xff);
        }
        else
            _recvbuf[0].set_next_seg(0xff);

        //setup for reading
        _flg = _roffs;  //mark size of last segment here
        _roffs = 0;
        _rsize = _rnseg>1 ? (ushort)udp_seg::DATA_SIZE : (ushort)_flg;

        return true;
    }

    void setup_socket ()
    {
        _rpckid = 0;
        _tpckid = 0;
        _trsmid = 0;
        _flg = 0;
        _toffs = 0;
        _roffs = 0;
        _rfseg = 0xff;
        _rnseg = 0;
        _socket.setBlocking( true );
        _socket.setNoDelay( true );
        _socket.setReuseAddr( true );
    }

public:
    netstreamudp(ushort port)
    {
        _socket.open( false );
        setup_socket();
        _socket.bind( "", port );

        //netAddress addr;
        //_socket.getAddress(&addr);
    }

    netstreamudp()
    {
        _socket.open( false );
        setup_socket();
        _socket.bind( "", 0 );

        //netAddress addr;
        //_socket.getAddress(&addr);
    }

    void set_socket (netSocket& s)
    {
        _socket = s;
        s.setHandleInvalid();
        setup_socket();
    }

    void set_broadcast_addr (ushort port)
    {
        _socket.setBroadcast( true );
        _address.setBroadcast();
        _address.setPort( port );
    }

    void set_addr( const netAddress& addr )
    {
        _socket.setBroadcast( false );
        _address = addr;
    }

    virtual opcd close( bool linger=false )
    {
        _socket.close();
        return 0;
    }

    const netAddress* get_address () const     { return &_address; }

private:
    netSocket           _socket;
    netAddress          _address;
    uchar               _rpckid;    ///< expected packet id, def 0, 0xff when no more packets are expected
    uchar               _tpckid;    ///< packet id of currently open packet for writting
    ushort              _trsmid;    ///< transmission id
    uint                _flg;       ///< every bit represents one packet, one's for received ones
    uchar               _rfseg;     ///< id of the first or read segment (position in recvbuf)
    uchar               _rnseg;     ///< count of recvd segments
    ushort              _rsize;     ///< size of current segment data
    dynarray<udp_seg>   _recvbuf;
    ushort              _toffs;     ///< offset in the transmission packet buffer
    ushort              _roffs;     ///< offset in the _recvbuf
    udp_seg             _sendbuf;
};

COID_NAMESPACE_END

#endif //__COID_COMM_NETSTREAMUDP__HEADER_FILE__

