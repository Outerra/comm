
#include "txtconv.h"

using namespace coid;

namespace coid {
namespace charstrconv {

////////////////////////////////////////////////////////////////////////////////
void append_fixed( char* dst, char* dste, double v, int nfrac, EAlignNum align)
{
    char sbuf[32];
    bool doalign = align != ALIGN_NUM_LEFT  &&  align != ALIGN_NUM_LEFT_PAD_0;
    uint dsize = dste - dst;

    char* buf = doalign ? sbuf : dst;
    char* bufend = doalign ? sbuf+dsize : dste;

    char* end = charstrconv::append_float(buf, bufend, v, nfrac);

    if(doalign) {
        int d = (dste-dst) - (end-buf);
        DASSERT(d >= 0);

        switch(align) {
            case ALIGN_NUM_RIGHT: {
                ::memset(dst, ' ', d);
                ::memcpy(dst+d, buf, end-buf);
                break;
            }
            case ALIGN_NUM_RIGHT_FILL_ZEROS: {
                if(buf[0] == '-') {
                    *dst++ = '-';
                    ++buf;
                }
                ::memset(dst, '0', d);
                ::memcpy(dst+d, buf, end-buf);
                break;
            }
            case ALIGN_NUM_CENTER: {
                ::memset(dst, ' ', d/2);
                ::memcpy(dst+d/2, buf, end-buf);
                ::memset(dste-(d-d/2), ' ', d-d/2);
                break;
            }
        }
    }
    else
        ::memset(end, align==ALIGN_NUM_LEFT ? ' ' : '\0', bufend-end);
}

////////////////////////////////////////////////////////////////////////////////
char* append_float( char* dst, char* dste, double d, int nfrac, uint minsize )
{
    char* p = dst;

    if(d<0) {
        *p++ = '-';
        d = -d;
    }

    double l = log10(d);
    double w = floor(l);
    int e = int(w);

    //number of characters unnormalized
    int nuc;
    if(nfrac<0)
        nuc = 1-nfrac + (e>=0 ? e+2 : -e);
    else
        nuc = e>=0 ? e : -e+1;

    if(e<0 && nfrac<0)
    {
        //always as fraction
        if(p<dste)
            *p++ = '.';

        p = append_fraction(p, dste, d, nfrac);
    }
    else if(p+nuc <= dste)
    {
        if(e>=0) {
            token t = num_formatter<int64>::u_insert(p, dste-p, (int64)d, 10, 0, minsize, ALIGN_NUM_LEFT);
            p += t.len();

            d -= floor(d);
        }

        if(p<dste && nfrac!=0) {
            *p++ = '.';
        }

        //if(e<0)
        //    nfrac += nfrac<0 ? e : -e;

        p = append_fraction(p, dste, d, nfrac);
    }
    else
    {
        double mantissa = pow(10.0, l - double(e)); //>= 1 < 10

        //the part required for exponent
        int eabs = e<0 ? -e : e;
        int nexp = eabs>=10 ? 3 : 2;    //eXX
        if(e<0) ++nexp; //e-XX

        //if there's a place for the first number and dot
        if(p+nexp+2 > dste) {
            ::memset(dst, '?', dste-dst);
            return dste;
        }

        *p++ = '0' + (int)mantissa;
        *p++ = '.';

        for( char* me=dste-nexp; p<me; ) {
            mantissa -= floor(mantissa);
            mantissa *= 10.0;

            *p++ = '0' + (int)mantissa;
        }

        *p++ = 'e';
        if(e<0)
            *p++ = '-';
        if(eabs>=10) {
            *p++ = '0' + eabs/10;
            eabs = eabs % 10;
        }
        *p++ = '0' + eabs;
    }

    return p;
}

////////////////////////////////////////////////////////////////////////////////
char* append_fraction( char* dst, char* dste, double n, int ndig )
{
    int ndiga = int_abs(ndig);
    char* p = dst;

    if(dste-dst < ndiga)
        ndiga = dste-dst;

    n += 0.5*pow(10.0, -ndiga);

    int lastnzero=1;
    for( int i=0; i<ndiga; ++i )
    {
        n *= 10;
        double f = floor(n);
        n -= f;
        uint8 v = (uint8)f;
        *p++ = '0' + v;

        if( ndig >= 0  &&  v != 0 )
            lastnzero = i+1;
    }

    return dst + (ndig>0 ? lastnzero : ndiga);
}

////////////////////////////////////////////////////////////////////////////////
uints hex2bin( token& src, void* dst, uints nbytes, char sep )
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

////////////////////////////////////////////////////////////////////////////////
void bin2hex( const void* src, char*& dst, uints nitems, uint itemsize, char sep )
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
uints write_intelhex_line( char* dst, ushort addr, uchar n, const void* data )
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

} //namespace charstrconv
} //namespace coid
