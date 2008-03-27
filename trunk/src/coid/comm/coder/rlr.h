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

    static const int8 minplane = 3;


    rlr_coder() {
        planes.need(8*sizeof(INT));
        //reset();
    }

    ///
    void encode( const INT* data, uints len, binstream& bin )
    {
        reset(true);

        int8 plane = minplane;
        int8 maxplane = plane;

        for( uints i=0; i<len; ++i )
        {
            INT vs = data[i];
            UINT v = (vs<<1) ^ (vs>>(8*sizeof(INT)-1));//vs<0 ? (((~vs)<<1)|1) : (vs<<1);

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
                if(plane>minplane)
                    planes[plane-1].one(v);
                else
                    planes[plane-1].zero(v);
            }
            else if(plane>minplane)
            {
                //below the current plane
                do
                {
                    --plane;
                    planes[plane].down(i);
                }
                while( !(v>>(plane-1)) && plane>minplane );

                if(plane>minplane)
                    planes[plane-1].one(v);
                else
                    planes[plane-1].zero(v);
            }
            else
                planes[plane-1].zero(v);
        }

        bin << maxplane;

        for( int8 i=maxplane; i>minplane; --i )
            planes[i-1].write_to(bin);

        if(minplane>0)
            planes[minplane-1].write_to(bin);
    }

    void decode( INT* data, uints len, binstream& bin )
    {
        reset(false);

        int8 maxplane;
        bin >> maxplane;

        for( int8 i=maxplane; i>minplane; --i )
            planes[i-1].read_from(bin);

        if(minplane>0)
            planes[minplane-1].read_from(bin);

        decode( maxplane, data, len );
    }

private:

    void reset( bool enc ) {
        for( uint i=0; i<8*sizeof(INT); ++i )
            planes[i].reset(i, enc);
    }

    void decode( int8 plane, INT* data, uints len )
    {
        rlr_bitplane& rp = planes[plane-1];

        if(plane<=minplane) {
            rp.readn(data, len);
            return;
        }

        ints& n = rp.zeros(len);

        while(len>0)
        {
            if(n == 0) {
                *data++ = rp.read();
                --len;
                rp.zeros(len);
            }

            //n elements encoded in lower planes
            uints k = int_min(len, uints(n));

            if(k) {
                decode( plane-1, data, k );

                data += k;
                len -= k;
                n -= k;
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
            write_rem(v, plane);
        }

        void zero( UINT v ) {
            write_rem(v, plane+1);
        }

        ///Leaving to lower plane
        void down( uints pos ) {
            lastpos = pos;
        }

        void reset( uint plane, bool enc ) {
            nzero = enc ? 0 : -1;
            lastpos = 0;
            runbits = 3;
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
            //reset(UMAX);
        }

        INT read()
        {
            static const int8 bits = 8*sizeof(INT)-1;

            --nzero;

            UINT v = (1<<plane) | read_rem(plane);
            return (INT(v<<bits)>>bits) ^ (v>>1);
        }

        void readn( INT* val, uint n )
        {
            static const int8 bits = 8*sizeof(INT)-1;

            for( uint i=0; i<n; ++i ) {
                UINT v = read_rem(plane+1);
                val[i] = (INT(v<<bits)>>bits) ^ (v>>1);
            }
        }

        ///Decode count of zeroes followed by 1
        bool decode0()
        {
            int run = 1<<runbits;
            nzero = 0;

            while(!readb()) {
                nzero += run;
                ++runbits;
                run <<= 1;
            }

            if(bitpos >= lastpos)
                return false;    //end

            nzero += read_count();
            
            if(runbits>0) {
                --runbits;
                run >>= 1;
            }

            return true;
        }

        ints& zeros( ints rem ) {
            if(bitpos<lastpos  &&  nzero < 0)
                decode0();

            if(bitpos>=lastpos)
                nzero = rem;
            
            return nzero;
        }

    private:
        ///Encode count of zeroes followed by 1
        void encode0()
        {
            ints run = 1<<runbits;

            while(nzero >= run)
            {
                //encode run of run zeros by a single 0
                write0();
                nzero -= run;
                
                ++runbits;
                run <<= 1;
            }

            //encode run of count 0's followed by a 1
            write1();
            write_count(nzero);
            
            if(runbits>0) {
                --runbits;
                run >>= 1;
            }

            nzero = 0;
        }


        void write_count( uints count )
        {
            if(bitpos+runbits > 8*buf.byte_size())
                buf.need(2*buf.size());

            int8 k = runbits;
            while(k>0)
            {
                if(count&1)
                    buf[bitpos>>5] |= 1<<(bitpos&31);
                else
                    buf[bitpos>>5] &= ~(1<<(bitpos&31));
                ++bitpos;
                count >>= 1;
                --k;
            }
        }

        void write_rem( UINT v, uint8 nbits )
        {
            if(bitpos+nbits > 8*buf.byte_size())
                buf.need(2*buf.size());

            for( uint8 k=nbits; k>0; --k ) {
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
            uchar i = 0;

            int8 k = runbits;
            while(k>0) {
                count |= ((buf[bitpos>>5] >> (bitpos&31))&1) << i++;
                ++bitpos;
                --k;
            }

            return count;
        }

        UINT read_rem( uint8 nbits ) {
            UINT v = 0;

            for( uint8 i=0; i<nbits; ++i ) {
                v |= ((buf[bitpos>>5] >> (bitpos&31))&1) << i;
                ++bitpos;
            }

            return v;
        }

    private:
        ints nzero;
        uints lastpos;

        dynarray<uint> buf;
        uint bitpos;

        int8 runbits;
        uint8 plane;
    };


private:

    dynarray<rlr_bitplane> planes;

    coid::binstream* bin;
};

COID_NAMESPACE_END

#endif //__COID_COMM_PACKSTREAMRLR__HEADER_FILE__
