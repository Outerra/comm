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

#ifndef __COID_COMM_TXTCONV__HEADER_FILE__
#define __COID_COMM_TXTCONV__HEADER_FILE__

#include "namespace.h"
#include "token.h"
#include "commassert.h"

COID_NAMESPACE_BEGIN


///Number alignment
enum EAlignNum {
    ALIGN_NUM_LEFT_PAD_0            = -2,       ///< align left, pad with the '\0' character
    ALIGN_NUM_LEFT                  = -1,       ///< align left, pad with space
    ALIGN_NUM_CENTER                = 0,        ///< align center, pad with space
    ALIGN_NUM_RIGHT                 = 1,        ///< align right, fill with space
    ALIGN_NUM_RIGHT_FILL_ZEROS      = 2,        ///< align right, fill with '0' characters
};

///Helper formatter for integer numbers, use specialized from below
template<int WIDTH, int ALIGN, class NUM>
struct num_fmt {
    NUM value;
    num_fmt(NUM value) : value(value) {}
};

template<int WIDTH, class NUM> inline num_fmt<WIDTH,ALIGN_NUM_LEFT,NUM>
num_left(NUM n) {
    return num_fmt<WIDTH,ALIGN_NUM_LEFT,NUM>(n);
}

template<int WIDTH, class NUM> inline num_fmt<WIDTH,ALIGN_NUM_CENTER,NUM>
num_center(NUM n) {
    return num_fmt<WIDTH,ALIGN_NUM_CENTER,NUM>(n);
}

template<int WIDTH, class NUM> inline num_fmt<WIDTH,ALIGN_NUM_RIGHT,NUM>
num_right(NUM n) {
    return num_fmt<WIDTH,ALIGN_NUM_RIGHT,NUM>(n);
}

template<int WIDTH, class NUM> inline num_fmt<WIDTH,ALIGN_NUM_RIGHT_FILL_ZEROS,NUM>
num_right0(NUM n) {
    return num_fmt<WIDTH,ALIGN_NUM_RIGHT_FILL_ZEROS,NUM>(n);
}

///Helper formatter for floating point numbers
template<int WIDTH, int ALIGN>
struct float_fmt {
    double value;
    int nfrac;

    float_fmt(double value, int nfrac=-1)
        : value(value), nfrac(nfrac)
    {}
};

template<int WIDTH> inline float_fmt<WIDTH,ALIGN_NUM_LEFT>
float_left(double n, int nfrac=-1) {
    return float_fmt<WIDTH,ALIGN_NUM_LEFT>(n, nfrac);
}

template<int WIDTH> inline float_fmt<WIDTH,ALIGN_NUM_CENTER>
float_center(double n, int nfrac=-1) {
    return float_fmt<WIDTH,ALIGN_NUM_CENTER>(n, nfrac);
}

template<int WIDTH> inline float_fmt<WIDTH,ALIGN_NUM_RIGHT>
float_right(double n, int nfrac=-1) {
    return float_fmt<WIDTH,ALIGN_NUM_RIGHT>(n, nfrac);
}

template<int WIDTH> inline float_fmt<WIDTH,ALIGN_NUM_RIGHT_FILL_ZEROS>
float_right0(double n, int nfrac=-1) {
    return float_fmt<WIDTH,ALIGN_NUM_RIGHT_FILL_ZEROS>(n, nfrac);
}

////////////////////////////////////////////////////////////////////////////////
class charstrconv
{
public:
/*
    ///converts chunk of text to specified type
    static void strton (const char*& src, void* dst, binstream::type t, ulong num, char sep)
    {
        switch (t & binstream::type::xTYPE)
        {
        case binstream::type::tTYPE_BINARY:
            {
                //binary data are written as hexadecimal numbers tacked together by item size
                // is=1:  00 56 fa 4e 57 ...
                // is=2:  0045 faec c77b ...
                // is=4:  fc29a4b5 84ac4fa3 ...
                hex2bin (src, dst, t.get_size(num));
            }
        case binstream::type::tTYPE_INT:
            {
                //signed integers, can be in decimal form (123), hexadecimal (xfa34), octal (o712342)
                // binary (b01010001)
            }
        case binstream::type::tTYPE_UINT:
            {
                //unsigned integers, can be in decimal form (123), hexadecimal (xfa34), octal (o712342)
                // binary (b01010001)
            }
        case binstream::type::tTYPE_FLOAT:
            {
                //floating point numbers
            }
        case binstream::type::tTYPE_CHAR:
            {
                //string to string
            }
        }
    }

    ///converts specified type to chunk of text
    static void ntostr (const void* src, char*& dst, binstream::type t, ulong num, char sep)
    {
        switch (t & binstream::type::xTYPE)
        {
        case binstream::type::tTYPE_BINARY:
            {
                //binary data are written as hexadecimal numbers tacked together by item size
                // is=1:  00 56 fa 4e 57 ...
                // is=2:  0045 faec c77b ...
                // is=4:  fc29a4b5 84ac4fa3 ...
                hex2bin (src, dst, t.get_size(num));
            }
        case binstream::type::tTYPE_INT:
            {
                //signed integers, can be in decimal form (123), hexadecimal (xfa34), octal (o712342)
                // binary (b01010001)
            }
        case binstream::type::tTYPE_UINT:
            {
                //unsigned integers, can be in decimal form (123), hexadecimal (xfa34), octal (o712342)
                // binary (b01010001)
            }
        case binstream::type::tTYPE_FLOAT:
            {
                //floating point numbers
            }
        case binstream::type::tTYPE_CHAR:
            {
                //string to string
            }
        }
    }
*/

    ////////////////////////////////////////////////////////////////////////////////
    ///append number in baseN
    template< class INT >
    struct num_formatter
    {
        typedef typename SIGNEDNESS<INT>::SIGNED          SINT;
        typedef typename SIGNEDNESS<INT>::UNSIGNED        UINT;

        static token insert( char* dst, uints dstsize, INT n, int BaseN, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            if( SIGNEDNESS<INT>::isSigned )
                return s_insert(dst, dstsize, (SINT)n, BaseN, minsize, align);
            else
                return u_insert(dst, dstsize, (UINT)n, BaseN, 0, minsize, align);
        }

        static token insert_zt( char* dst, uints dstsize, INT n, int BaseN, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            if( SIGNEDNESS<INT>::isSigned )
                return s_insert_zt(dst, dstsize, (SINT)n, BaseN, minsize, align);
            else
                return u_insert_zt(dst, dstsize, (UINT)n, BaseN, 0, minsize, align);
        }

        ///Write signed number
        static token s_insert( char* dst, uints dstsize, SINT n, int BaseN, uints minsize, EAlignNum align )
        {
            if( n < 0 ) return u_insert( dst, dstsize, (UINT)int_abs(n), BaseN, -1, minsize, align );
            else        return u_insert( dst, dstsize, (UINT)n, BaseN, 0, minsize, align );
        }

        ///Write signed number, zero terminate the buffer
        static token s_insert_zt( char* dst, uints dstsize, SINT n, int BaseN, uints minsize, EAlignNum align )
        {
            if( n < 0 ) return u_insert_zt( dst, dstsize, (UINT)int_abs(n), BaseN, -1, minsize, align );
            else        return u_insert_zt( dst, dstsize, (UINT)n, BaseN, 0, minsize, align );
        }

        ///Write unsigned number, zero terminate the buffer
        static token u_insert_zt( char* dst, uints dstsize, UINT n, int BaseN, int sgn, uints minsize, EAlignNum align )
        {
            char bufx[32];
            uints i = precompute( buf, n, BaseN, sgn );
            const char* buf = fix_overflow(bufx, i, dstsize);

            uints fc=0;              //fill count
            if( i < minsize )
                fc = minsize-i;

            if( dstsize < i+fc+1 )
                return token::empty();

            char* zt = produce( dst, buf, i, fc, sgn, align );
            *zt = 0;
            return token(dst, i+fc);
        }

        ///Write unsigned number
        static token u_insert( char* dst, uints dstsize, UINT n, int BaseN, int sgn, uints minsize, EAlignNum align )
        {
            char bufx[32];
            uints i = precompute( bufx, n, BaseN, sgn );
            const char* buf = fix_overflow(bufx, i, dstsize);

            uints fc=0;              //fill count
            if( i < minsize )
                fc = minsize-i;

            if( dstsize < i+fc )
                return token::empty();

            produce( dst, buf, i, fc, sgn, align );
            return token(dst, i+fc);
        }

    protected:

        friend class charstr;

        static const char* fix_overflow( char* buf, uints size, uints sizemax )
        {
            if( size <= sizemax )  return buf;

            uints overflow = size - sizemax > 9  ?  3  :  2;

            if( sizemax <= overflow )  return "!!!";
            else if( overflow == 3 ) {
                buf[sizemax - 3] = 'e';
                buf[sizemax - 2] = '0' + (overflow / 10);
                buf[sizemax - 1] = '0' + (overflow % 10);
            }
            else {
                buf[sizemax - 2] = 'e';
                buf[sizemax - 1] = '0' + overflow;
            }

            return buf;
        }

        //@return number of characters taken
        static uints precompute( char* buf, UINT n, int BaseN, int sgn )
        {
            uints i=0;
            if(n) {
                for( ; n; ) {
                    UINT d = n / BaseN;
                    UINT m = n % BaseN;

                    if( m>9 )
                        buf[i++] = 'a' + (char)m - 10;
                    else
                        buf[i++] = '0'+(char)m;
                    n = d;
                }
            }
            else
                buf[i++] = '0';

            if(sgn) ++i;

            return i;
        }

        static char* produce( char* p, const char* buf, uints i, uints fillcnt, int sgn, EAlignNum align )
        {
            if( align == ALIGN_NUM_RIGHT )
            {
                for( ; fillcnt>0; --fillcnt )
                    *p++ = ' ';
            }
            else if( align == ALIGN_NUM_CENTER )
            {
                uints mc = fillcnt>>1;
                for( ; mc>0; --mc )
                    *p++ = ' ';
                fillcnt -= mc;
            }

            if( sgn < 0 )
                --i,*p++ = '-';
            else if( sgn > 0 )
                --i,*p++ = '+';

            if( fillcnt  &&  align == ALIGN_NUM_RIGHT_FILL_ZEROS )
            {
                for( ; fillcnt>0; --fillcnt )
                    *p++ = '0';
            }

            for( ; i>0; )
            {
                --i;
                *p++ = buf[i];
            }

            if(fillcnt)
            {
                char fc = (align==ALIGN_NUM_LEFT_PAD_0)  ?  0 : ' ';

                for( ; fillcnt>0; --fillcnt )
                    *p++ = fc;
            }
            return p;
        }
    };

    ///Append NUM type integer to buffer
    template<class NUM>
    static token append_num( char* dst, uint dstsize, int base, NUM n, uint minsize=0, EAlignNum align = ALIGN_NUM_RIGHT ) \
    {
        return num_formatter<NUM>::insert( dst, dstsize, n, base, minsize, align );
    }

    ///Append signed integer contained in memory pointed to by \a p
    static token append_num_int( char* dst, uint dstsize, int base, const void* p, uint bytes, uint minsize=0,
        EAlignNum align = ALIGN_NUM_RIGHT )
    {
        switch( bytes )
        {
        case 1: return append_num( dst, dstsize, base, *(int8*)p, minsize, align );
        case 2: return append_num( dst, dstsize, base, *(int16*)p, minsize, align );
        case 4: return append_num( dst, dstsize, base, *(int32*)p, minsize, align );
        case 8: return append_num( dst, dstsize, base, *(int64*)p, minsize, align );
        default:
            throw ersINVALID_TYPE "unsupported size";
        }
    }

    ///Append unsigned integer contained in memory pointed to by \a p
    static token append_num_uint( char* dst, uint dstsize, int base, const void* p, uint bytes, uint minsize=0,
        EAlignNum align = ALIGN_NUM_RIGHT )
    {
        switch( bytes )
        {
        case 1: return append_num( dst, dstsize, base, *(uint8*)p, minsize, align );
        case 2: return append_num( dst, dstsize, base, *(uint16*)p, minsize, align );
        case 4: return append_num( dst, dstsize, base, *(uint32*)p, minsize, align );
        case 8: return append_num( dst, dstsize, base, *(uint64*)p, minsize, align );
        default:
            throw ersINVALID_TYPE "unsupported size";
        }
    }

    ///Append floating point number
    ///@param nfrac number of decimal places: >0 maximum, <0 precisely -nfrac places
    ///@param minsize minimum size of the integer part
    static token append( char* dst, uint dstsize, double d, int nfrac, uint minsize=0, EAlignNum align = ALIGN_NUM_RIGHT )
    {
        uint anfrac = nfrac < 0  ?  -nfrac : nfrac;
        if( minsize>0 && anfrac>0 )
            minsize = minsize > anfrac+1  ?  minsize-anfrac-1  :  0;

        double w = d>0 ? floor(d) : ceil(d);
        token tokw = append_num( dst, dstsize, 10, (int64)w, minsize, align );

        if(nfrac == 0)
            return tokw;

        if( tokw.len() - dstsize > 0 ) {
            dst[tokw.len()] = '.';
            tokw++;
        }

        int tfrac = int_min( int(int_abs(nfrac)), int(dstsize-tokw.len()) );

        token tokf = append_fraction( dst+tokw.len(), fabs(d-w), nfrac>0 ? tfrac : -tfrac );

        tokw.shift_end( tokf.len() );
        return tokw;
    }

    ///@param ndig number of decimal places: >0 maximum, <0 precisely -ndig places
    static token append_fraction( char* dst, double n, int ndig )
    {
        uint ndiga = (uint)int_abs(ndig);
        char* p = dst;

        int lastnzero=1;
        for( uint i=0; i<ndiga; ++i )
        {
            n *= 10;
            double f = floor(n);
            n -= f;
            uint8 v = (uint8)f;
            *p++ = '0' + v;

            if( ndig >= 0  &&  v != 0 )
                lastnzero = i+1;
        }

        return token(dst, ndig > 0 ? lastnzero : ndiga);
    }

    ///Convert hexadecimal string content to binary data. Expects little-endian ordering.
    //@param src input: source string, output: remainder of the input
    //@param dst destination buffer of size at least nbytes
    //@param nbytes maximum number of bytes to convert
    //@param sep separator character to ignore, all other characters will cause the function to stop
    //@return number of bytes remaining to convert
    //@note function returns either after it converted required number of bytes, or it has read
    /// the whole string, or it encountered an invalid character.
    static uints hex2bin( token& src, void* dst, uints nbytes, char sep )
    {
        for( ; !src.is_empty(); )
        {
            if( src.first_char() == sep )  { ++src; continue; }

            char base;
            char c = src.first_char();
            if( c >= '0'  &&  c <= '9' )        base = '0';
            else if( c >= 'a'  &&  c <= 'f' )   base = 'a'-10;
            else if( c >= 'A'  &&  c <= 'F' )   base = 'A'-10;
            else 
                break;

            *(uchar*)dst = (c - base) << 4;
            ++src;

            c = src.first_char();
            if( c >= '0'  &&  c <= '9' )        base = '0';
            else if( c >= 'a'  &&  c <= 'f' )   base = 'a'-10;
            else if( c >= 'A'  &&  c <= 'F' )   base = 'A'-10;
            else
            {
                --src;
                break;
            }

            *(uchar*)dst += (c - base);
            dst = (uchar*)dst + 1;
            ++src;

            --nbytes;
            if(!nbytes)  break;
        }

        return nbytes;
    }

    ///Convert binary data to hexadecimal string
    //@param src source memory buffer
    //@param dst destination character buffer capable to hold at least (((itemsize*2) + sep?1:0) * nitems) bytes
    //@param nitems number of itemsize sized words to convert
    //@param itemsize number of bytes to write clumped together before separator
    //@param sep separator character, 0 if none
    static void bin2hex( const void* src, char*& dst, uints nitems, uint itemsize, char sep=' ' )
    {
        if( nitems == 0 )  return;

        static char tbl[] = "0123456789ABCDEF";
        for( uints i=0;; )
        {
            if(sysIsLittleEndian)
            {
                for( uint j=itemsize; j>0; )
                {
                    --j;
                    dst[0] = tbl[((uchar*)src)[j] >> 4];
                    dst[1] = tbl[((uchar*)src)[j] & 0x0f];
                    dst += 2;
                }
            }
            else
            {
                for( uint j=0; j<itemsize; ++j )
                {
                    dst[0] = tbl[((uchar*)src)[j] >> 4];
                    dst[1] = tbl[((uchar*)src)[j] & 0x0f];
                    dst += 2;
                }
            }

            src = (uchar*)src + itemsize;

            ++i;
            if( i>=nitems )  break;

            if(sep)
            {
                dst[0] = sep;
                ++dst;
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Convert bytes to intelhex format output
    static uints write_intelhex_line( char* dst, ushort addr, uchar n, const void* data )
    {
        *dst++ = ':';

        const char* dstb = dst;

        charstrconv::bin2hex( &n, dst, 1, 1 );
        charstrconv::bin2hex( &addr, dst, 1, 2 );

        uchar t=0;
        charstrconv::bin2hex( &t, dst, 1, 1 );

        charstrconv::bin2hex( data, dst, n, 1, 0 );

        uchar sum=0;
        for( const char* dst1=dstb ; dst1<dst; )
        {
            char base;
            if( *dst1 >= '0'  &&  *dst1 <= '9' )        base = '0';
            else if( *dst1 >= 'a'  &&  *dst1 <= 'f' )   base = 'a'-10;
            else if( *dst1 >= 'A'  &&  *dst1 <= 'F')    base = 'A'-10;

            sum += (*dst1++ - base) << 4;        

            if( *dst1 >= '0'  &&  *dst1 <= '9' )        base = '0';
            else if( *dst1 >= 'a'  &&  *dst1 <= 'f' )   base = 'a'-10;
            else if( *dst1 >= 'A'  &&  *dst1 <= 'F')    base = 'A'-10;

            sum += *dst1++ - base;        
        }

        sum = -(schar)sum;
        charstrconv::bin2hex( &sum, dst, 1, 1 );

        *dst++ = '\r';
        *dst++ = '\n';
        *dst++ = 0;

        return dst - dstb;
    }

    static token write_intelhex_terminator()
    {
        static token t = ":00000001FF\r\n";
        return t;
    }

};

COID_NAMESPACE_END

#endif //__COID_COMM_TXTCONV__HEADER_FILE__
