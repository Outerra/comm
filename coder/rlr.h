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
 * Brano Kemen
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __COID_COMM_PACKSTREAMRLR__HEADER_FILE__
#define __COID_COMM_PACKSTREAMRLR__HEADER_FILE__

#include "comm/binstream/binstream.h"
#include "comm/mathi.h"

COID_NAMESPACE_BEGIN


///Run-length Rice encoder/decoder
template<class INT>
struct rlr_coder
{
    typedef typename SIGNEDNESS<INT>::UNSIGNED      UINT;

    void reset() {
        for( uint i=0; i<8*sizeof(INT); ++i )
            planes[i].reset(i);
    }

    rlr_coder() {
        planes.need(8*sizeof(INT));
        reset();
    }

    ///
    void encode( const INT* data, uints len, binstream& bin )
    {
        reset();

        uchar plane = 1;
        uchar maxplane = plane;

        for( uints i=0; i<len; ++i )
        {
            INT vs = data[i];
            UINT v = vs<0 ? (((~vs)<<1)|1) : (vs<<1);

            if(v>>plane)
            {
                //above the current plane
                do {
                    planes[plane].up(i);
                    ++plane;
                }
                while( (v>>plane) && plane<8*sizeof(INT) );

                maxplane = int_max(plane, maxplane);

                planes[plane-1].one(v);
            }
            else if(v>>(plane-1))
            {
                //in the plane
                planes[plane-1].one(v);
            }
            else
            {
                //below the current plane
                if(plane>1) do
                {
                    --plane;
                    planes[plane].down(i);
                }
                while( !(v>>(plane-1)) && plane>1 );

                if(v)  planes[plane-1].one(v);
            }
        }

        bin << maxplane;

        for( uchar i=maxplane; i>1; --i )
            planes[i-1].write_to(bin);

        planes[0].write_to(bin);
    }

    void decode( INT* data, uints len, binstream& bin )
    {
        reset();

        uchar maxplane;
        bin >> maxplane;

        for( uchar i=maxplane; i>1; --i )
            planes[i-1].read_from(bin);

        planes[0].read_from(bin);

        decode( maxplane, data, len );
    }

private:

    void decode( uchar plane, INT* data, uints len )
    {
        rlr_bitplane& rp = planes[plane-1];
        uints n = rp.zeros();

        while( len>0 )
        {
            if(n == 0) {
                *data++ = rp.read();
                --len;
                rp.decode0();
                n = rp.zeros();
            }

            //n elements encoded in lower planes
            uints k = int_min(len, n);

            if(k) {
                decode( plane-1, data, k );
                data += k;
                len -= k;
            }
        }
    }

    struct rlr_bitplane
    {
        ///Coming from lower plane
        void up( uints pos ) {
            nzero += pos - lastpos;
        }

        void one( UINT v ) {
            encode0();
            write_rem(v);
        }

        ///Leaving to lower plane
        void down( uints pos ) {
            lastpos = pos;
        }

        void reset( uint plane ) {
            nzero = 0;
            lastpos = 0;
            k = 3;
            bitpos = 0;
            buf.need(32);
            this->plane = plane;
        }

        void write_to( binstream& bin )
        {
            uints total = align_to_chunks(bitpos, 8);
            lastpos = bitpos;

            while( bitpos < total*8 ) {
                buf[bitpos>>5] |= 1<<(bitpos&31);
                ++bitpos;
            }

            bin << (uint)lastpos;
            bin.xwrite_raw( buf.ptr(), total );
        }

        void read_from( binstream& bin )
        {
            uint pos;
            bin >> pos;

            lastpos = pos;
            bitpos = 0;

            uints total = align_to_chunks(lastpos, 8);
            bin.xread_raw(
                buf.need(align_to_chunks(total, sizeof(uint))),
                total );
        }

        rlr_bitplane() {
            reset(UMAX);
        }

        UINT read()
        {
            return (1<<plane) | read_rem();
        }

        ///Decode count of zeroes followed by 1
        bool decode0()
        {
            uints count = 0;

            while(!readb()) {
                nzero += k;
                ++k;
            }

            if(bitpos >= lastpos)
                return false;    //end

            nzero += read_count();
            --k;
            return true;
        }

        uints zeros() {
            if(bitpos == 0)
                decode0();
            return nzero;
        }

    private:
        ///Encode count of zeroes followed by 1
        void encode0()
        {
            while(nzero >= k)
            {
                //encode run of k zeros by a single 0
                write0();
                nzero -= k;
                ++k;
            }

            //encode run of count 0's followed by a 1
            write1();
            write_count(nzero);
            --k;
            nzero = 0;
        }


        void write_count( uints count )
        {
            if(bitpos+32 > 8*buf.byte_size())
                buf.need(2*buf.size());

            uint kk = k;
            while(kk) {
                if(count&1)
                    buf[bitpos>>5] |= 1<<(bitpos&31);
                else
                    buf[bitpos>>5] &= ~(1<<(bitpos&31));
                ++bitpos;
                count >>= 1;
                kk >>= 1;
            }
        }

        void write_rem( UINT v )
        {
            if(bitpos+plane > 8*buf.byte_size())
                buf.need(2*buf.size());

            for( uint kk=plane; kk>0; --kk ) {
                if(v&1)
                    buf[bitpos>>5] |= 1<<(bitpos&31);
                else
                    buf[bitpos>>5] &= ~(1<<(bitpos&31));
                ++bitpos;
                v >>= 1;
            }
        }

        void write1() {
            if(bitpos >= 8*buf.byte_size())
                buf.need(2*buf.size());

            buf[bitpos>>5] |= 1<<(bitpos&31);
            ++bitpos;
        }

        void write0() {
            if(bitpos >= 8*buf.byte_size())
                buf.need(2*buf.size());

            buf[bitpos>>5] &= ~(1<<(bitpos&31));
            ++bitpos;
        }

        uint readb() {
            if(bitpos >= lastpos)
                return 1;

            uint v = buf[bitpos>>5] & (1<<(bitpos&31));
            ++bitpos;
            return v;
        }

        uints read_count() {
            uints count = 0;
            uint kk = k;
            uchar i = 0;

            while(kk) {
                count |= ((buf[bitpos>>5] >> (bitpos&31))&1) << i++;
                ++bitpos;
                kk >>= 1;
            }

            return count;
        }

        UINT read_rem() {
            UINT v = 0;

            for( uint i=0; i<plane; ++i ) {
                v |= ((buf[bitpos>>5] >> (bitpos&31))&1) << i;
                ++bitpos;
            }

            return v;
        }

    private:
        uint nzero;
        uint k;
        uint plane;
        uints lastpos;

        dynarray<uint> buf;
        uint bitpos;
    };


private:

    dynarray<rlr_bitplane> planes;

    coid::binstream* bin;
};

COID_NAMESPACE_END

#endif //__COID_COMM_PACKSTREAMRLR__HEADER_FILE__
