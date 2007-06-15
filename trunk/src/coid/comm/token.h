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


#ifndef __COID_COMM_TOKEN__HEADER_FILE__
#define __COID_COMM_TOKEN__HEADER_FILE__

#include "namespace.h"

#include "tutf8.h"
#include "commtime.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

COID_NAMESPACE_BEGIN

#ifdef SYSTYPE_MSVC8plus
# define strncasecmp     _strnicmp
# define strcasecmp      _stricmp
#elif defined SYSTYPE_MSVC
# define strncasecmp     strnicmp
# define strcasecmp      stricmp
#endif


///Copy max n characters from src to dst, append 0 only if src length < n
inline void xstrncpy( char* dst, const char* src, uints n )
{
    for( ; n>0; --n )
    {
        *dst = *src;
        if( 0 == *src )  break;
        ++dst;
        ++src;
    }
}


class charstr;
struct token;
template<class T, class A> class dynarray;

////////////////////////////////////////////////////////////////////////////////
///Preprocessed substring for fast substring searches
class substring
{
    mutable uchar _shf[256];
    const uchar* _subs;
    uints _len;
    uchar _tmpbuf[4];
    mutable int   _state;
public:

    substring()
    {
        _subs = 0;
        _len = 0;
        _state = 0;
    }

    substring( const char* subs, uints len )    { set(subs,len); }

    substring( const token& tok )               { set(tok); }
    explicit substring( char k )                { set(k); }


    substring& operator = ( const token& tok )  { set(tok);  return *this; }


    static substring& crlf()
    {
        static substring _S("\r\n",2);
        return _S;
    }

    static substring& null()
    {
        static substring _S("",1);
        return _S;
    }


    void set( const char* subs, uints len )
    {
        DASSERT( len < 255 );
        _subs = (const uchar*)subs;
        _len = len;
        memset( _shf, (uchar)len+1, 256 );
        for( uints i=0; i<len; ++i )  _shf[ _subs[i] ] = (uchar)(len-i);
    }

    void set( char k )
    {
        _tmpbuf[0] = k;
        _subs = _tmpbuf;
        _len = 1;
    }

    void set( const token& tok );


    uints len() const               { return _len; }
    const uchar* ptr() const        { return _subs; }

    token get() const;

    ///
    uints get_shift( uchar k ) const
    {
        if(!_state) {
            memset( _shf, (uchar)_len+1, 256 );
            for( uints i=0; i<_len; ++i )  _shf[ _subs[i] ] = (uchar)(_len-i);
            _state = 1;
        }
        return _shf[k];
    }



    ///Compare with string and return length of matching byte sequence
    uints memcmplen( const char* a ) const
    {
        uints off=0;
        for( ; off<_len && a[off] == _subs[off]; ++off );
        return off;
    }

};

////////////////////////////////////////////////////////////////////////////////
///Token structure describing a part of a string.
struct token
{
    const char *_ptr;           ///<ptr to the beginning of current string part
    uints _len;                  ///<length of current string part

    token() {}

    token( const char *czstr )
    {   set(czstr); }

    token( const charstr& str );

    token( const char* ptr, uints len )
    {
        _ptr = ptr;
        _len = len;
    }

    static token from_cstring( const char* czstr, uints maxlen = UMAX )
    {
        return token( czstr, strnlen(czstr,maxlen) );
    }

    token ( const token& src, uint offs, uint len )
    {
        if( offs > src._len )
            _ptr = src._ptr+src._len, _len = 0;
        else if( len > src._len - offs )
            _ptr = src._ptr+offs, _len = src._len - offs;
        else
            _ptr = src._ptr+offs, _len = len;
    }


    void swap( token& t )
    {
        const char* p = t._ptr; t._ptr = _ptr;  _ptr = p;
        uints n = t._len;       t._len = _len;  _len = n;
    }

    const char* ptr() const                 { return _ptr; }
    const char* ptre() const                { return _ptr+_len; }
    uints len() const                       { return _len; }

    void truncate( ints n )
    {
        if( n < 0 )
        {
            if( (uints)-n > _len )  _len=0;
            else  _len += n;
        }
        else if( (uints)n < _len )
            _len = n;
    }


    static token empty()                    { static token _tk(0,0);  return _tk; }

    static const token& TK_whitespace()     { static token _tk(" \t\n\r");  return _tk; }
    static const token& TK_newlines()       { static token _tk("\n\r");  return _tk; }


    char operator[] (ints i) const {
        return _ptr[i];
    }

    bool operator == (const token& tok) const
    {
        if( _len != tok._len ) return 0;
        return ::memcmp( _ptr, tok._ptr, _len ) == 0;
    }

    bool operator == (const char* str) const {
        uints i;
        for( i=0; i<_len; ++i ) {
            if( str[i] != _ptr[i] )  return false;
        }
        return str[i] == 0;
    }

    friend bool operator == (const token& tok, const charstr& str );

    bool operator == (char c) const {
        if( 1 != _len ) return 0;
        return c == *_ptr;
    }

    bool operator != (const token& tok) const {
        if( _len != tok._len ) return true;
        return ::memcmp( _ptr, tok._ptr, _len ) != 0;
    }

    bool operator != (const char* str) const {
        uints i;
        for( i=0; i<_len; ++i ) {
            if( str[i] != _ptr[i] )  return true;
        }
        return str[i] != 0;
    }

    friend bool operator != (const token& tok, const charstr& str );

    bool operator != (char c) const {
        if (1 != _len) return true;
        return c != *_ptr;
    }

    bool operator > (const token& tok) const
    {
        if( !_len )  return 0;
        if( !tok._len )  return 1;

        uints m = uint_min( _len, tok._len );

        int k = ::strncmp( _ptr, tok._ptr, m );
        if( k == 0 )
            return _len > tok._len;
        return k > 0;
    }

    bool operator < (const token& tok) const
    {
        if( !tok._len )  return 0;
        if( !_len )  return 1;

        uints m = uint_min( _len, tok._len );

        int k = ::strncmp( _ptr, tok._ptr, m );
        if( k == 0 )
            return _len < tok._len;
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
    bool cmpeq( const token& str ) const
    {
        if( len() != str.len() )
            return 0;
        return 0 == memcmp( ptr(), str.ptr(), len() );
    }

    bool cmpeqi( const token& str ) const
    {
        if( len() != str.len() )
            return 0;
        return 0 == strncasecmp( ptr(), str.ptr(), len() );
    }

    bool cmpeqc( const token& str, bool casecmp ) const
    {
        return casecmp ? cmpeq(str) : cmpeqi(str);
    }

    ////////////////////////////////////////////////////////////////////////////////
    int cmp( const token& str ) const
    {
        uints lex = str.len();
        int r = memcmp( ptr(), str.ptr(), uint_min(_len,lex) );
        if( r == 0 )
        {
            if( _len<lex )  return -1;
            if( lex<_len )  return 1;
        }
        return r;
    }

    int cmpi( const token& str ) const
    {
        uints lex = str.len();
        int r = strncasecmp( ptr(), str.ptr(), uint_min(_len,lex) );
        if( r == 0 )
        {
            if( _len<lex )  return -1;
            if( lex<_len )  return 1;
        }
        return r;
    }

    int cmpc( const token& str, bool casecmp ) const
    {
        return casecmp ? cmp(str) : cmpi(str);
    }



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


    ////////////////////////////////////////////////////////////////////////////////
    ///Eat the first character from token, returning it
    char operator ++ ()
    {
        if(_len)
        {
            --_len;
            ++_ptr;
            return _ptr[-1];
        }
        return 0;
    }

    ///Extend token to include one more character to the right
    token& operator ++ (int)
    {
        ++_len;
        return *this;
    }

    ///Extend token to include one more character to the left
    token& operator -- ()
    {
        ++_len;
        --_ptr;
        return *this;
    }

    ///Eat the last character from token, returning it
    char operator -- (int)
    {
        if(_len) {
            --_len;
            return _ptr[_len];
        }
        return 0;
    }

    ///Positive value moves the pointer forward, a negative one cuts from end
    token& operator += (ints i)
    {
        if(i>0)
        {
            if( i>(ints)_len )  i = _len;
            _ptr += i;
            _len -= i;
        }
        else
        {
            if( -i>(ints)_len )  i = -(ints)_len;
            _len += i;
        }
        return *this;
    }

    ///Positive value moves the pointer backward, a negative one adds to end
    token& operator -= (ints i)
    {
        if(i>0)
        {
            _ptr -= i;
            _len += i;
        }
        else
        {
            _len -= i;
        }
        return *this;
    }
/*
    const char* operator + (ints i)
    {
        return _ptr + i;
    }
*/
    bool is_empty() const               { return _len == 0; }
    bool is_null() const                { return _ptr == 0; }
    void set_empty()                    { _len = 0; }
    void set_null()                     { _ptr = 0; _len = 0; }

    char last_char() const              { return _len > 0  ?  _ptr[_len-1]  :  0; }
    char first_char() const             { return _len > 0  ?  _ptr[0]  :  0; }
    char nth_char( ints n ) const       { return n<0 ? ((uints)-n<=_len  ?  _ptr[_len+n] : 0) : ((uints)n<_len  ?  _ptr[n] : 0); }


    token& operator = ( const char *czstr ) {
        _ptr = czstr;
        _len = czstr ? ::strlen(czstr) : 0;
        return *this;
    }

    token& operator = ( const token& t )
    {
        _ptr = t._ptr;
        _len = t._len;
        return *this;
    }

    token& operator = ( const charstr& t );

    ///Assigns string to token, initially setting up the token as empty, allowing for subsequent calls to token() method to retrieve next token.
    void assign( const char *czstr ) {
        _ptr = czstr;
        _len = 0;
    }

    ///Assigns string to token, initially setting up the token as empty, allowing for subsequent calls to token() method to retrieve next token.
    void assign( const token& str ) {
        _ptr = str.ptr();
        _len = 0;
    }

    ///Assigns string to token, initially setting up the token as empty, allowing for subsequent calls to token() method to retrieve next token.
    void assign( const charstr& str );

    const char* set( const char* czstr ) {
        _ptr = czstr;
        _len = czstr ? ::strlen(czstr) : 0;
        return _ptr+_len;
    }

    const char* set( const char* czstr, uints len )
    {
        _ptr = czstr;
        _len = len;
        return _ptr+_len;
    }

    const char* set( const charstr& str );


    char* copy_to( char* str, uints maxbufsize ) const
    {
        if( maxbufsize == 0 )  return str;

        uints lt = _len;
        if( lt >= maxbufsize )
            lt = maxbufsize - 1;
        xmemcpy( str, _ptr, lt );
        str[lt] = 0;

        return str;
    }

    ///Retrieve UCS-4 code from UTF-8 encoded sequence at offset \a off
    ucs4 get_utf8( uints& off ) const
    {
        if( off >= _len )  return 0;
        if( _len - off < get_utf8_seq_expected_bytes(_ptr+off) )  return UMAX;
        return read_utf8_seq( _ptr, off );
    }

    ///Cut UTF-8 sequence from the token, returning its UCS-4 code
    ucs4 cut_utf8()
    {
        if( _len == 0 )  return 0;
        uints off = get_utf8_seq_expected_bytes(_ptr);
        if( _len < off ) {
            //malformed UTF-8 character, but truncate it to make progress
            _ptr += _len;
            _len = 0;
            return UMAX;
        }
        ucs4 ch = read_utf8_seq(_ptr);
        _ptr += off;
        _len -= off;
        return ch;
    }


    token cut_left_n( uints n )
    {
        if( n > _len )
            n = _len;

        token r;
        r._ptr = _ptr;
        r._len = n;

        _ptr += n;
        _len -= n;
        return r;
    }
    
    token cut_right_n( uints n )
    {
        if( n > _len )
            n = _len;

        _len -= n;

        token r;
        r._ptr = _ptr + _len;
        r._len = n;
        return r;
    }
    


    ///Cut a token off, using single character as the delimiter
    token cut_left( char c, int skipsep, bool def_empty=false )     //up to, but without the character c
    {
        token r;
        const char* p = strchr(c);
        if(p) {
            r._ptr = _ptr;
            r._len = uints(p-_ptr);
            uints n = skipsep ? 1 : 0;
            n += r._len;
            _ptr += n;
            _len -= n;
        }
        else if (!def_empty) {
            r._ptr = _ptr;
            r._len = _len;
            _ptr += r._len;
            _len = 0;
        }
        else {
            r._ptr = _ptr;
            r._len = 0;
        }
        return r;
    }

    ///Cut a token off, using group of single characters as delimiters
    token cut_left( const token& separators, int skipsep, bool def_empty=false )
    {
        token r;
        uints ln = count_notingroup(separators);
        if( _len > ln )         //if not all is not separator
        {
            r._ptr = _ptr;
            r._len = ln;
            if(skipsep>0)       ++ln;
            else if(skipsep<0)  ln = count_ingroup(separators,ln);
            _ptr += ln;
            _len -= ln;
        }
        else if( !def_empty )   //_len==ln
        {
            r._ptr = _ptr;
            r._len = ln;
            _ptr += ln;
            _len = 0;
        }
        else
        {
            r._ptr = _ptr;
            r._len = 0;
        }

        return r;
    }

    ///Cut left substring
    token cut_left( const substring& ss, int skipsep, bool def_empty=false )
    {
        token r;
        uints n = count_until_substring(ss);
        if( n < _len )
        {
            r._ptr = _ptr;
            r._len = n;
            if(skipsep)  n += ss.len();
            _ptr += n;
            _len -= n;
        }
        else if( !def_empty )   //_len==ln
        {
            r._ptr = _ptr;
            r._len = n;
            _ptr += n;
            _len = 0;
        }
        else
        {
            r._ptr = _ptr;
            r._len = 0;
        }

        return r;
    }


    ///Cut left substring, searching for separator backwards
    token cut_left_back( const char c, int skipsep, bool def_empty=true )     //up to, but without the character c
    {
        token r;
        const char* p = strrchr(c);
        if(p)
        {
            r._ptr = _ptr;
            r._len = uints(p-_ptr);
            uints n = skipsep ? 1 : 0;
            _ptr = p+n;
            _len -= r._len+n;
        }
        else if( !def_empty ) {
            r._ptr = _ptr;
            r._len = _len;
            _ptr += _len;
            _len = 0;
        }
        else {
            r._ptr = _ptr;
            r._len = 0;
        }
        return r;
    }

    ///Cut left substring, searching for separator backwards
    token cut_left_back( const token& separators, int skipsep, bool def_empty=false)
    {
        token r;
        uints off = count_notingroup_back(separators);

        if( off > 0 )
        {
            uints n;
            if(skipsep>0)       n = off-1;
            else if(skipsep<0)  n = count_ingroup_back(separators,off);
            else                n = off;
            r._ptr = _ptr;
            r._len = n;
            _ptr = _ptr + off;
            _len -= off;
        }
        else if( !def_empty )
        {
            r._ptr = _ptr;
            r._len = _len;
            _ptr += _len;
            _len = 0;
        }
        else
        {
            r._ptr = _ptr;
            r._len = 0;
        }

        return r;
    }

    ///Cut left substring, searching for separator backwards
    token cut_left_back( const substring& ss, int skipsep, bool def_empty=false)
    {
        token r;
        uints off=0;
        uints lastss = _len;    //position of the last substring found

        for(;;)
        {
            uints us = count_until_substring( ss, off );
            if( us >= _len )
                break;

            lastss = us;
            off = us + ss.len();
        }

        if( lastss < _len )
        {
            r._ptr = _ptr;
            r._len = _len - lastss;
            if(skipsep)
                lastss += ss.len();
            _ptr += lastss;
            _len -= lastss;
        }
        else if(!def_empty)
        {
            r._ptr = _ptr;
            r._len = _len;
            _ptr += _len;
            _len = 0;
        }
        else
        {
            r._ptr = _ptr;
            r._len = 0;
        }
        return r;
    }

    ///Cut right substring
    token cut_right( const char c, int skipsep, bool def_empty=false )       //up to, but without the character c
    {
        token r = cut_left( c, skipsep, !def_empty );
        swap(r);
        return r;
    }

    ///Cut right substring
    token cut_right( const token& separators, int skipsep, bool def_empty=false )
    {
        token r = cut_left( separators, skipsep, !def_empty );
        swap(r);
        return r;
    }

    ///Cut right substring
    token cut_right( const substring& ss, int skipsep, bool def_empty=false )
    {
        token r = cut_left( ss, skipsep, !def_empty );
        swap(r);
        return r;
    }

    ///Cut right substring, searching for separator backwards
    token cut_right_back( const char c, int skipsep, bool def_empty=false )       //up to, but without the character c
    {
        token r = cut_left_back( c, skipsep, !def_empty );
        swap(r);
        return r;
    }

    ///Cut right substring, searching for separator backwards
    token cut_right_back( const token& separators, int skipsep, bool def_empty=false )
    {
        token r = cut_left_back( separators, skipsep, !def_empty );
        swap(r);
        return r;
    }

    ///Cut right substring, searching for separator backwards
    token cut_right_back( const substring& ss, int skipsep, bool def_empty=false )
    {
        token r = cut_left_back( ss, skipsep, !def_empty );
        swap(r);
        return r;
    }





    uints count_notingroup( const token& sep, uints off=0 ) const
    {
        uints i;
        for (i=off; i<_len; ++i)
        {
            uints j;
            for( j=0; j<sep._len; ++j )
                if( _ptr[i] == sep._ptr[j] )
                    return i;
        }
        return i;
    }

    ///\a tbl is assumed to contain 128 entries for basic ascii.
    /// characters with codes above the 128 are considered to be utf8 stuff
    /// and \a utf8in tells whether they are considered to be 'in'
    uints count_notintable_utf8( const uchar* tbl, uchar msk, bool utf8in, uints off=0 ) const
    {
        uints i;
        for( i=off; i<_len; ++i )
        {
            uchar k = _ptr[i];

            if( k >= 128 )
            {
                if(utf8in)
                    break;
            }
            else if( (tbl[k] & msk) != 0 )
                break;
        }
        return i;
    }

    uints count_notintable( const uchar* tbl, uchar msk, uints off=0 ) const
    {
        uints i;
        for( i=off; i<_len; ++i )
        {
            uchar k = _ptr[i];
            if( (tbl[k] & msk) != 0 )
                break;
        }
        return i;
    }

    uints count_notinrange( char from, char to, uints off=0 ) const
    {
        uints i;
        for (i=off; i<_len; ++i)
        {
            if( _ptr[i] >= from  &&  _ptr[i] <= to )
                break;
        }
        return i;
    }

    uints count_notchar( char sep, uints off=0 ) const
    {
        uints i;
        for (i=off; i<_len; ++i)
        {
            if (_ptr[i] == sep)
                break;
        }
        return i;
    }

    uints count_ingroup( const token& sep, uints off=0 ) const
    {
        uints i;
        for( i=off; i<_len; ++i )
        {
            uints j;
            for( j=0; j<sep._len; ++j )
            {
                if( _ptr[i] == sep._ptr[j] )
                    break;
            }
            if( j>=sep._len )
                break;
        }
        return i;
    }

    ///\a tbl is assumed to contain 128 entries for basic ascii.
    /// characters with codes above the 128 are considered to be utf8 stuff
    /// and \a utf8in tells whether they are considered to be 'in'
    uints count_intable_utf8( const uchar* tbl, uchar msk, bool utf8in, uints off=0 ) const
    {
        uints i;
        for( i=off; i<_len; ++i )
        {
            uchar k = _ptr[i];

            if( k >= 128 )
            {
                if(!utf8in)
                    break;
            }
            else if( (tbl[k] & msk) == 0 )
                break;
        }
        return i;
    }

    uints count_intable( const uchar* tbl, uchar msk, uints off=0 ) const
    {
        uints i;
        for( i=off; i<_len; ++i )
        {
            uchar k = _ptr[i];
            if( (tbl[k] & msk) == 0 )
                break;
        }
        return i;
    }

    uints count_inrange( char from, char to, uints off=0 ) const
    {
        uints i;
        for( i=off; i<_len; ++i )
        {
            if( _ptr[i] < from  ||  _ptr[i] > to )
                break;
        }
        return i;
    }

    uints count_char( char sep, uints off=0 ) const
    {
        uints i;
        for (i=off; i<_len; ++i)
        {
            if (_ptr[i] != sep)
                break;
        }
        return i;
    }


    uints count_notingroup_back( const token& sep, uints off=0 ) const
    {
        uints i=_len;
        for( ; i>off; )
        {
            --i;

            uints j;
            for( j=0; j<sep._len; ++j )
                if( _ptr[i] == sep._ptr[j] )
                    return i+1;
        }
        return i;
    }

    uints count_notinrange_back( char from, char to, uints off=0 ) const
    {
        uints i=_len;
        for( ; i>off; )
        {
            --i;

            if( _ptr[i] >= from  &&  _ptr[i] <= to )
                return i+1;
        }
        return i;
    }

    uints count_notchar_back( char sep, uints off=0 ) const
    {
        uints i=_len;
        for( ; i>off; )
        {
            --i;

            if( _ptr[i] == sep )
                return i+i;
        }
        return i;
    }

    uints count_ingroup_back( const token& sep, uints off=0 ) const
    {
        uints i=_len;
        for( ; i>off; )
        {
            --i;

            uints j;
            for( j=0; j<sep._len; ++j )
                if( _ptr[i] == sep._ptr[j] )
                    break;

            if( j >= sep._len )
                return i+1;
        }
        return i;
    }

    uints count_inrange_back( char from, char to, uints off=0 ) const
    {
        uints i=_len;
        for( ; i>off; )
        {
            --i;

            if( _ptr[i] < from  ||  _ptr[i] > to )
                return i+1;
        }
    }

    uints count_char_back( char sep, uints off=0 ) const
    {
        uints i=_len;
        for( ; i>off; )
        {
            --i;

            if( _ptr[i] != sep )
                return i+1;
        }
        return i;
    }

    ///Count characters up to specified substring
    uints count_until_substring( const substring& sub, uints off=0 ) const
    {
        if( sub.len() == 1 )
            return count_notchar( sub.ptr()[0], off );

        uints slen = sub.len();
        uints rlen = _len - off;
        while(1)
        {
            if( rlen >= slen )
            {
                uints n = sub.memcmplen( _ptr+off );
                if( n == slen )
                    //substring found
                    return off;
            }
            
            if( slen >= rlen )
                return _len;       //end of input, substring cannot be there

            uchar o = _ptr[off+slen];

            // part to skip
            uints sk = sub.get_shift(o);      //sk can be (sub._len+1) max
            off += sk;
            rlen -= sk;
        }
    }


    ///Return position where the substring is located
    ///@return substring position, len() if not found
    uints contains( const substring& sub, uints off=0 ) const       { return count_until_substring(sub,off); }

    ///Return position where the character is located
    uints contains( char c, uints off=0 ) const                     { return count_notchar(c,off); }

    ///Return position where the character is located, searching from end
    uints contains_back( char c, uints off=0 ) const
    {
        uints n = count_notchar_back(c,off);
        return ( n > off )
            ? n-1
            : _len;
    }

    ///Returns number of newline sequences found, detects \r \n and \r\n
    uints count_newlines() const
    {
        uints n=0;
        char oc=0;
        const char* p = ptr();
        const char* pe = ptre();

        for( ; p<pe; ++p ) {
            char c = *p;
            if( c == '\r' ) ++n;
            else if( c == '\n' && oc != '\r' ) ++n;

            oc = c;
        }

        return n;
    }

    ///Trims trailing \r\n, \r or \n sequence
    token& trim_newline()
    {
        char c = last_char();
        if( c == '\n' ) {
            --_len;
            c = last_char();
        }
        if( c == '\n' )
            --_len;

        return *this;
    }

    ///Skips leading \r\n, \r or \n sequence
    token& skip_newline()
    {
        char c = first_char();
        if( c == '\r' ) {
            ++_ptr;
            --_len;
            c = first_char();
        }
        if( c == '\n' ) {
            ++_ptr;
            --_len;
        }
        return *this;
    }

    token& skip_ingroup( const token& sep, uints off=0 )
    {
        uints n = count_ingroup( sep, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_intable_utf8( const uchar* tbl, uchar msk, bool utf8in, uints off=0 )
    {
        uints n = count_intable_utf8( tbl, msk, utf8in, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_intable( const uchar* tbl, uchar msk, uints off=0 )
    {
        uints n = count_intable( tbl, msk, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_inrange( char from, char to, uints off=0 )
    {
        uints n = count_inrange( from, to, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_char( char sep, uints off=0 )
    {
        uints n = count_char( sep, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_notingroup( const token& sep, uints off=0 )
    {
        uints n = count_notingroup( sep, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_notintable_utf8( const uchar* tbl, uchar msk, bool utf8in, uints off=0 )
    {
        uints n = count_notintable_utf8( tbl, msk, utf8in, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_notintable( const uchar* tbl, uchar msk, uints off=0 )
    {
        uints n = count_notintable( tbl, msk, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_notinrange( char from, char to, uints off=0 )
    {
        uints n = count_notinrange( from, to, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_notchar( char sep, uints off=0 )
    {
        uints n = count_notchar( sep, off );
        _ptr += n;
        _len -= n;
        return *this;
    }

    token& skip_until_substring( const substring& ss, uints off=0 )
    {
        uints n = count_until_substring( ss, off );
        _ptr += n;
        _len -= n;
        return *this;
    }



    token get_line()
    {
        token r = cut_left( '\n', 1 );
        if( r.last_char() == '\r' )
            --r._len;
        return r;
    }


    token trim_to_null() const
    {
        token t = *this;
        t._len = strnlen( t._ptr, t._len );
        return t;
    }

    token& trim( bool newline=true, bool whitespace=true )
    {
        if( whitespace ) 
        {
            for( uints i=_len; i>0; --i )
            {
                if( *_ptr == ' '  ||  *_ptr == '\t' )
                    ++_ptr, --_len;
            }
        }

        for( uints i=_len; i>0; )
        {
            --i;
            if( newline  &&  (_ptr[i] == '\n'  ||  _ptr[i] == '\r') )
                _len = i;
            else if( whitespace  &&  (_ptr[i] == ' '  ||  _ptr[i] == '\t') )
                _len = i;
            else
                break;
        }
        return *this;
    }



    static uints strnlen( const char* str, uints n )
    {
        uints i;
        for( i=0; i<n; ++i )
            if( str[i] == 0 ) break;
        return i;
    }

    const char* strchr( char c, uints off=0 ) const
    {
        uints i;
        for( i=off; i<_len; ++i )
            if( _ptr[i] == c )  return _ptr+i;
        return 0;
    }

    const char* strrchr( char c, uints off=0 ) const
    {
        uints i;
        for( i=_len; i>off; )
        {
            --i;
            if( _ptr[i] == c )  return _ptr+i;
        }
        return 0;
    }


    bool begins_with( const char* str ) const
    {
        uints n = _len;
        for( uints i=0; i<n; ++i )
        {
            if( str[i] == 0 )
                return true;
            if( str[i] != _ptr[i] )
                return false;
        }
        return false;
    }

    bool begins_with( const token& tok, uints off=0 ) const
    {
        if( tok._len+off > _len )
            return false;
        const char* p = _ptr+off;
        for( uints i=0; i<tok._len; ++i )
        {
            if( tok._ptr[i] != p[i] )
                return false;
        }
        return true;
    }

    bool begins_with_icase( const char* str ) const
    {
        uints n = _len;
        uints i;
        for( i=0; i<n; ++i )
        {
            if( str[i] == 0 )
                return true;
            if( tolower(str[i]) != tolower(_ptr[i]) )
                return false;
        }
        return str[i] == 0;
    }

    bool begins_with_icase( const token& tok, uints off=0 ) const
    {
        if( tok._len+off > _len )
            return false;
        const char* p = _ptr+off;
        for( uints i=0; i<tok._len; ++i )
        {
            if( tolower(tok._ptr[i]) != tolower(p[i]) )
                return false;
        }
        return true;
    }

    bool ends_with( const token& tok ) const
    {
        if( tok._len > _len )
            return false;
        const char* p = _ptr + _len - tok.len();
        for( uints i=0; i<tok._len; ++i )
        {
            if( tok._ptr[i] != p[i] )
                return false;
        }
        return true;
    }

    bool ends_with_icase( const token& tok ) const
    {
        if( tok._len > _len )
            return false;
        const char* p = _ptr + _len - tok.len();
        for( uints i=0; i<tok._len; ++i )
        {
            if( tolower(tok._ptr[i]) != tolower(p[i]) )
                return false;
        }
        return true;
    }

    bool consume( const token& tok )
    {
        if( tok._len > _len )
            return false;
        for( uints i=0; i<tok._len; ++i )
        {
            if( tok._ptr[i] != _ptr[i] )
                return false;
        }
        _ptr += tok._len;
        _len -= tok._len;
        return true;
    }

    bool consume_icase( const token& tok )
    {
        if( tok._len > _len )
            return false;
        for( uints i=0; i<tok._len; ++i )
        {
            if( tolower(tok._ptr[i]) != tolower(_ptr[i]) )
                return false;
        }
        _ptr += tok._len;
        _len -= tok._len;
        return true;
    }
    
    
    token get_after_substring( const substring& sub ) const
    {
        uints n = count_until_substring(sub);

        if( n < _len )
        {
            n += sub.len();
            return token( _ptr+n, _len-n );
        }
        
        return empty();
    }

    token get_before_substring( const substring& sub ) const
    {
        uints n = count_until_substring(sub);
        return token( _ptr, n );
    }


    ////////////////////////////////////////////////////////////////////////////////
    template< class T >
    struct tonum
    {
        static T touint( token& t, uints offs, uint BaseN )
        {
            T r=0;
            uints i=offs;
            for( ; i<t._len; ++i )
            {
                char k = t._ptr[i];
                uchar a = 255;
                if( k >= '0'  &&  k <= '9' )
                    a = k - '0';
                else if( k >= 'a' && k <= 'z' )
                    a = k - 'a' + 10;
                else if( k >= 'A' && k <= 'Z' )
                    a = k - 'A' + 10;

                if( a >= BaseN )
                    break;

                r = r*BaseN + a;
            }
            t += i;
            return r;
        }

        static T toint( token& t, uints offs, uint BaseN )
        {
            if(t.is_empty())  return 0;
            char c = t[offs];
            if(c == '-')  return -(T)touint(t,offs+1,BaseN);
            if(c == '+')  return (T)touint(t,offs+1,BaseN);
            return (T)touint(t,offs,BaseN);
        }

        static T touint( const char* s, uint BaseN )
        {
            token t(s, UMAX);
            return touint( t, 0, BaseN );
        }

        static T toint( const char* s, uint BaseN )
        {
            if( *s == 0 )  return 0;
            token t(s, UMAX);
            return toint( t, 0, BaseN );
        }
    };

    ///Convert the token to unsigned int using as much digits as possible
    uint touint( uints offs=0 ) const
    {
        token t(*this);
        return tonum<uint>::touint(t, offs, 10 );
    }

    ///Convert the token to unsigned int using as much digits as possible
    uint64 touint64( uints offs=0 ) const
    {
        token t(*this);
        return tonum<uint64>::touint(t, offs, 10 );
    }

    ///Convert the token to unsigned int using as much digits as possible
    uint touint_and_shift( uints offs=0 )
    {
        return tonum<uint>::touint( *this, offs, 10 );
    }

    ///Convert the token to unsigned int using as much digits as possible
    uint64 touint64_and_shift( uints offs=0 )
    {
        return tonum<uint64>::touint( *this, offs, 10 );
    }

    uint get_num_base( uints& off ) const
    {
        uint base=10;
        uints i=off;
        if( _len>i+2  &&  _ptr[i] == '0' )
        {
            ++i;
            if( _ptr[i] == 'x' )  base=16;
            else if( _ptr[i] == 'o' )  base=8;
            else if( _ptr[i] == 'b' )  base=2;
            else if( _ptr[i] >= '0' && _ptr[i] <= '9' )  base=10;

            if( base != 10 )
                ++i;
        }

        off = i;
        return base;
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint xtouint_and_shift( uints offs=0 )
    {
        uint base = get_num_base(offs);
        return tonum<uint>::touint( *this, offs, base );
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint64 xtouint64_and_shift( uints offs=0 )
    {
        uint base = get_num_base(offs);
        return tonum<uint64>::touint( *this, offs, base );
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint touint_and_shift_base( uint base, uints offs=0 )
    {
        return tonum<uint>::touint( *this, offs, base );
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint64 touint64_and_shift_base( uint base, uints offs=0 )
    {
        return tonum<uint64>::touint( *this, offs, base );
    }

    ///Convert the token to signed int using as much digits as possible
    int toint( uints offs=0 ) const
    {
        token t(*this);
        return tonum<int>::toint( t, offs, 10 );
    }

    ///Convert the token to signed int using as much digits as possible
    int64 toint64( uints offs=0 ) const
    {
        token t(*this);
        return tonum<int64>::toint( t, offs, 10 );
    }

    ///Convert the token to signed int using as much digits as possible
    int toint_and_shift( uints offs=0 )
    {
        return tonum<int>::toint( *this, offs, 10 );
    }

    ///Convert the token to signed int using as much digits as possible
    int64 toint64_and_shift( uints offs=0 )
    {
        return tonum<int64>::toint( *this, offs, 10 );
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    int xtoint_and_shift( uints offs=0 )
    {
        uint base = get_num_base(offs);
        return tonum<int>::toint( *this, offs, base );
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    int64 xtoint64_and_shift( uints offs=0 )
    {
        uint base = get_num_base(offs);
        return tonum<int64>::toint( *this, offs, base );
    }


    double todouble( uints offs=0 ) const
    {
        token t = *this;
        return t.todouble_and_shift(offs);
    }
    
    ///Convert the token to double
    double todouble_and_shift( uints offs=0 )
    {
        bool invsign=false;
        if( first_char() == '-' ) { ++_ptr; --_len; invsign=true; }
        else if( first_char() == '+' ) { ++_ptr; --_len; }

        double val = (double)touint_and_shift(offs);

        if( first_char() == '.' )
        {
            ++_ptr;
            --_len;

            uints plen = len();
            uints dec = touint_and_shift();
            plen -= len();
            if(plen && dec)
                val += dec * pow( (double)10, -(int)plen );
        }

        if( first_char() == 'e'  ||  first_char() == 'E' )
        {
            ++_ptr;
            --_len;

            int m = toint_and_shift();
            val *= pow( (double)10, m );
        }

        return invsign ? -val : val;
    }

    opcd todate_local( timet& dst )
    {
        struct tm tmm;

        opcd e = todate( tmm, token::empty() );
        if(e)  return e;

    #ifdef SYSTYPE_MSVC8plus
        dst = _mkgmtime( &tmm );
    #else
        dst = timegm( &tmm );
    #endif
        return 0;
    }

    opcd todate_gmt( timet& dst )
    {
        struct tm tmm;

        opcd e = todate( tmm, "gmt" );
        if(e)  return e;

    #ifdef SYSTYPE_MSVC8plus
        dst = _mkgmtime( &tmm );
    #else
        dst = timegm( &tmm );
    #endif
        return 0;
    }

    opcd todate( struct tm& tmm, const token& timezone )
    {
        //Tue, 15 Nov 1994 08:12:31 GMT

        //skip the day name
        cut_left(',', 1, true);
        cut_left(' ', 1, true);

        tmm.tm_mday = touint_and_shift();
        if( tmm.tm_mday == 0 || tmm.tm_mday > 31 )  return ersINVALID_PARAMS;
        cut_left(' ', 1, true);

        token monstr = cut_left(' ',1,true);
        static const char* mons[] = {
            "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"
        };
        uint mon;
        for( mon=0; mon<12; ++mon )  if( monstr.cmpeqi(mons[mon]) )  break;
        if( mon >= 12 )  return ersINVALID_PARAMS "timefmt: month name";
        tmm.tm_mon = mon;

        tmm.tm_year = touint_and_shift() - 1900;
        cut_left(' ', 1, true);

        token h = cut_left(':', 1, true);
        tmm.tm_hour = h.touint_and_shift();
        if( !h.is_empty() || tmm.tm_hour > 23 )  return ersINVALID_PARAMS "timefmt: hours";

        token m = cut_left(':', 1, true);
        tmm.tm_min = m.touint_and_shift();
        if( !m.is_empty() || tmm.tm_min > 59 )  return ersINVALID_PARAMS "timefmt: minutes";

        token s = cut_left(' ', 1, !timezone.is_empty() );
        tmm.tm_sec = s.touint_and_shift();
        if( !s.is_empty() || tmm.tm_sec > 59 )  return ersINVALID_PARAMS "timefmt: seconds";

        tmm.tm_isdst = 0;//-(int)timezone.is_empty();

        if( !consume_icase(timezone) )  return ersINVALID_PARAMS "timefmt: time zone";
        return 0;
    }

    template<class A>
    bool to_wchar( dynarray<ushort,A>& dst ) const;
};

////////////////////////////////////////////////////////////////////////////////
///Wrapper class for binstream type key
struct key_token : public token {};

////////////////////////////////////////////////////////////////////////////////
inline void substring::set( const token& tok )
{
    set( tok.ptr(), tok.len() );
}

inline token substring::get() const
{
    return token( (const char*)_subs, _len );
}


COID_NAMESPACE_END

#endif //__COID_COMM_TOKEN__HEADER_FILE__

