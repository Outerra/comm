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

#ifndef __COID_COMM_CHARSTR__HEADER_FILE__
#define __COID_COMM_CHARSTR__HEADER_FILE__

#include "namespace.h"

#include "binstream/bstype.h"
#include "hash/hashfunc.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "token.h"
#include "dynarray.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///String class
class charstr
{
    friend binstream& operator >> (binstream &in, charstr& x);
    friend binstream& operator << (binstream &out, const charstr& x);
    friend opcd binstream_read_key( binstream &in, charstr& x );
    
    struct CHUNK { uchar a[4]; };

public:



    charstr() {}

    static charstr empty()  { return charstr(); }

    typedef seg_allocator::HEADER   HDR;
    //void * operator new (size_t size)   { return (charstr*) SINGLETON(segchunk_manager).alloc_hook(); }
    //void operator delete (void * ptr)   { SINGLETON(segchunk_manager).free_hook(ptr); }

    ///copy constructor
    charstr (const charstr& str) : _tstr(str._tstr)          { }

    ///Take control over content of another string, the other string becomes empty
    charstr& takeover( charstr& ref )
    {
        _tstr.takeover( ref._tstr );
        return *this;
    }

    ///Take control over content of another string, the other string becomes empty
    charstr& takeover( dynarray<char>& ref )
    {
        _tstr.takeover(ref);
        if( _tstr.size() > 0  &&  _tstr[_tstr.size()-1] != 0 )
            *_tstr.add() = 0;
        return *this;
    }

    ///Hand control over to the given dynarray<char> object
    void handover( dynarray<char>& dst )
    {
        dst.takeover(_tstr);
    }

    ///Swap strings
    void swap( charstr& ref )
    {
        _tstr.swap( ref._tstr );
    }
/*
    ///Share content with another string
    charstr& share( charstr& ref )
    {
        _tstr.share (ref._tstr);
        return *this;
    }
*/
    charstr( const char* czstr )                { set (czstr); }
    charstr( const char* czstr, uints len )      { set_from (czstr, len); }
    charstr( const token& tok )
    {
        if( tok._len == 0 ) return;
		assign( tok._ptr, tok._len+1 );
        _tstr[tok._len] = 0;
    }

    charstr& set( const char* czstr )
    {
        if(czstr)
            assign( czstr, (uints)strlen(czstr) + 1 );
        else
            free();
        return *this;
    }

    charstr& set( const charstr& str )
    {
        assign( str.ptr(), str.lent() );
        return *this;
    }


    const char* set_from( const token& tok )
    {
        if( tok._len == 0 )  { reset(); return tok._ptr; }
        assign( tok._ptr, tok._len+1 );
        _tstr[tok._len] = 0;
        return tok._ptr + tok._len;
    }

    const char* set_from( const char* czstr, uints slen )
    {
        if (slen == 0)  { reset(); return czstr; }
        assign (czstr, slen+1);
        _tstr[slen] = 0;
        return czstr + slen;
    }

    const char* set_from( const char* strbgn, const char* strend )
    {
        return set_from( strbgn, uints(strend-strbgn) );
    }

    
    const char* add_from( const token& tok )
    {
        if( tok._len == 0 )
            return tok._ptr;
        _append( tok._ptr, tok._len );
        _tstr[len()] = 0;
        return tok._ptr + tok._len;
    }

    const char* add_from( const char* czstr, uints slen )
    {
        if( slen == 0 )
            return czstr;
        _append( czstr, slen );
        _tstr[len()] = 0;
        return czstr + slen;
    }

    const char* add_from_range( const char* strbgn, const char* strend )
    {
        return add_from( strbgn, uints(strend-strbgn) );
    }


    char* copy_to( char* str, uints maxbufsize ) const
    {
        if( maxbufsize == 0 )  return str;

        uints lt = len();
        if( lt >= maxbufsize )
            lt = maxbufsize - 1;
        xmemcpy( str, _tstr.ptr(), lt );
        str[lt] = 0;

        return str;
    }

    const char* set_group( const char* czstr, const char* chars )
    {
        uints len = count_ingroup( czstr, chars );
        if( czstr[len] != 0 ) {
            assign( czstr, len+1 );
            _tstr[len] = 0;
            return czstr + len;
        }
        return czstr;
    }


    ///Cut to specified length, negative numbers cut abs(len) from the end
    charstr& trim_to_length( ints length )
    {
        if( length < 0 )
        {
            ints k = len() + length;
            if( k <= 0 )
                reset();
            else
            {
                _tstr.need( k+1, 2 );
                _tstr.ptr()[k]=0;
            }
        }
        else if( length+1 < (ints)_tstr.size() )
        {
            _tstr.need( length+1, 2 );
            _tstr.ptr()[length]=0;
        }
        else if( _tstr.size()>0 )
            termzero();

        return *this;
    }

    ///Cut trailing newlines (both \n and \r)
    charstr& trim( bool newline=true, bool whitespace=true )
    {
        if (_tstr.size() <= 1)  return *this;

        uints n = _tstr.size() - 1;  //without trailing zero
        for( uints i=n; i>0; )
        {
            --i;
            if( newline  &&  (_tstr[i] == '\n'  ||  _tstr[i] == '\r') )
                n = i;
            else if( whitespace  &&  (_tstr[i] == ' '  ||  _tstr[i] == '\t') )
                n = i;
            else
                break;
        }
        _tstr.need( n+1, 2 );
        _tstr[n] = 0;
        return *this;
    }

    //assignment
    charstr& operator = (const char *czstr) {
        uints len = czstr ? ((uints)strlen(czstr)+1) : 0;
        assign (czstr, len);
        return *this;
    }
    charstr& operator = (const charstr& str) {
        assign (str.ptr(), str.lent());
        return *this;
    }

    charstr& operator = (const token& tok)
    {
        if( tok._len == 0 )
            reset();
        else
        {
            assign( tok._ptr, tok._len+1 );
            _tstr[tok._len] = 0;
        }
        return *this;
    }

    charstr& operator = (char c)                { reset(); append(c); return *this; }
    
    charstr& operator = (int8 i)                { reset(); append_num(10,(int)i);  return *this; }
    charstr& operator = (uint8 i)               { reset(); append_num(10,(uint)i); return *this; }
    charstr& operator = (int16 i)               { reset(); append_num(10,(int)i);  return *this; }
    charstr& operator = (uint16 i)              { reset(); append_num(10,(uint)i); return *this; }
    charstr& operator = (int32 i)               { reset(); append_num(10,(int)i);  return *this; }
    charstr& operator = (uint32 i)              { reset(); append_num(10,(uint)i); return *this; }
    charstr& operator = (int64 i)               { reset(); append_num(10,i);       return *this; }
    charstr& operator = (uint64 i)              { reset(); append_num(10,i);       return *this; }

#ifdef SYSTYPE_MSVC
    charstr& operator = (ints i)                { reset(); append_num(10,(ints_to)i);  return *this; }
    charstr& operator = (uints i)               { reset(); append_num(10,(uints_to)i); return *this; }
#else
    charstr& operator = (long i)                { reset(); append_num(10,(ints_to)i);  return *this; }
    charstr& operator = (ulong i)               { reset(); append_num(10,(uints_to)i); return *this; }
#endif //SYSTYPE_MSVC

    charstr& operator = (double d)              { reset(); return operator += (d); }


    //@{ retrieve nth character
    char last_char() const                      { uints n=len(); return n ? _tstr[n-1] : 0; }
    char first_char() const                     { return _tstr.size() ? _tstr[0] : 0; }
    char nth_char( ints n ) const               { uints s=len(); return n<0 ? ((uints)-n<=s  ?  _tstr._ptr[s+n] : 0) : ((uints)n<s  ?  _tstr._ptr[n] : 0); } 
    //@}

    //@{ set nth character
    char last_char( char c )                    { uints n=len(); return n ? (_tstr[n-1]=c) : 0; }
    char first_char( char c)                    { return _tstr.size() ? (_tstr[0]=c) : 0; }
    char nth_char( ints n, char c )
    {
        uints s=len();
        return n<0
            ? ((uints)-n<=s  ?  (_tstr._ptr[s+n]=c) : 0)
            : ((uints)n<s  ?  (_tstr._ptr[n]=c) : 0);
    }
    //@}


    static uints count_notingroup( const char* str, const char* sep )
    {   return (uints)::strcspn( str, sep );   }

    static uints count_ingroup( const char* str, const char* sep )
    {   return (uints)::strspn( str, sep );   }

    
    //concatenation
    charstr& operator += (const char *czstr) {
        if(!czstr) return *this;
        uints len = (uints)::strlen(czstr);
        _append( czstr, len );
        return *this;
    }
    charstr& operator += (const charstr& str) {
        if( str.len() == 0 ) return *this;
        _append( str.ptr(), str.len() );
        return *this;
    }
    charstr& operator += (char c)               { append(c); return *this; }

    charstr& operator += (int8 i)               { append_num(10,(int)i);  return *this; }
    charstr& operator += (uint8 i)              { append_num(10,(uint)i); return *this; }
    charstr& operator += (int16 i)              { append_num(10,(int)i);  return *this; }
    charstr& operator += (uint16 i)             { append_num(10,(uint)i); return *this; }
    charstr& operator += (int32 i)              { append_num(10,(int)i);  return *this; }
    charstr& operator += (uint32 i)             { append_num(10,(uint)i); return *this; }
    charstr& operator += (int64 i)              { append_num(10,i);       return *this; }
    charstr& operator += (uint64 i)             { append_num(10,i);       return *this; }

#ifdef SYSTYPE_MSVC
    charstr& operator += (ints i)               { append_num(10,(ints_to)i);  return *this; }
    charstr& operator += (uints i)              { append_num(10,(uints_to)i); return *this; }
#else
    charstr& operator += (long i)               { append_num(10,(ints_to)i);  return *this; }
    charstr& operator += (ulong i)              { append_num(10,(uints_to)i); return *this; }
#endif //SYSTYPE_MSVC

    charstr& operator += (double d)
    {
        int dec;
        int sig;
#ifdef SYSTYPE_MSVC8plus
        char pbuf[32];
        char* p = pbuf;
        *p = 0;
        _fcvt_s( pbuf, 32, d, 3, &dec, &sig );
#else
        char* p = fcvt(d, 3, &dec, &sig);
#endif
        if (sig)
            append('-');
        if (dec > 0)
            add_from( p, dec );
        append('.');
        if (dec < 0)
        {
            for (; dec<0; ++dec)
                append('0');
        }

        _append( p+dec, (uints)::strlen(p+dec) );
        return *this;
    }

    charstr& operator += (long double d)
    {
        int dec;
        int sig;
#ifdef SYSTYPE_MSVC8plus
        char pbuf[32];
        char* p = pbuf;
        *p = 0;
        _fcvt_s( pbuf, 32, d, 3, &dec, &sig );
#else
        char* p = fcvt(d, 3, &dec, &sig);
#endif
        if (sig)
            append('-');
        if (dec > 0)
            add_from( p, dec );
        append ('.');
        if (dec < 0)
        {
            for (; dec<0; ++dec)
                append('0');
        }

        _append( p+dec, (uints)::strlen(p+dec) );
        return *this;
    }

    charstr& operator += (const token& tok) {
        _append( tok._ptr, tok._len );
        return *this;
    }

    //
    charstr& operator << (const char *czstr)    { return operator += (czstr); }
    charstr& operator << (const charstr& str)   { return operator += (str); }
    charstr& operator << (char c)               { return operator += (c); }

    charstr& operator << (int8 i)               { append_num(10,(int)i);  return *this; }
    charstr& operator << (uint8 i)              { append_num(10,(uint)i); return *this; }
    charstr& operator << (int16 i)              { append_num(10,(int)i);  return *this; }
    charstr& operator << (uint16 i)             { append_num(10,(uint)i); return *this; }
    charstr& operator << (int32 i)              { append_num(10,(int)i);  return *this; }
    charstr& operator << (uint32 i)             { append_num(10,(uint)i); return *this; }
    charstr& operator << (int64 i)              { append_num(10,i);       return *this; }
    charstr& operator << (uint64 i)             { append_num(10,i);       return *this; }

#ifdef SYSTYPE_MSVC
    charstr& operator << (ints i)               { append_num(10,(ints_to)i);  return *this; }
    charstr& operator << (uints i)              { append_num(10,(uints_to)i); return *this; }
#else
    charstr& operator << (long i)               { append_num(10,(ints_to)i);  return *this; }
    charstr& operator << (ulong i)              { append_num(10,(uints_to)i); return *this; }
#endif //SYSTYPE_MSVC

    charstr& operator << (double d)             { return operator += (d); }
    charstr& operator << (long double d)        { return operator += (d); }

    template <class T>
    charstr& operator << (const dynarray<T>& a)
    {
        uints n = a.size();
        if( n == 0 )
            return *this;

        --n;
        uints i;
        for( i=0; i<n; ++i )
            (*this) << a[i] << " ";
        (*this) << a[i];

        return *this;
    }

    friend charstr& operator << (charstr& out, const opcd_formatter& f)
    {
        out << f._e.error_desc();
        /*if( f._e.subcode() ) {
			char tmp[2] = "\0";
			*tmp = (char) f._e.subcode();
            out << " (code: " << tmp << ")";
		}*/

        if( f._e.text() && f._e.text()[0] )
            out << " : " << f._e.text();
        return out;
    }

    charstr& operator << (const token& tok)                 { return operator += (tok); }
    charstr& operator <<= (const token& tok)                { return operator += (tok); }



    ///Number alignment
    enum EAlignNum {
        ALIGN_NUM_LEFT_PAD_0            = -2,       ///< align left, pad with the '\0' character
        ALIGN_NUM_LEFT                  = -1,       ///< align left, pad with space
        ALIGN_NUM_CENTER                = 0,        ///< align center, pad with space
        ALIGN_NUM_RIGHT                 = 1,        ///< align right, fill with space
        ALIGN_NUM_RIGHT_FILL_ZEROS      = 2,        ///< align right, fill with '0' characters
    };

    ////////////////////////////////////////////////////////////////////////////////
    ///append number in baseN
    /**
        @param align    align output - fill with zeros(-2), to the left(-1), center(0), right(1)
    **/
    template< class INT >
    struct num
    {
        typedef typename SIGNEDNESS<INT>::SIGNED          SINT;
        typedef typename SIGNEDNESS<INT>::UNSIGNED        UINT;

        static UINT int_abs( SINT a )
        {
            return a - ((a+a) & (SINT(a)>>(8*sizeof(INT)-1)));
        }

        static token insert_signed( char* dst, uints dstsize, SINT n, int BaseN, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            if( n < 0 ) return insert( dst, dstsize, int_abs(n), BaseN, -1, minsize, align );
            else        return insert( dst, dstsize, n, BaseN, 0, minsize, align );
        }

        static token insert_signed_zt( char* dst, uints dstsize, SINT n, int BaseN, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            if( n < 0 ) return insert_zt( dst, dstsize, int_abs(n), BaseN, -1, minsize, align );
            else        return insert_zt( dst, dstsize, n, BaseN, 0, minsize, align );
        }

        static void append_signed( charstr& str, SINT n, int BaseN, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            if( n < 0 ) append( str, int_abs(n), BaseN, -1, minsize, align );
            else        append( str, n, BaseN, 0, minsize, align );
        }

        static token insert_zt( char* dst, uints dstsize, UINT n, int BaseN, int sgn=0, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            char buf[128];
            uints i = precompute( buf, n, BaseN, sgn );

            uints fc=0;              //fill count
            if( i < minsize )
                fc = minsize-i;

            if( dstsize < i+fc+1 )
                return token::empty();

            char* zt = produce( dst, buf, i, fc, sgn, align );
            *zt = 0;
            return token(dst, i+fc);
        }

        static token insert( char* dst, uints dstsize, UINT n, int BaseN, int sgn=0, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            char buf[128];
            uints i = precompute( buf, n, BaseN, sgn );

            uints fc=0;              //fill count
            if( i < minsize )
                fc = minsize-i;

            if( dstsize < i+fc )
                return token::empty();

            produce( dst, buf, i, fc, sgn, align );
            return token(dst, i+fc);
        }

        static void append( charstr& str, UINT n, int BaseN, int sgn=0, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
        {
            char buf[128];
            uints i = precompute( buf, n, BaseN, sgn );

            uints fc=0;              //fill count
            if( i < minsize )
                fc = minsize-i;

            char* p = str.get_append_buf( i+fc );

            char* zt = produce( p, buf, i, fc, sgn, align );
            *zt = 0;
        }

    protected:
        static uints precompute( char* buf, UINT n, int BaseN, int sgn )
        {
            uints i=0;
            if(n)
            {
                for( ; n; )
                {
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

    void append_num( int base, uint n, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {   num<uint>::append( *this, n, base, 0, minsize, align ); }

    void append_num( int base, int n, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {   num<uint>::append_signed( *this, n, base, minsize, align ); }

    void append_num( int base, uint64 n, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {   num<uint64>::append( *this, n, base, 0, minsize, align ); }

    void append_num( int base, int64 n, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {   num<uint64>::append_signed( *this, n, base, minsize, align ); }


    void append_num_int( int base, const void* p, uints bytes, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {
        switch( bytes )
        {
        case 1: append_num( base, (int)*(int8*)p, minsize, align );  break;
        case 2: append_num( base, (int)*(int16*)p, minsize, align );  break;
        case 4: append_num( base, (int)*(int32*)p, minsize, align );  break;
        case 8: append_num( base, *(int64*)p, minsize, align );  break;
        default:
            throw ersINVALID_TYPE "unsupported size";
        }
    }

    void append_num_uint( int base, const void* p, uints bytes, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {
        switch( bytes )
        {
        case 1: append_num( base, (uint)*(uint8*)p, minsize, align );  break;
        case 2: append_num( base, (uint)*(uint16*)p, minsize, align );  break;
        case 4: append_num( base, (uint)*(uint32*)p, minsize, align );  break;
        case 8: append_num( base, *(uint64*)p, minsize, align );  break;
        default:
            throw ersINVALID_TYPE "unsupported size";
        }
    }

    ///Append floating point number
    ///@param nfrac number of decimal places: >0 maximum, <0 precisely -nfrac places
    void append( float d, int nfrac )
    {
        float f = floorf(d);
        append_num( 10, (int)f );
        append('.');
        append_fraction( d-f, nfrac );
    }

    ///@param ndig number of decimal places: >0 maximum, <0 precisely -ndig places
    void append_fraction( float n, int ndig )
    {
        uint ndiga = (uint)int_abs(ndig);
        uint size = (uint)len();
        char* p = get_append_buf(ndiga);

        int lastnzero=1;
        for( uint i=0; i<ndiga; ++i )
        {
            n *= 10;
            float f = floorf(n);
            n -= f;
            int v = (int)f;
            *p++ = '0' + v;

            if( ndig >= 0  &&  v != 0 )
                lastnzero = i+1;
        }

        if( lastnzero < ndig )
            trim_to_length( size + lastnzero );
    }

    void append( char c )
    {
        *uniadd(1) = c;
    }

    ///Append n characters 
    void appendn( uint n, char c )
    {
        char *p = uniadd(n);
        for( ; n>0; --n )
            *p++ = c;
    }

    ///Append n uninitialized characters
    char* appendn_uninitialized( uint n )
    {
        return uniadd(n);
    }

    void append( const token& tok, uints filllen = 0, char fillchar=' ' )
    {
        if( filllen == 0 )
            filllen = tok._len;
        
        char* p = alloc_append_buf( filllen );
        uints n = tok._len > filllen ? filllen : tok._len;
        xmemcpy( p, tok._ptr, n );
        memset( p+n, fillchar, filllen - n );
    }

    ///Append UCS-4 character
    ///@return number of bytes written
    uint append_ucs4( ucs4 c )
    {
        if( c <= 0x7f ) {
            append((char)c);
            return 1;
        }
        else {
            char* p = get_append_buf(6);
            uchar n = write_utf8_seq( c, p );

            trim_to_length( n + uints(p - ptr()) );
            return n;
        }
    }

    ///Append wchar (UCS-2) buffer, converting it to the UTF-8 on the fly
    ///@param src pointer to the source buffer
    ///@param nchars number of characters in the source buffer, -1 if zero terminated
    ///@return number of bytes appended
    uints append_wchar_buf( const wchar_t* src, uints nchars )
    {
        uints nold = len();
        _tstr.set_size(nold);

        for( ; *src!=0 && nchars>0; ++src,--nchars )
        {
            if( *src <= 0x7f )
                *_tstr.add() = (char)*src;
            else
            {
                uints old = _tstr.size();
                char* p = _tstr.add(6);
                uint n = write_utf8_seq( *src, p );
                _tstr.set_size( old + n );
            }
        }
        if( _tstr.size() )
            *_tstr.add() = 0;

        return len() - nold;
    }

#ifdef SYSTYPE_WIN32
    ///Append wchar (UCS-2) buffer, converting it to the ANSI on the fly
    ///@param src pointer to the source buffer
    ///@param nchars number of characters in the source buffer, -1 if zero terminated
    uints append_wchar_buf_ansi( const wchar_t* src, uints nchars );
#endif

    ///Date element codes
    enum {
        DATE_WDAY           = 0x01,
        DATE_MDAY           = 0x02,
        DATE_MONTH          = 0x04,
        DATE_YEAR           = 0x08,
        DATE_HHMM           = 0x10,
        DATE_HHMMSS         = 0x20,
        DATE_TZ             = 0x40,
    };

    ///Append GMT date string constructed by the flags set
    charstr& append_date_gmt( const timet t, uint flg=UMAX )
    {
#ifdef SYSTYPE_MSVC8plus
        struct tm tm;
        gmtime_s( &tm, &t.t );
#else
        struct tm const& tm = *gmtime( (const time_t*)&t.t );
#endif
        return append_time( tm, flg, "GMT" );
    }

    charstr& append_date_local( const timet t, uint flg=UMAX )
    {
#ifdef SYSTYPE_MSVC8plus
        struct tm tm;
        localtime_s( &tm, &t.t );
        char tzbuf[32];
        if( flg & DATE_TZ ) {
            uints sz;
            _get_tzname( &sz, tzbuf, 32, 0 );
        }
        else
            tzbuf[0] = 0;
        return append_time( tm, flg, tzbuf );
#else
        struct tm const& tm = *localtime( (const time_t*)&t.t );
        return append_time( tm, flg, tzname[0] );
#endif
    }

    charstr& append_time( struct tm const& tm, uint flg, const token& tz )
    {
        //RFC 822, updated by RFC 1123
        //Tue, 15 Nov 1994 08:12:31 GMT
        static const char* wday[] = { "Sun, ", "Mon, ", "Tue, ", "Wed, ", "Thu, ", "Fri, ", "Sat, " };
        
        if( flg & DATE_WDAY )
            add_from( wday[tm.tm_wday], 5 );

        if( flg & DATE_MDAY ) {
            append_num(10, tm.tm_mday);
            append(' ');
        }

        static const char* mon[] = {
            "Jan ", "Feb ", "Mar ", "Apr ", "May ", "Jun ", "Jul ", "Aug ", "Sep ", "Oct ", "Nov ", "Dec "
        };
        if( flg & DATE_MONTH )
            add_from( mon[tm.tm_mon], 4 );
        
        if( flg & DATE_YEAR ) {
            append_num(10, tm.tm_year+1900);
            append(' ');
        }

        if( flg & (DATE_HHMM|DATE_HHMMSS) ) {
            append_num(10,tm.tm_hour,2,charstr::ALIGN_NUM_RIGHT_FILL_ZEROS);
            append(':');
            append_num(10,tm.tm_min,2,charstr::ALIGN_NUM_RIGHT_FILL_ZEROS);
        }

        if( flg & DATE_HHMMSS ) {
            append(':');
            append_num(10,tm.tm_sec,2,charstr::ALIGN_NUM_RIGHT_FILL_ZEROS);
        }

        if( flg & DATE_TZ ) {
            append(' ');
            append(tz);
        }

        return *this;
    }

    ///Append string while encoding characters as specified for URL encoding
    charstr& append_encode_url( const token& str )
    {
        static char charmap[256];
        static const char* hexmap = 0;

        if( !hexmap ) {
            ::memset( charmap, 0, sizeof(charmap) );

            for( uchar i='0'; i<='9'; ++i )  charmap[i] = 1;
            for( uchar i='a'; i<='z'; ++i )  charmap[i] = 1;
            for( uchar i='A'; i<='Z'; ++i )  charmap[i] = 1;
            const char* spec = "-_.~!*'(),";
            for( ; *spec; ++spec ) charmap[(uchar)*spec] = 1;

            hexmap = "0123456789abcdef";
        }

        const char* p = str.ptr();
        const char* pe = str.ptre();
        const char* ps = p;

        for( ; p<pe; ++p ) {
            uchar c = *p;

            if( !charmap[c] )
            {
                if(p-ps)
                    add_from( ps, p-ps );

                char* h = uniadd(3);
                h[0] = '%';
                h[1] = hexmap[c>>4];
                h[2] = hexmap[c&0x0f];

                ps = p+1;
            }
        }

        if(p-ps)
            add_from( ps, p-ps );

        return *this;
    }



    void replace( char from, char to )
    {
        char* pe = (char*)ptre();
        for( char* p = (char*)ptr(); p<pe; ++p )
            if( *p == from )  *p = to;
    }

    void tolower()
    {
        char* pe = (char*)ptre();
        for( char* p = (char*)ptr(); p<pe; ++p )
            *p = ::tolower(*p);
    }

    void toupper()
    {
        char* pe = (char*)ptre();
        for( char* p = (char*)ptr(); p<pe; ++p )
            *p = ::toupper(*p);
    }


    bool ins( uints pos, char c )
    {
        if( pos > len() )  return false;
        *_tstr.ins(pos) = c;
        return true;
    }

    bool ins( uints pos, const token& t )
    {
        if( pos > len() )  return false;
        xmemcpy( _tstr.ins(pos,t.len()), t.ptr(), t.len() );
        return true;
    }

    bool del( uints pos, uints n=1 )
    {
        if( pos+n > len() )  return false;
        _tstr.del(pos, n);
        return true;
    }


    char& operator [] (uints i)                  { return _tstr[i]; }
    
/*
    char& operator [] (char i)                  { return _tstr[(int)i]; }
    char& operator [] (uchar i)                 { return _tstr[(uint)i]; }

    char& operator [] (short i)                 { return _tstr[(int)i]; }
    char& operator [] (ushort i)                { return _tstr[(uint)i]; }

    char& operator [] (int i)                   { return _tstr[i]; }
    char& operator [] (uint i)                  { return _tstr[i]; }

    char& operator [] (int64 i)                 { return _tstr[i]; }
    char& operator [] (uint64 i)                { return _tstr[i]; }

    char operator [] (char i) const             { return _tstr[(int)i]; }
    char operator [] (uchar i) const            { return _tstr[(uint)i]; }

    char operator [] (short i) const            { return _tstr[(int)i]; }
    char operator [] (ushort i) const           { return _tstr[(uint)i]; }

    char operator [] (int i) const              { return _tstr[i]; }
    char operator [] (uint i) const             { return _tstr[i]; }

    char operator [] (int64 i) const            { return _tstr[i]; }
    char operator [] (uint64 i) const           { return _tstr[i]; }
*/

    bool operator == (const charstr& str) const
    {
        if( ptr() == str.ptr() )
            return 1;           //only when 0==0
        if( _tstr.size() != str._tstr.size() )
            return 0;
        return 0 == memcmp( _tstr.ptr(), str._tstr.ptr(), _tstr.size() );
        //return _tbuf.operator == (str._tbuf);
    }
    bool operator != (const charstr& str) const     { return !operator == (str); }

    bool operator == (const char *czstr) const {
        if( _tstr.ptr() == czstr )  return 1;
        if( !_tstr.ptr() )
            return czstr[0] == 0;
        return strcmp(_tstr.ptr(), czstr) == 0;
    }
    bool operator != (const char *czstr) const      { return !operator == (czstr); }

    bool operator == (const token& tok) const {
        if (tok._len != len())  return false;
        return strncmp(_tstr.ptr(), tok._ptr, tok._len) == 0;
    }

    bool operator != (const token& tok) const {
        if (tok._len != len())  return true;
        return strncmp(_tstr.ptr(), tok._ptr, tok._len) != 0;
    }

    bool operator == (const char c) const {
        if (_tstr.size() != 2)  return false;
        return _tstr[0] == c;
    }
    bool operator != (const char c) const {
        if (_tstr.size() != 2)  return true;
        return _tstr[0] != c;
    }

    bool operator > (const charstr& str) const {
        if( !len() )  return 0;
        if( !str.len() )  return 1;
        return strcmp( _tstr.ptr(), str._tstr.ptr() ) > 0;
    }
    bool operator < (const charstr& str) const {
        if( !str.len() )  return 0;
        if( !len() )  return 1;
        return strcmp( _tstr.ptr(), str._tstr.ptr() ) < 0;
    }
    bool operator >= (const charstr& str) const {
        return !operator < (str);
    }
    bool operator <= (const charstr& str) const {
        return !operator > (str);
    }

    bool operator > (const char *czstr) const {
        if( !len() )  return 0;
        if( !czstr || czstr[0] == 0 )  return 1;
        return strcmp( _tstr.ptr(), czstr ) > 0;
    }
    bool operator < (const char *czstr) const {
        if( !czstr || czstr[0] == 0 )  return 0;
        if( !len() )  return 1;
        return strcmp( _tstr.ptr(), czstr ) < 0;
    }
    bool operator >= (const char *czstr) const {
        return !operator < (czstr);
    }
    bool operator <= (const char *czstr) const {
        return !operator > (czstr);
    }

    bool operator > (const token& tok) const
    {
        if( !len() )  return 0;
        if( !tok._len )  return 1;

        int k = strncmp( _tstr.ptr(), tok._ptr, tok._len );
        if( k == 0 )
            return len() > tok._len;
        return k > 0;
    }

    bool operator < (const token& tok) const
    {
        if( !tok._len )  return 0;
        if( !len() )  return 1;

        int k = strncmp( _tstr.ptr(), tok._ptr, tok._len );
        return k < 0;
    }

    bool operator <= (const token& tok) const
    {
        return !operator > (tok);
    }

    bool operator >= (const token& tok) const
    {
        return !operator < (tok);
    }


    ////////////////////////////////////////////////////////////////////////////////
    char* get_buf( uints len )                   { return alloc_buf(len); }
    char* get_append_buf( uints len )            { return alloc_append_buf(len); }

    ///correct the size of the string if a terminating zero is found inside
    charstr& correct_size()
    {
        uints l = len();
        if (l>0)
        {
            uints n = ::strlen(_tstr.ptr());
            if (n<l)
                trim_to_length(n);
        }
        return *this;
    }

    char *copy32(char *p) const {
        uints len = get_aligned_size(_tstr.size(), 2);
        xmemcpy( p, _tstr.ptr(), len );
        return p + len;
    }

    bool is_empty() const {
        return _tstr.size () <= 1;
    }

    dynarray<char>& dynarray_ref()      { return _tstr; }

    const char* ptr() const             { return _tstr.ptr(); }
    const char* ptre() const            { return _tstr.ptr() + len(); }

    const char* c_str() const           { return _tstr.size() ? _tstr.ptr() : ""; }

    ///<String length excluding terminating zero
    uints len() const                   { return _tstr.size() ? (_tstr.size() - 1) : 0; }

    ///<String length counting terminating zero
    uints lent() const                  { return _tstr.size(); }

    ///<String allocated length, always aligned to 4-byte
    uints lenf() const                  { return (_tstr.size() + 3) & ~3; }

    void free ()                        { _tstr.discard(); }

    char* reserve( uints len )          { return _tstr.reserve(len, true); }
    void reset()                        { _tstr.reset();  if (_tstr._ptr) _tstr._ptr[0]=0; }

    //operator const char *() const       { return _tstr; }
    //operator char *()                   { return _tstr; }

    ~charstr() {
        //_tbuf.free();
    }


protected:
    static uints reserve_count( uints len )         { return ((len+3)&~3) - len; }

    void assign( const char *czstr, uints len )
    {
        if( len == 0 ) {
            reset();
            return;
        }
        _tstr.need_new( len, 2 );
        xmemcpy( _tstr.ptr(), czstr, len );
        termzero();
        //fill_align();
    }

    void _append( const char *czstr, uints len )
    {
        if(!len) return;
        xmemcpy( alloc_append_buf(len), czstr, len );
    }


    char* alloc_buf( uints len )
    {
        if(!len) {
            _tstr.reset();
            return 0;
        }

        char* p = _tstr.need_new( len+1, 2 );
        termzero();

        return p;
    }

    char* alloc_append_buf( uints len )
    {
        if(!len) return 0;
        return uniadd(len);
    }

    /*void fill_align()
    {
        uints len = _tstr.size();
        if( !len )  return;
        uints lea = get_aligned_size( len, 2 );
        for( --len; len<lea; ++len )
            ((char*)_tstr)[len] = 0;
    }*/

    void termzero()
    {
        uints len = _tstr.size();
        _tstr[len-1] = 0;
    }


    dynarray<char> _tstr;

public:


    bool begins_with( const char* str ) const
    {
        uints n = len();
        for( uints i=0; i<n; ++i )
        {
            if( str[i] == 0 )
                return true;
            if( str[i] != _tstr[i] )
                return false;
        }
        return false;
    }

    bool begins_with( const token& tok ) const
    {
        if( tok._len > len() )
            return false;
        for( uints i=0; i<tok._len; ++i )
        {
            if( tok._ptr[i] != _tstr[i] )
                return false;
        }
        return true;
    }

    bool begins_with_icase( const char* str ) const
    {
        uints n = len();
        uints i;
        for( i=0; i<n; ++i )
        {
            if( str[i] == 0 )
                return true;
            if( ::tolower(str[i]) != ::tolower(_tstr[i]) )
                return false;
        }
        return str[i] == 0;
    }

    bool begins_with_icase( const token& tok ) const
    {
        if( tok._len > len() )
            return false;
        for( uints i=0; i<tok._len; ++i )
        {
            if( ::tolower(tok._ptr[i]) != ::tolower(_tstr[i]) )
                return false;
        }
        return true;
    }

    bool ends_with( const token& tok ) const
    {
        if( tok._len > len() )
            return false;
        const char* p = _tstr.ptr() + len() - tok.len();
        for( uints i=0; i<tok._len; ++i )
        {
            if( tok._ptr[i] != p[i] )
                return false;
        }
        return true;
    }

    bool ends_with_icase( const token& tok ) const
    {
        if( tok._len > len() )
            return false;
        const char* p = _tstr.ptr() + len() - tok.len();
        for( uints i=0; i<tok._len; ++i )
        {
            if( ::tolower(tok._ptr[i]) != ::tolower(p[i]) )
                return false;
        }
        return true;
    }



    uints touint( uints offs=0 ) const       { return token(ptr()+offs, ptre()).touint(); }
    ints toint (uints offs=0) const          { return token(ptr()+offs, ptre()).toint(); }
    
    uints xtouint( uints offs=0 ) const      { return token(ptr()+offs, ptre()).xtouint(); }
    ints xtoint (uints offs=0) const         { return token(ptr()+offs, ptre()).xtoint(); }

    double todouble (uints offs=0) const     { return token(ptr()+offs, ptre()).todouble(); }

    ////////////////////////////////////////////////////////////////////////////////
    bool cmpeq( const token& str ) const
    {
        if( _tstr.size() != str.len() )
            return 0;
        return 0 == memcmp( _tstr.ptr(), str.ptr(), str.len() );
    }
    bool cmpeq( const char* str ) const
    {
        if( _tstr.size() == 0 )  return str == 0  ||  str[0] == 0;
        return 0 == strcmp( _tstr.ptr(), str );
    }

    bool cmpeqi( const token& str ) const
    {
        if( _tstr.size() != str.len() )
            return 0;
        return 0 == strncasecmp( _tstr.ptr(), str.ptr(), str.len() );
    }
    bool cmpeqi( const char* str ) const
    {
        if( _tstr.size() == 0 )  return str == 0  ||  str[0] == 0;
        return 0 == strcasecmp( _tstr.ptr(), str );
    }

    bool cmpeqc( const token& str, bool casecmp ) const      { return casecmp ? cmpeq(str) : cmpeqi(str); }
    bool cmpeqc( const char* str, bool casecmp ) const       { return casecmp ? cmpeq(str) : cmpeqi(str); }

    ////////////////////////////////////////////////////////////////////////////////
    int cmp( const token& str ) const
    {
        uints len = _tstr.size();
        uints lex = str.len();
        int r = memcmp( _tstr.ptr(), str.ptr(), uint_min(len,lex) );
        if( r == 0 )
        {
            if( len<lex )  return -1;
            if( lex<len )  return 1;
        }
        return r;
    }
    int cmp( const char* str ) const        { return strcmp( _tstr.ptr(), str ); }

    int cmpi( const token& str ) const
    {
        uints len = _tstr.size();
        uints lex = str.len();
        int r = strncasecmp( _tstr.ptr(), str.ptr(), uint_min(len,lex) );
        if( r == 0 )
        {
            if( len<lex )  return -1;
            if( lex<len )  return 1;
        }
        return r;
    }
    int cmpi( const char* str ) const       { return strcasecmp( _tstr.ptr(), str ); }

    int cmpc( const token& str, bool casecmp ) const      { return casecmp ? cmp(str) : cmpi(str); }
    int cmpc( const char* str, bool casecmp ) const       { return casecmp ? cmp(str) : cmpi(str); }

    char char_is_alpha( ints n ) const
    {
        char c = nth_char(n);
        return (c >= 'a' && c <= 'z') || (c >='A' && c <= 'Z') ? c : 0;
    }

    char char_is_number( ints n ) const
    {
        char c = nth_char(n);
        return (c >= '0' && c <= '9') ? c : 0;
    }

    char char_is_alphanum( ints n ) const
    {
        char c = nth_char(n);
        return (c >= 'a' && c <= 'z') || (c >='A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : 0;
    }

    // extract from token
    charstr& set_left( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_left( c, 0, def_empty );
    }

    charstr& set_left( token str, const token& separators, bool def_empty=false )
    {
        return *this = str.cut_left( separators, 0, def_empty );
    }

    charstr& set_left_back( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_left_back( c, 0, def_empty );
    }

    charstr& set_left_back( token str, const token& separators, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_left_back( separators, 0, def_empty );
    }

    //
    charstr& set_right( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_right( c, 0, def_empty );
    }

    charstr& set_right( token str, const token& separators, bool def_empty=false )
    {
        return *this = str.cut_right( separators, 0, def_empty );
    }

    charstr& set_right_back( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_right_back( c, 0, def_empty );
    }

    charstr& set_right_back( token str, const token& separators, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_right_back( separators, 0, def_empty );
    }


    //extract token from string
    token select_left( const char c, bool def_empty=false ) const    //up to, but without the character c
    {
        token r = *this;
        return r.cut_left( c, 0, def_empty );
    }

    token select_left( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_left( separators, 0, def_empty );
    }

    token select_left_back( const char c, bool def_empty=false ) const     //up to, but without the character c
    {
        token r = *this;
        return r.cut_left_back( c, 0, def_empty );
    }

    token select_left_back( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_left_back( separators, 0, def_empty );
    }

    token select_right( const char c, bool def_empty=false ) const     //up to, but without the character c
    {
        token r = *this;
        return r.cut_right( c, 0, def_empty );
    }

    token select_right( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_right( separators, 0, def_empty );
    }

    token select_right_back( const char c, bool def_empty=false ) const     //up to, but without the character c
    {
        token r = *this;
        return r.cut_right_back( c, 0, def_empty );
    }

    token select_right_back( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_right_back( separators, 0, def_empty );
    }




    token get_after_substring( const substring& sub ) const
    {
        token s = *this;
        return s.get_after_substring(sub);
    }

    token get_before_substring( const substring& sub ) const
    {
        token s = *this;
        return s.get_before_substring(sub);
    }


    charstr  operator + (const token& tok) const
    {
        charstr res = *this;
        res += tok;
        return res;
    }

    charstr operator + (const charstr& p) const
    {
        charstr res = *this;
        res += p;
        return res;
    }

    charstr operator + (const char* p) const
    {
        charstr res = *this;
        res += p;
        return res;
    }


protected:

    ///Add n uninitialized characters, plus one character for the terminating zero if it's not there already 
    char* uniadd( uints n )
    {
        char* p = (_tstr.size() == 0)
            ? _tstr.add(n+1)
            : _tstr.add(n)-1;
        p[n] = 0;

        return p;
    }
};


////////////////////////////////////////////////////////////////////////////////
inline token::token( const charstr& str )       { set(str); }

inline token& token::operator = ( const charstr& t )
{
    _ptr = t.ptr();
    _len = t.len();
    return *this;
}

inline bool operator == (const token& tok, const charstr& str )
{
    if (tok._len != str.len() ) return 0;
    return memcmp( tok._ptr, str.ptr(), tok._len) == 0;
}

inline bool operator != (const token& tok, const charstr& str )
{
    if (tok._len != str.len() ) return 1;
    return memcmp( tok._ptr, str.ptr(), tok._len) != 0;
}

inline const char* token::set( const charstr& str )
{
    _ptr = str.ptr();
    _len = str.len();
    return _ptr+_len;
}

inline void token::assign( const charstr& str )
{
    _ptr = str.ptr();
    _len = 0;
}

template<class A>
inline bool token::utf8_to_wchar_buf( dynarray<wchar_t,A>& dst ) const
{
    dst.reset();
    uints n = len();
    const char* p = ptr();
    
    while(n>0)
    {
        if( (uchar)*p <= 0x7f ) {
            *dst.add() = *p++;
            --n;
        }
        else
        {
            uint ne = get_utf8_seq_expected_bytes(p);
            if( ne > n )  return false;

            *dst.add() = (wchar_t)read_utf8_seq(p);
            p += ne;
            n -= ne;
        }
    }
    *dst.add() = 0;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
inline binstream& operator >> (binstream &in, charstr& x)
{
    x._tstr.reset();
    dynarray<char>::binstream_container c(x._tstr);

    in.xread_array(c);
    if( x._tstr.size() )
        *x._tstr.add() = 0;
    return in;
}

inline binstream& operator << (binstream &out, const charstr& x)
{
    return out.xwrite_token( x.ptr(), x.len() );
}

inline binstream& operator << (binstream &out, const token& x)
{
    return out.xwrite_token(x);
}

inline binstream& operator >> (binstream &out, token& x)
{
    //this should not be called ever
    RASSERT(0);
    return out;
}

////////////////////////////////////////////////////////////////////////////////
///Wrappers for making key-type out of std charstr and token classes
namespace bstype {

template<>
struct t_key<charstr> : public charstr
{
    friend binstream& operator >> (binstream &in, t_key<charstr>& x);
    friend binstream& operator << (binstream &out, const t_key<charstr>& x);
};

template<>
struct t_key<token> : public token
{
    friend binstream& operator << (binstream &out, const t_key<token>& x);
};


////////////////////////////////////////////////////////////////////////////////
inline binstream& operator >> (binstream &in, t_key<charstr>& x)
{
    x.reset();
    dynarray<key>::binstream_container c((dynarray<key>&)x.dynarray_ref());

    in.xread_array(c);
    if( x._tstr.size() )
        *x._tstr.add() = 0;

    return in;
}

inline binstream& operator << (binstream &out, const t_key<charstr>& x)
{
    return out.xwrite_key( x.ptr(), x.len() );
}

inline binstream& operator << (binstream &out, const t_key<token>& x)
{
    return out.xwrite_key(x);
}

} //namespace bstype


////////////////////////////////////////////////////////////////////////////////
inline opcd binstream_read_key( binstream &in, charstr& x )
{
    x.reset();
    dynarray<bstype::key>::binstream_container c((dynarray<bstype::key>&)x.dynarray_ref());

    opcd e = in.read_array(c);
    if( x._tstr.size() )
        *x._tstr.add() = 0;

    return e;
}

inline opcd binstream_write_key( binstream &out, const charstr& x )
{
    return out.write_key( x.ptr(), x.len() );
}

inline opcd binstream_write_token( binstream &out, const token& x )
{
    return out.write_key(x);
}


////////////////////////////////////////////////////////////////////////////////
template<> struct hash<charstr>
{
    typedef charstr type_key;

    size_t operator() (const charstr& str) const
    {
        return __coid_hash_string( str.ptr(), str.len() );
    }
};

template<> struct hash<token>
{
    typedef token type_key;

    size_t operator() (const token& tok) const
    {
        return __coid_hash_string( tok._ptr, tok._len );
    }
};


////////////////////////////////////////////////////////////////////////////////
//
struct ndigit_fmt
{
    ints _i;
    uint _numc;
    charstr::EAlignNum _align;
    
    explicit ndigit_fmt( ints i, uint numc, charstr::EAlignNum align = charstr::ALIGN_NUM_RIGHT )
        : _i(i),_numc(numc),_align(align) { }

    friend binstream& operator << (binstream& out, const ndigit_fmt& d)
    {
        char buf[128];
        token t = charstr::num<ints>::insert_signed( buf, 128, d._i, 10, d._numc, d._align );
        //d._buf.append_num( 10, d._i, d._numc, d._align );
        return out << t;
    }
};
/*
struct nchar_fmt
{
    token _str;
    uints  _fill;

    explicit nchar_fmt( const token& str, uints n )
    {
        _str._ptr = str._ptr;
        if( str._len > n )
        {
            _str._len = n;
            _fill = 0;
        }
        else
        {
            _str._len = str._len;
            _fill = n - str._len;
        }
    }

    friend binstream& operator << (binstream& out, const nchar_fmt& d)
    {
        binstream::container_fixed_array<char> c( d._str.ptr(), d._str.len() );
        out.xwrite_array(c);
        c.set( d._str._ptr, d._fill);
        out.write( d._str._ptr, d._fill, binstream::type::t_char );
        return out;
    }
};
*/



////////////////////////////////////////////////////////////////////////////////
inline charstr& opcd_formatter::text( charstr& dst ) const
{
    dst << _e.error_desc();

    if( _e.text() && _e.text()[0] )
        dst << " : " << _e.text();
    return dst;
}

////////////////////////////////////////////////////////////////////////////////
/// This is lazy command tokenizer, expecting that no one will ever want him to
/// actually do something
struct command_tokens
{
    token  _cmd;                ///< command line
    token  _rtok;               ///< remaining token
    token  _ctok;               ///< current token
    int _tokn;                  ///< current token number

    token operator[] (ints i)
    {
        if( i < _tokn )
        {
            _rtok = _cmd;
            _tokn = -1;
            _ctok._ptr = _cmd._ptr;
            _ctok._len = 0;
        }

        if( i == _tokn )
            return _ctok;

        for( ; _tokn < i; ++_tokn )
        {
            _ctok = _rtok.cut_left( token::TK_whitespace(), -1 );
            if( _ctok.is_empty() )  { ++_tokn; break; }
        }

        return _ctok;
    }

    command_tokens()
    {
        _cmd.set_empty();
        _tokn = -1;
        _rtok = _cmd;
        _ctok.set_empty();
    }

    command_tokens( const token& str )
    {
        _cmd = str;
        _cmd.skip_ingroup( token::TK_whitespace() );
        _tokn = -1;
        _rtok = _cmd;
        _ctok.set_empty();
    }
};



COID_NAMESPACE_END

#endif //__COID_COMM_CHARSTR__HEADER_FILE__
