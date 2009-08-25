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

    COIDNEWDELETE(charstr);

    struct output_iterator : std::iterator<std::output_iterator_tag, char>
    {
        charstr* _p;                    ///<ptr to the managed item

        char& operator *(void) const {
            return *_p->uniadd(1);
        }

        output_iterator& operator ++()  { return *this; }
        output_iterator& operator ++(int)  { return *this; }

        output_iterator() { _p = 0; }
        output_iterator( charstr& p ) : _p(&p)  { }
    };

    charstr() {}

    static charstr empty()  { return charstr(); }

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
    template<class COUNT>
    charstr& takeover( dynarray<char,COUNT>& ref )
    {
        _tstr.takeover(ref);
        if( _tstr.size() > 0  &&  _tstr[_tstr.size()-1] != 0 )
            *_tstr.add() = 0;
        return *this;
    }

    ///Take control over content of another string, the other string becomes empty
    template<class COUNT>
    charstr& takeover( dynarray<uchar,COUNT>& ref )
    {
        _tstr.discard();
        uchar* p = ref.ptr();
        ref.set_dynarray_conforming_ptr(0);
        _tstr.set_dynarray_conforming_ptr((char*)p);

        if( _tstr.size() > 0  &&  _tstr[_tstr.size()-1] != 0 )
            *_tstr.add() = 0;
        return *this;
    }

    ///Hand control over to the given dynarray<char> object
    void handover( dynarray<char,uint>& dst )
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
    charstr( const char* czstr )            { set (czstr); }
    charstr( const char* czstr, uints len ) { set_from (czstr, len); }
    charstr( const token& tok )
    {
        if( tok.is_empty() ) return;
		assign( tok.ptr(), tok.len()+1 );
        _tstr[tok.len()] = 0;
    }


    charstr& set( const char* czstr )
    {
        if(czstr)
            assign( czstr, strlen(czstr) + 1 );
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
        if( tok.is_empty() )  { reset(); return tok.ptr(); }
        assign( tok.ptr(), tok.len()+1 );
        _tstr[tok.len()] = 0;
        return tok.ptre();
    }

    const char* set_from( const char* czstr, uints slen )
    {
        if(slen == 0)
        { reset(); return czstr; }

        DASSERT( slen <= UMAX32 );

        assign(czstr, slen+1);
        _tstr[slen] = 0;
        return czstr + slen;
    }

    const char* set_from( const char* strbgn, const char* strend )
    {
        return set_from( strbgn, strend-strbgn );
    }

    
    const char* add_from( const token& tok )
    {
        if( tok.is_empty() )
            return tok.ptr();
        _append( tok.ptr(), tok.len() );
        _tstr[tok.len()] = 0;
        return tok.ptre();
    }

    const char* add_from( const char* czstr, uints slen )
    {
        if( slen == 0 )
            return czstr;

        DASSERT( slen <= UMAX32 );

        _append( czstr, slen );
        _tstr[len()] = 0;
        return czstr + slen;
    }

    const char* add_from_range( const char* strbgn, const char* strend )
    {
        return add_from( strbgn, strend-strbgn );
    }

    ///Copy to buffer, terminate with zero
    char* copy_to( char* str, uints maxbufsize ) const
    {
        if( maxbufsize == 0 )
            return str;

        uints lt = lens();
        if( lt >= maxbufsize )
            lt = maxbufsize - 1;
        xmemcpy( str, _tstr.ptr(), lt );
        str[lt] = 0;

        return str;
    }

    ///Copy to buffer, unterminated
    uints copy_raw_to( char* str, uints maxbufsize ) const
    {
        if( maxbufsize == 0 )
            return 0;

        uints lt = lens();
        if( lt > maxbufsize )
            lt = maxbufsize;
        xmemcpy( str, _tstr.ptr(), lt );

        return lt;
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
    charstr& resize( ints length )
    {
        if( length < 0 )
        {
            if( (uints)-length >= lens() )
                reset();
            else {
                _tstr.realloc(lens() + length + 1);
                termzero();
            }
        }
        else {
            uints ts = lens();
            DASSERT( ts+length <= UMAX32 );
            
            if( (uints)length < ts )
            {
                _tstr.realloc( length+1 );
                _tstr.ptr()[length]=0;
            }
            else if( _tstr.size()>0 )
                termzero();
        }

        return *this;
    }

    ///Cut trailing newlines (both \n and \r)
    charstr& trim( bool newline=true, bool whitespace=true )
    {
        if (_tstr.size() <= 1)  return *this;

        uints n = _tstr.sizes() - 1;  //without the trailing zero
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
        _tstr.realloc(n+1);
        _tstr[n] = 0;
        return *this;
    }

    //assignment
    charstr& operator = (const char *czstr) {
        uints len = czstr ? (strlen(czstr)+1) : 0;
        assign(czstr, len);
        return *this;
    }
    charstr& operator = (const charstr& str) {
        assign (str.ptr(), str.lent());
        return *this;
    }

    charstr& operator = (const token& tok)
    {
        if( tok.is_empty() )
            reset();
        else
            assign( tok.ptr(), tok.len()+1 );
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
# ifdef SYSTYPE_32
    charstr& operator = (ints i)                { reset(); append_num(10,(ints)i);  return *this; }
    charstr& operator = (uints i)               { reset(); append_num(10,(uints)i); return *this; }
# else //SYSTYPE_64
    charstr& operator = (int i)                 { reset(); append_num(10,i); return *this; }
    charstr& operator = (uint i)                { reset(); append_num(10,i); return *this; }
# endif
#elif defined(SYSTYPE_32)
    charstr& operator = (long i)                { reset(); append_num(10,(ints)i);  return *this; }
    charstr& operator = (ulong i)               { reset(); append_num(10,(uints)i); return *this; }
#endif //SYSTYPE_MSVC

    charstr& operator = (double d)              { reset(); return operator += (d); }

    ///Formatted numbers
    template<int WIDTH, int ALIGN, class NUM>
    charstr& operator = (const num_fmt<WIDTH,ALIGN,NUM> v) {
        append_num(10, v.value, WIDTH, ALIGN);
        return *this;
    }

    //@{ retrieve nth character
    char last_char() const                      { uints n=lens(); return n ? _tstr[n-1] : 0; }
    char first_char() const                     { return _tstr.size() ? _tstr[0] : 0; }
    char nth_char( ints n ) const
    {
        uints s = lens();
        return n<0
            ? ((uints)-n<=s  ?  _tstr[s+n] : 0)
            : ((uints)n<s  ?  _tstr[n] : 0);
    }
    //@}

    //@{ set nth character
    char last_char( char c )                    { uints n=lens(); return n ? (_tstr[n-1]=c) : 0; }
    char first_char( char c)                    { return _tstr.size() ? (_tstr[0]=c) : 0; }
    char nth_char( ints n, char c )
    {
        uints s = lens();
        return n<0
            ? ((uints)-n<=s  ?  (_tstr[s+n]=c) : 0)
            : ((uints)n<s  ?  (_tstr[n]=c) : 0);
    }
    //@}


    static uint count_notingroup( const char* str, const char* sep )
    {   return (uint)::strcspn( str, sep );   }

    static uint count_ingroup( const char* str, const char* sep )
    {   return (uint)::strspn( str, sep );   }


    //concatenation
    charstr& operator += (const char *czstr) {
        if(!czstr) return *this;
        uints len = ::strlen(czstr);
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
# ifdef SYSTYPE_32
    charstr& operator += (ints i)               { append_num(10,(ints)i);  return *this; }
    charstr& operator += (uints i)              { append_num(10,(uints)i); return *this; }
# else //SYSTYPE_64
    charstr& operator += (int i)                { append_num(10,i); return *this; }
    charstr& operator += (uint i)               { append_num(10,i); return *this; }
# endif
#elif defined(SYSTYPE_32)
    charstr& operator += (long i)               { append_num(10,(ints)i);  return *this; }
    charstr& operator += (ulong i)              { append_num(10,(uints)i); return *this; }
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

        _append( p+dec, ::strlen(p+dec) );
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

        _append( p+dec, ::strlen(p+dec) );
        return *this;
    }

    ///Formatted numbers
    template<int WIDTH, int ALIGN, class NUM>
    charstr& operator += (const num_fmt<WIDTH,ALIGN,NUM> v) {
        append_num(10, v.value, WIDTH, ALIGN);
        return *this;
    }

    charstr& operator += (const token& tok) {
        _append( tok.ptr(), tok.len() );
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
# ifdef SYSTYPE_32
    charstr& operator << (ints i)               { append_num(10,(ints)i);  return *this; }
    charstr& operator << (uints i)              { append_num(10,(uints)i); return *this; }
# else //SYSTYPE_64
    charstr& operator << (int i)                { append_num(10,i); return *this; }
    charstr& operator << (uint i)               { append_num(10,i); return *this; }
# endif
#elif defined(SYSTYPE_32)
    charstr& operator << (long i)               { append_num(10,(ints)i);  return *this; }
    charstr& operator << (ulong i)              { append_num(10,(uints)i); return *this; }
#endif //SYSTYPE_MSVC

    charstr& operator << (double d)             { return operator += (d); }
    charstr& operator << (long double d)        { return operator += (d); }

    ///Formatted numbers
    template<int WIDTH, int ALIGN, class NUM>
    charstr& operator << (const num_fmt<WIDTH,ALIGN,NUM> v) {
        append_num(10, v.value, WIDTH, (EAlignNum)ALIGN);
        return *this;
    }

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
        out << f.e.error_desc();
        /*if( f._e.subcode() ) {
			char tmp[2] = "\0";
			*tmp = (char) f._e.subcode();
            out << " (code: " << tmp << ")";
		}*/

        if( f.e.text() && f.e.text()[0] )
            out << " : " << f.e.text();
        return out;
    }

    charstr& operator << (const token& tok)                 { return operator += (tok); }
    charstr& operator <<= (const token& tok)                { return operator += (tok); }



    ////////////////////////////////////////////////////////////////////////////////
    ///Append signed number
    template<class NUM>
    void append_num_signed( NUM n, int BaseN, uints minsize, EAlignNum align )
    {
        if( n < 0 ) append_num_unsigned( int_abs(n), BaseN, -1, minsize, align );
        else        append_num_unsigned( n, BaseN, 0, minsize, align );
    }

    ///Append unsigned number
    template<class NUM>
    void append_num_unsigned( NUM n, int BaseN, int sgn, uints minsize, EAlignNum align )
    {
        char buf[128];
        uints i = charstrconv::num_formatter<NUM>::precompute( buf, n, BaseN, sgn );

        uints fc=0;              //fill count
        if( i < minsize )
            fc = minsize-i;

        char* p = get_append_buf( i+fc );

        char* zt = charstrconv::num_formatter<NUM>::produce( p, buf, i, fc, sgn, align );
        *zt = 0;
    }

    template<class NUM>
    void append_num( int base, NUM n, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT ) \
    {
        if( SIGNEDNESS<NUM>::isSigned )
            return append_num_signed(n, base, minsize, align);
        else
            return append_num_unsigned(n, base, 0, minsize, align);
    }

    void append_num_int( int base, const void* p, uints bytes, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {
        switch( bytes )
        {
        case 1: append_num_signed( *(int8*)p, base, minsize, align );  break;
        case 2: append_num_signed( *(int16*)p, base, minsize, align );  break;
        case 4: append_num_signed( *(int32*)p, base, minsize, align );  break;
        case 8: append_num_signed( *(int64*)p, base, minsize, align );  break;
        default:
            throw ersINVALID_TYPE "unsupported size";
        }
    }

    void append_num_uint( int base, const void* p, uints bytes, uints minsize=0, EAlignNum align=ALIGN_NUM_RIGHT )
    {
        switch( bytes )
        {
        case 1: append_num_unsigned( *(uint8*)p, base, 0, minsize, align );  break;
        case 2: append_num_unsigned( *(uint16*)p, base, 0, minsize, align );  break;
        case 4: append_num_unsigned( *(uint32*)p, base, 0, minsize, align );  break;
        case 8: append_num_unsigned( *(uint64*)p, base, 0, minsize, align );  break;
        default:
            throw ersINVALID_TYPE "unsupported size";
        }
    }

    ///Append number with thousands separated
    void append_num_formatted( int64 n, char thousand_sep = ' ' )
    {
        if(n == 0)
            return append('0');

        int mods[(64+10-1)/10];
        uints k=0;

        for( ; n>=1000 || n<=-1000; ++k) {
            mods[k] = n % 1000;
            n = n / 1000;
        }

        append_num( 10, n );

        for( ; k>0; ) {
            --k;
            if(thousand_sep)
                append(thousand_sep);

            append_num( 10, int_abs(mods[k]), 3, ALIGN_NUM_RIGHT_FILL_ZEROS );
        }
    }

    ///Append time (secs)
    void append_time_formatted( uint64 n, bool msec=false )
    {
        if(n == 0) {
            *this << "00:00";
            return;
        }

        uint ms;
        if(msec) {
            ms = uint(n % 1000);
            n /= 1000;
        }

        uint hrs = uint(n / 3600);
        uint hs = uint(n % 3600);
        uint mns = hs / 60;
        uint sec = hs % 60;

        append_num(10, hrs);
        append(':');
        append_num(10, mns, 2, ALIGN_NUM_RIGHT_FILL_ZEROS);
        append(':');
        append_num(10, sec, 2, ALIGN_NUM_RIGHT_FILL_ZEROS);

        if(msec) {
            append('.');
            append_num(10, ms, 3, ALIGN_NUM_RIGHT_FILL_ZEROS);
        }
    }

    ///Append floating point number
    ///@param nfrac number of decimal places: >0 maximum, <0 precisely -nfrac places
    void append( double d, int nfrac, int minsize=0)
    {
        double w = d>0 ? floor(d) : ceil(d);
        append_num( 10, (int64)w, minsize );
        append('.');
        append_fraction( fabs(d-w), nfrac );
    }

    ///@param ndig number of decimal places: >0 maximum, <0 precisely -ndig places
    void append_fraction( double n, int ndig )
    {
        uint ndiga = int_abs(ndig);
        uints size = lens();
        char* p = get_append_buf(ndiga);

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

        if( lastnzero < ndig )
            resize( size + lastnzero );
    }

    void append( char c )
    {
        *uniadd(1) = c;
    }

    ///Append n characters 
    void appendn( uints n, char c )
    {
        char *p = uniadd(n);
        for( ; n>0; --n )
            *p++ = c;
    }

    ///Append n uninitialized characters
    char* appendn_uninitialized( uints n )
    {
        return uniadd(n);
    }

    void append( const token& tok, uints filllen = 0, char fillchar=' ' )
    {
        uints len = tok.lens();
        if( filllen == 0 )
            filllen = len;

        char* p = alloc_append_buf( filllen );
        uints n = len > filllen ? filllen : len;
        xmemcpy( p, tok.ptr(), n );
        if(filllen > n)
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

            resize( n + (p - ptr()) );
            return n;
        }
    }

    ///Append wchar (UCS-2) buffer, converting it to the UTF-8 on the fly
    ///@param src pointer to the source buffer
    ///@param nchars number of characters in the source buffer, -1 if zero terminated
    ///@return number of bytes appended
    uint append_wchar_buf( const wchar_t* src, uints nchars )
    {
        uints nold = lens();
        _tstr.set_size(nold);

        for( ; *src!=0 && nchars>0; ++src,--nchars )
        {
            if( *src <= 0x7f )
                *_tstr.add() = (char)*src;
            else
            {
                uints old = _tstr.size();
                char* p = _tstr.add(6);
                uints n = write_utf8_seq( *src, p );
                _tstr.set_size( old + n );
            }
        }
        if( _tstr.size() )
            *_tstr.add() = 0;

        return uint(lens() - nold);
    }

#ifdef SYSTYPE_WIN
    ///Append wchar (UCS-2) buffer, converting it to the ANSI on the fly
    ///@param src pointer to the source buffer
    ///@param nchars number of characters in the source buffer, -1 if zero-terminated
    uint append_wchar_buf_ansi( const wchar_t* src, uints nchars );
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
		DATE_ISO8601        = 0x80,
		DATE_ISO8601_GMT	= 0x100,
    };

    ///Append GMT date string constructed by the flags set
    charstr& append_date_gmt( const timet t, uint flg=UMAX32 )
    {
#ifdef SYSTYPE_MSVC8plus
        struct tm tm;
        gmtime_s( &tm, &t.t );
#else
	time_t tv = (time_t)t.t;
        struct tm const& tm = *gmtime(&tv);
#endif
        return append_time( tm, flg, "GMT" );
    }

    charstr& append_date_local( const timet t, uint flg=UMAX32 )
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
	time_t tv = (time_t)t.t;
        struct tm const& tm = *localtime(&tv);
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

		if( flg & DATE_ISO8601 ) {  // 2008-02-22T15:08:13Z
			append_num(10, tm.tm_year+1900);
			append('-');
			append_num(10, tm.tm_mon+1);
			append('-');
			append_num(10, tm.tm_mday);
			append('T');
		}

        if( flg & (DATE_HHMM|DATE_HHMMSS|DATE_ISO8601) ) {
            append_num(10,tm.tm_hour,2,ALIGN_NUM_RIGHT_FILL_ZEROS);
            append(':');
            append_num(10,tm.tm_min,2,ALIGN_NUM_RIGHT_FILL_ZEROS);
        }

        if( flg & (DATE_HHMMSS|DATE_ISO8601) ) {
            append(':');
            append_num(10,tm.tm_sec,2,ALIGN_NUM_RIGHT_FILL_ZEROS);
        }

        if( flg & DATE_TZ ) {
			append(' ');
			append(tz);
        }

		if( flg & DATE_ISO8601 ) {
			if( flg & DATE_ISO8601_GMT ) {
				append('Z');
			} else {
				int t;
#ifdef SYSTYPE_MSVC8plus
                long tz;
				_get_timezone(&tz);
				t = -tz;
#else
				t = -__timezone;
#endif
				if (t > 0) append('+');
				append_num(10,t/3600,2,ALIGN_NUM_RIGHT_FILL_ZEROS);
				append(':');
				append_num(10,(t%3600)/60,2,ALIGN_NUM_RIGHT_FILL_ZEROS);
			}
		}

        return *this;
    }

	charstr& append_time_xsd( const timet & t)
	{
		return append_date_local(t, DATE_ISO8601);
	}

	charstr& append_time_xsd_gmt( const timet & t)
	{
		return append_date_local(t, DATE_ISO8601|DATE_ISO8601_GMT);
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


    ///Append binary data converted to escaped hexadecimal string
    //@param src source memory buffer
    //@param dst destination character buffer capable to hold at least (((itemsize*2) + sep?1:0) * nitems) bytes
    //@param nitems number of itemsize sized words to convert
    //@param itemsize number of bytes to write clumped together after prefix
    //@param prefix before each item
    charstr& append_prehex( const void* src, uints nitems, uint itemsize, const token& prefix )
    {
        if( nitems == 0 )
            return *this;

        static char tbl[] = "0123456789ABCDEF";
        for( uints i=0;; )
        {
            append(prefix);
            char* pdst = get_append_buf(itemsize * 2);

            if(sysIsLittleEndian)
            {
                for( uint j=itemsize; j>0; )
                {
                    --j;
                    pdst[0] = tbl[((uchar*)src)[j] >> 4];
                    pdst[1] = tbl[((uchar*)src)[j] & 0x0f];
                    pdst += 2;
                }
            }
            else
            {
                for( uint j=0; j<itemsize; ++j )
                {
                    pdst[0] = tbl[((uchar*)src)[j] >> 4];
                    pdst[1] = tbl[((uchar*)src)[j] & 0x0f];
                    pdst += 2;
                }
            }

            src = (uchar*)src + itemsize;

            ++i;
            if( i>=nitems )  break;
        }

        return *this;
    }

    ///Append string from binstream (without resetting previous content)
    binstream& append_from_stream( binstream& bin );


    ///Clamp character positions to actual string range. Negative values index from end and are converted. Zero in \a to means an end of the string position
    void clamp_range( ints& from, ints& to ) const
    {
        ints size = len();
        if(from<0)  from = size - from;
        if(to<=0)   to   = size - to;

        if(from >= size) from = size;
        else if(from < 0) from = 0;

        if(to >= size) to = size;
        else if(to < from) to = from;
    }

    ///Convert string to lowercase
    void tolower()
    {
        char* pe = (char*)ptre();
        for( char* p = (char*)ptr(); p<pe; ++p )
            *p = ::tolower(*p);
    }

    ///Convert range within string to lowercase
    void tolower( ints from, ints to )
    {
        clamp_range(from, to);

        char* pe = (char*)ptr() + to;
        for( char* p = (char*)ptr()+from; p<pe; ++p )
            *p = ::tolower(*p);
    }

    ///Convert string to uppercase
    void toupper()
    {
        char* pe = (char*)ptre();
        for( char* p = (char*)ptr(); p<pe; ++p )
            *p = ::toupper(*p);
    }

    ///Convert range within string to uppercase
    void toupper( ints from, ints to )
    {
        clamp_range(from, to);

        char* pe = (char*)ptr() + to;
        for( char* p = (char*)ptr()+from; p<pe; ++p )
            *p = ::toupper(*p);
    }

    ///Replace every occurence of character \a from to character \a to
    void replace( char from, char to )
    {
        char* pe = (char*)ptre();
        for( char* p = (char*)ptr(); p<pe; ++p )
            if( *p == from )  *p = to;
    }


    ///Insert character at position
    bool ins( uints pos, char c )
    {
        if( pos > lens() )  return false;
        *_tstr.ins(pos) = c;
        return true;
    }

    ///Insert substring at position
    bool ins( uints pos, const token& t )
    {
        uints len = t.len();
        if( pos > len )  return false;
        xmemcpy( _tstr.ins(pos,len), t.ptr(), len );
        return true;
    }

    ///Delete character(s) at position
    bool del( uints pos, uints n=1 )
    {
        if( pos+n > lens() )  return false;
        _tstr.del(pos, n);
        return true;
    }

    ///Return position where the substring is located
    ///@return substring position, len() if not found
    uint contains( const substring& sub, uints off=0 ) const
    {   return token(*this).count_until_substring(sub,off); }

    ///Return position where the character is located
    ///@return substring position, len() if not found
    uint contains( char c, uints off=0 ) const
    {   return token(*this).count_notchar(c,off); }

    ///Return position where the character is located, searching from the end
    ///@return substring position, len() if not found
    uint contains_back( char c, uints off=0 ) const
    {
        uint n = token(*this).count_notchar_back(c,off);
        return ( n > off )
            ? n-1
            : len();
    }


    char& operator [] (uints i)                 { return _tstr[i]; }


    bool operator == (const charstr& str) const
    {
        if( ptr() == str.ptr() )
            return 1;           //only when 0==0
        if( len() != str.len() )
            return 0;
        return 0 == memcmp( _tstr.ptr(), str._tstr.ptr(), len() );
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
        if (tok.len() != len())  return false;
        return strncmp(_tstr.ptr(), tok.ptr(), tok.len()) == 0;
    }

    bool operator != (const token& tok) const {
        if (tok.len() != len())  return true;
        return strncmp(_tstr.ptr(), tok.ptr(), tok.len()) != 0;
    }

    bool operator == (const char c) const {
        if (len() != 1)  return false;
        return _tstr[0] == c;
    }
    bool operator != (const char c) const {
        if (len() != 1)  return true;
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
        if( is_empty() )  return 0;
        if( tok.is_empty() )  return 1;

        int k = strncmp( _tstr.ptr(), tok.ptr(), tok.len() );
        if( k == 0 )
            return len() > tok.len();
        return k > 0;
    }

    bool operator < (const token& tok) const
    {
        if( tok.is_empty() )  return 0;
        if( is_empty() )  return 1;

        int k = strncmp( _tstr.ptr(), tok.ptr(), tok.len() );
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
    char* get_buf( uints len )          { return alloc_buf(len); }
    char* get_append_buf( uints len )   { return alloc_append_buf(len); }

    ///correct the size of the string if a terminating zero is found inside
    charstr& correct_size()
    {
        uints l = lens();
        if(l>0)
        {
            uints n = ::strlen(_tstr.ptr());
            if(n<l)
                resize(n);
        }
        return *this;
    }

    char *copy32(char *p) const {
        uints len = _tstr.size();
        xmemcpy( p, _tstr.ptr(), len );
        return p + len;
    }

    //@return true if the string is empty
    bool is_empty() const {
        return _tstr.size () <= 1;
    }

    typedef dynarray<char,uint> charstr::*unspecified_bool_type;

    ///Automatic cast to bool for checking emptiness
	operator unspecified_bool_type () const {
	    return len() == 0  ?  0  :  &charstr::_tstr;
	}



    dynarray<char,uint>& dynarray_ref()      { return _tstr; }

    //@return char* to the string beginning
    const char* ptr() const             { return _tstr.ptr(); }

    //@return char* to past the string end
    const char* ptre() const            { return _tstr.ptr() + len(); }

    //@return zero-terminated C-string
    const char* c_str() const           { return _tstr.size() ? _tstr.ptr() : ""; }

    ///String length excluding terminating zero
    uint len() const                    { return _tstr.size() ? (_tstr.size() - 1) : 0; }
    uints lens() const                  { return _tstr.sizes() ? (_tstr.sizes() - 1) : 0; }

    ///String length counting terminating zero
    uints lent() const                  { return _tstr.size(); }

    ///Set string to empty and discard the memory
    void free()                         { _tstr.discard(); }

    ///Reserve memory for string
    char* reserve( uints len )          { return _tstr.reserve(len, true); }

    ///Reset string to empty but keep the memory
    void reset() {
        if(_tstr.size())
            _tstr[0] = 0;
        _tstr.reset();
    }

    ~charstr() {}


protected:

    void assign( const char *czstr, uints len )
    {
        if( len == 0 ) {
            reset();
            return;
        }

        DASSERT( len <= UMAX32 );

        _tstr.alloc(len);
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

        DASSERT( len <= UMAX32 );

        char* p = _tstr.alloc(len+1);
        termzero();

        return p;
    }

    char* alloc_append_buf( uints len )
    {
        if(!len) return 0;
        return uniadd(len);
    }

    void termzero()
    {
        uints len = _tstr.sizes();
        _tstr[len-1] = 0;
    }


    dynarray<char,uint> _tstr;

public:


    bool begins_with( const char* str ) const
    {
        token me = *this;
        return me.begins_with(str);
    }

    bool begins_with( const token& tok ) const
    {
        token me = *this;
        return me.begins_with(tok);
    }

    bool begins_with_icase( const char* str ) const
    {
        token me = *this;
        return me.begins_with_icase(str);
    }

    bool begins_with_icase( const token& tok ) const
    {
        token me = *this;
        return me.begins_with_icase(tok);
    }

    bool ends_with( const token& tok ) const
    {
        token me = *this;
        return me.ends_with(tok);
    }

    bool ends_with_icase( const token& tok ) const
    {
        token me = *this;
        return me.ends_with_icase(tok);
    }



    uints touint( uints offs=0 ) const      { return token(ptr()+offs, ptre()).touint(); }
    ints toint( uints offs=0 ) const        { return token(ptr()+offs, ptre()).toint(); }

    uints xtouint( uints offs=0 ) const     { return token(ptr()+offs, ptre()).xtouint(); }
    ints xtoint( uints offs=0 ) const       { return token(ptr()+offs, ptre()).xtoint(); }

    double todouble( uints offs=0 ) const   { return token(ptr()+offs, ptre()).todouble(); }

    ////////////////////////////////////////////////////////////////////////////////
    bool cmpeq( const token& str ) const
    {
        if( len() != str.len() )
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
        if( len() != str.len() )
            return 0;
        return 0 == xstrncasecmp( _tstr.ptr(), str.ptr(), str.len() );
    }
    bool cmpeqi( const char* str ) const
    {
        if( _tstr.size() == 0 )  return str == 0  ||  str[0] == 0;
        return 0 == xstrcasecmp( _tstr.ptr(), str );
    }

    bool cmpeqc( const token& str, bool casecmp ) const      { return casecmp ? cmpeq(str) : cmpeqi(str); }
    bool cmpeqc( const char* str, bool casecmp ) const       { return casecmp ? cmpeq(str) : cmpeqi(str); }

    ////////////////////////////////////////////////////////////////////////////////
    ///Compare strings
    //@return -1 if str<this, 0 if str==this, 1 if str>this
    int cmp( const token& str ) const
    {
        uints let = lens();
        uints lex = str.len();
        int r = memcmp( _tstr.ptr(), str.ptr(), uint_min(let,lex) );
        if( r == 0 )
        {
            if( let<lex )  return -1;
            if( lex<let )  return 1;
        }
        return r;
    }
    int cmp( const char* str ) const        { return strcmp( _tstr.ptr(), str ); }

    ///Compare strings, longer first
    int cmplf( const token& str ) const
    {
        uints let = lens();
        uints lex = str.len();
        int r = memcmp( _tstr.ptr(), str.ptr(), uint_min(let,lex) );
        if( r == 0 )
        {
            if( let<lex )  return 1;
            if( lex<let )  return -1;
        }
        return r;
    }

    int cmpi( const token& str ) const
    {
        uints let = lens();
        uints lex = str.len();
        int r = xstrncasecmp( _tstr.ptr(), str.ptr(), uint_min(let,lex) );
        if( r == 0 )
        {
            if( let<lex )  return -1;
            if( lex<let )  return 1;
        }
        return r;
    }
    int cmpi( const char* str ) const       { return xstrcasecmp( _tstr.ptr(), str ); }

    int cmpc( const token& str, bool casecmp ) const      { return casecmp ? cmp(str) : cmpi(str); }
    int cmpc( const char* str, bool casecmp ) const       { return casecmp ? cmp(str) : cmpi(str); }

    char char_is_alpha( int n ) const
    {
        char c = nth_char(n);
        return (c >= 'a' && c <= 'z') || (c >='A' && c <= 'Z') ? c : 0;
    }

    char char_is_number( int n ) const
    {
        char c = nth_char(n);
        return (c >= '0' && c <= '9') ? c : 0;
    }

    char char_is_alphanum( int n ) const
    {
        char c = nth_char(n);
        return (c >= 'a' && c <= 'z') || (c >='A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : 0;
    }

    // extract from token
    charstr& set_left( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_left( c, make_set_trait(def_empty) );
    }

    charstr& set_left( token str, const token& separators, bool def_empty=false )
    {
        return *this = str.cut_left( separators, make_set_trait(def_empty) );
    }

    charstr& set_left_back( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_left_back( c, make_set_trait(def_empty) );
    }

    charstr& set_left_back( token str, const token& separators, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_left_back( separators, make_set_trait(def_empty) );
    }

    //
    charstr& set_right( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_right( c, make_set_trait(def_empty) );
    }

    charstr& set_right( token str, const token& separators, bool def_empty=false )
    {
        return *this = str.cut_right( separators, make_set_trait(def_empty) );
    }

    charstr& set_right_back( token str, const char c, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_right_back( c, make_set_trait(def_empty) );
    }

    charstr& set_right_back( token str, const token& separators, bool def_empty=false )     //up to, but without the character c
    {
        return *this = str.cut_right_back( separators, make_set_trait(def_empty) );
    }


    //extract token from string
    token select_left( const char c, bool def_empty=false ) const    //up to, but without the character c
    {
        token r = *this;
        return r.cut_left( c, make_set_trait(def_empty) );
    }

    token select_left( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_left( separators, make_set_trait(def_empty) );
    }

    token select_left_back( const char c, bool def_empty=false ) const     //up to, but without the character c
    {
        token r = *this;
        return r.cut_left_back( c, make_set_trait(def_empty) );
    }

    token select_left_back( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_left_back( separators, make_set_trait(def_empty) );
    }

    token select_right( const char c, bool def_empty=false ) const     //up to, but without the character c
    {
        token r = *this;
        return r.cut_right( c, make_set_trait(def_empty) );
    }

    token select_right( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_right( separators, make_set_trait(def_empty) );
    }

    token select_right_back( const char c, bool def_empty=false ) const     //up to, but without the character c
    {
        token r = *this;
        return r.cut_right_back( c, make_set_trait(def_empty) );
    }

    token select_right_back( const token& separators, bool def_empty=false ) const
    {
        token r = *this;
        return r.cut_right_back( separators, make_set_trait(def_empty) );
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

    charstr operator + (char c) const
    {
        charstr res = *this;
        res.append(c);
        return res;
    }

protected:

    ///Add n uninitialized characters, plus one character for the terminating zero if it's not there already 
    char* uniadd( uints n )
    {
        uints cn = _tstr.sizes();
        DASSERT( cn+n <= UMAX32 );

        char* p = (cn == 0)
            ? _tstr.add(n+1)
            : _tstr.add(n)-1;
        p[n] = 0;

        return p;
    }

    token::cut_trait make_set_trait( bool def_empty ) const {
        uint flags = def_empty  ?  token::fON_FAIL_RETURN_EMPTY  :  0;
        return token::cut_trait(flags);
    }
};


////////////////////////////////////////////////////////////////////////////////
inline token::token( const charstr& str )       { set(str); }

inline token& token::operator = ( const charstr& t )
{
    _ptr = t.ptr();
    _pte = t.ptre();
    return *this;
}

inline bool operator == (const token& tok, const charstr& str )
{
    if( tok.len() != str.len() ) return 0;
    return memcmp( tok.ptr(), str.ptr(), tok.len()) == 0;
}

inline bool operator != (const token& tok, const charstr& str )
{
    if( tok.len() != str.len() ) return 1;
    return memcmp( tok.ptr(), str.ptr(), tok.len()) != 0;
}

inline const char* token::set( const charstr& str )
{
    _ptr = str.ptr();
    _pte = str.ptre();
    return _pte;
}
/*
inline void token::assign_empty( const charstr& str )
{
    _pte = _ptr = str.ptr();
}
*/
template<class A>
inline bool token::utf8_to_wchar_buf( dynarray<wchar_t,uint,A>& dst ) const
{
    dst.reset();
    uints n = lens();
    const char* p = ptr();

    while(n>0)
    {
        if( (uchar)*p <= 0x7f ) {
            *dst.add() = *p++;
            --n;
        }
        else
        {
            uints ne = get_utf8_seq_expected_bytes(p);
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
inline binstream& operator >> (binstream &bin, charstr& x)
{
    x._tstr.reset();
    dynarray<char,uint>::dynarray_binstream_container c(x._tstr);

    bin.xread_array(c);
    if( x._tstr.size() )
        *x._tstr.add() = 0;
    return bin;
}

inline binstream& charstr::append_from_stream( binstream& bin )
{
    dynarray<char,uint>::dynarray_binstream_container c(_tstr);

    bin.xread_array(c);
    if( _tstr.size() )
        *_tstr.add() = 0;
    return bin;
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
    dynarray<key,uint>::dynarray_binstream_container
        c(reinterpret_cast<dynarray<key,uint>&>(x.dynarray_ref()));

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
    dynarray<bstype::key,uint>::dynarray_binstream_container
        c(reinterpret_cast<dynarray<bstype::key,uint>&>(x.dynarray_ref()));

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
    typedef charstr key_type;

    size_t operator() (const charstr& str) const
    {
        return __coid_hash_string( str.ptr(), str.len() );
    }
};

template<> struct hash<token>
{
    typedef token key_type;

    size_t operator() (const token& tok) const
    {
        return __coid_hash_string( tok.ptr(), tok.len() );
    }
};


////////////////////////////////////////////////////////////////////////////////
inline charstr& opcd_formatter::text( charstr& dst ) const
{
    dst << e.error_desc();

    if( e.text() && e.text()[0] )
        dst << " : " << e.text();
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
            _ctok._pte = _ctok._ptr = _cmd._ptr;
        }

        if( i == _tokn )
            return _ctok;

        for( ; _tokn < i; ++_tokn )
        {
            _ctok = _rtok.cut_left( token::TK_whitespace(), token::cut_trait(token::fREMOVE_ALL_SEPARATORS) );
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
