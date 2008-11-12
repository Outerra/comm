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

#include "substring.h"
#include "tutf8.h"
#include "commtime.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string>

COID_NAMESPACE_BEGIN

#ifdef SYSTYPE_MSVC8plus
# define xstrncasecmp     _strnicmp
# define xstrcasecmp      _stricmp
#else
# define xstrncasecmp     strncasecmp
# define xstrcasecmp      strcasecmp
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


template<class T, class A> class dynarray;
class charstr;

////////////////////////////////////////////////////////////////////////////////
///Token structure describing a part of a string.
struct token
{
    const char *_ptr;                   ///<ptr to the beginning of current string part
    uints _len;                         ///<length of current string part

    token() {
        _ptr = 0;
        _len = 0;
    }

    token( const char *czstr )
    {   set(czstr); }

    token( const charstr& str );

    token( const char* ptr, uints len )
    {
        _ptr = ptr;
        _len = len;
    }

    token( const char* ptr, const char* ptre )
    {
        _ptr = ptr;
        _len = ptre - ptr;
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

    ///Const iterator for token
    struct const_iterator : std::iterator<std::random_access_iterator_tag, char>
    {
        const char* _p;

        difference_type operator - (const const_iterator& p) const  { return _p - p._p; }

        bool operator < (const const_iterator& p) const    { return _p < p._p; }
        bool operator <= (const const_iterator& p) const   { return _p <= p._p; }

        int operator == (const char* p) const  { return p == _p; }

        int operator == (const const_iterator& p) const    { return p._p == _p; }
        int operator != (const const_iterator& p) const    { return p._p != _p; }

        char operator *(void) const         { return *_p; }

        const_iterator& operator ++()       { ++_p;  return *this; }
        const_iterator& operator --()       { --_p;  return *this; }

        const_iterator  operator ++(int)    { const_iterator x(*this);  ++_p;  return x; }
        const_iterator  operator --(int)    { const_iterator x(*this);  --_p;  return x; }

        const_iterator& operator += (ints n)    { _p += n;  return *this; }
        const_iterator& operator -= (ints n)    { _p -= n;  return *this; }

        const_iterator  operator + (ints n) const   { const_iterator t(*this);  t += n;  return t; }
        const_iterator  operator - (ints n) const   { const_iterator t(*this);  t -= n;  return t; }

        char operator [] (ints i) const     { return _p[i]; }

        const_iterator() : _p(0)  { }
        const_iterator( const char* p ) : _p(p)  { }
    };

    const_iterator begin() const    { return const_iterator(ptr()); }
    const_iterator end() const      { return const_iterator(ptre()); }


    void swap( token& t )
    {
        const char* p = t._ptr; t._ptr = _ptr;  _ptr = p;
        uints n = t._len;       t._len = _len;  _len = n;
    }

    const char* ptr() const                 { return _ptr; }
    const char* ptre() const                { return _ptr+_len; }

    ///Return length of token
    uints len() const                       { return _len; }

    ///Return number of unicode characters within utf8 encoded token
    uints len_utf8() const
    {
        uints n=0;
        const char* p = _ptr;
        const char* pe = _ptr+_len;

        while(p<pe) {
            p += get_utf8_seq_expected_bytes(p);
            if(p>pe)
                return n;   //errorneous utf8 token
            
            ++n;
        }
            
        return n;
    }

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


    static token empty()                    { static token _tk(0,(const char*)0);  return _tk; }

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
        return 0 == xstrncasecmp( ptr(), str.ptr(), len() );
    }

    bool cmpeqc( const token& str, bool casecmp ) const
    {
        return casecmp ? cmpeq(str) : cmpeqi(str);
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Compare strings
    //@return -1 if str<this, 0 if str==this, 1 if str>this
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

    ///Compare strings, longer first
    int cmplf( const token& str ) const
    {
        uints lex = str.len();
        int r = memcmp( ptr(), str.ptr(), uint_min(_len,lex) );
        if( r == 0 )
        {
            if( _len<lex )  return 1;
            if( lex<_len )  return -1;
        }
        return r;
    }

    int cmpi( const token& str ) const
    {
        uints lex = str.len();
        int r = xstrncasecmp( ptr(), str.ptr(), uint_min(_len,lex) );
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
    //@return this token if not empty, otherwise the second one
    token operator | (const token& b) const {
        return _len
            ? *this
            : b;
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

    ///Shift the starting pointer forwards or backwards
    token& shift_start( ints i )
    {
        if( i>0  &&  i>(ints)_len )
            i = _len;

        _ptr += i;
        _len -= i;

        return *this;
    }

    ///Shift the end forwards or backwards
    token& shift_end( ints i )
    {
        if( i<0  &&  -i>(ints)_len )
            i = -(ints)_len;

        _len += i;

        return *this;
    }

/*
    const char* operator + (ints i)
    {
        return _ptr + i;
    }
*/
    bool is_empty() const               { return _len == 0; }
    bool is_set() const                 { return _len > 0; }
    bool is_null() const                { return _ptr == 0; }
    void set_empty()                    { _len = 0; }
    void set_null()                     { _ptr = 0; _len = 0; }

    typedef const char* token::*unspecified_bool_type;

    ///Automatic cast to bool for checking emptiness
	operator unspecified_bool_type () const {
	    return _len == 0  ?  0  :  &token::_ptr;
	}


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

    ///Cut maximum of \a n characters from the token from the left side
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

    ///Cut maximum of \a n characters from the token from the right side
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



    ///Flags for cut_xxx methods concerning the treating of separator
    enum ESeparatorTreat {
        fKEEP_SEPARATOR         = 0,    ///< do not remove the separator if found
        fREMOVE_SEPARATOR       = 1,    ///< remove the separator from source token after the cutting
        fREMOVE_ALL_SEPARATORS  = 3,    ///< remove all continuous separators from the source token
        fRETURN_SEPARATOR       = 4,    ///< if neither fREMOVE_SEPARATOR nor fREMOVE_ALL_SEPARATORS is set, return the separator with the cut token, otherwise keep with the source
        fON_FAIL_RETURN_EMPTY   = 8,    ///< if the separator is not found, return an empty token. If not set, whole source token is cut and returned.
        fSWAP                   = 16,   ///< swap resulting source and destination tokens. Note fRETURN_SEPARATOR and fON_FAIL_RETURN_EMPTY flags concern the actual returned value, i.e. after swap.
    };

    ///Behavior of the cut operation. Constructed using ESeparatorTreat flags or-ed.
    struct cut_trait
    {
        uint flags;

        explicit cut_trait(int flags) : flags(flags)
        {}

        //@return true if all continuous separators should be consumed
        bool consume_other_separators() const {
            return (flags & fREMOVE_ALL_SEPARATORS) != 0;
        }

        ///Duplicate cut_trait object but with fSWAP flag set
        cut_trait make_swap() {
            return cut_trait(flags | fSWAP);
        }



        token& process_found( token& source, token& dest, token& sep ) const
        {
            if( !(flags & fREMOVE_SEPARATOR) ) {
                if( ((flags>>2) ^ flags) & fRETURN_SEPARATOR )
                    sep.shift_start( sep._len );
                else
                    sep._len = 0;
            }

            dest._ptr = source._ptr;
            dest._len = sep._ptr - source._ptr;
            source._len = source.ptre() - sep.ptre();
            source._ptr = sep.ptre();

            if( flags & fSWAP )
                source.swap(dest);

            return dest;
        }

        token& process_notfound( token& source, token& dest ) const
        {
            if( flags & fON_FAIL_RETURN_EMPTY ) {
                dest._ptr = (flags & fSWAP) ? source.ptre() : source.ptr();
                dest._len = 0;
            }
            else {
                dest._len = source._len;
                dest._ptr = source._ptr;
                if(!(flags & fSWAP))
                    source._ptr += source._len;
                source._len = 0;
            }

            return dest;
        }
    };

    //@{ Most used traits
    static cut_trait cut_trait_keep_sep_with_source() {
        return cut_trait(fKEEP_SEPARATOR);
    }

    static cut_trait cut_trait_keep_sep_with_returned() {
        return cut_trait(fRETURN_SEPARATOR);
    }

    static cut_trait cut_trait_default_empty() {
        return cut_trait(fON_FAIL_RETURN_EMPTY);
    }

    static cut_trait cut_trait_remove_sep_default_empty() {
        return cut_trait(fREMOVE_SEPARATOR|fON_FAIL_RETURN_EMPTY);
    }

    static cut_trait cut_trait_remove_all_default_empty() {
        return cut_trait(fREMOVE_ALL_SEPARATORS|fON_FAIL_RETURN_EMPTY);
    }

    static cut_trait cut_trait_remove_sep() {
        return cut_trait(fREMOVE_SEPARATOR);
    }

    static cut_trait cut_trait_remove_all() {
        return cut_trait(fREMOVE_ALL_SEPARATORS);
    }
    //@}



    ///Cut a token off, using single character as the delimiter
    ///@param skipsep zero if separator should remain with the original token, nonzero if it's discarded
    token cut_left( char c, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        token r;
        const char* p = strchr(c);
        if(p) {
            token sep(p, 1);
            return ctr.process_found(*this, r, sep);
        }
        else
            return ctr.process_notfound(*this, r);
    }

    ///Cut a token off, using group of single characters as delimiters
    token cut_left( const token& separators, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        token r;
        uints ln = count_notingroup(separators);
        if( _len > ln )         //if not all is not separator
        {
            token sep(_ptr+ln, ctr.consume_other_separators()
                ? count_ingroup(separators,ln)-ln
                : 1);

            return ctr.process_found(*this, r, sep);
        }
        else
            return ctr.process_notfound(*this, r);
    }

    ///Cut left substring
    ///@param skipsep zero if the separator should remain with the original token, nonzero if it's discarded
    token cut_left( const substring& ss, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        token r;
        uints n = count_until_substring(ss);
        if( n < _len )
        {
            token sep(_ptr+n, ss.len());

            return ctr.process_found(*this, r, sep);
        }
        else
            return ctr.process_notfound(*this, r);
    }


    ///Cut left substring, searching for separator backwards
    ///@param skipsep zero if the separator should remain with the original token, nonzero if it's discarded
    token cut_left_back( const char c, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        token r;
        const char* p = strrchr(c);
        if(p)
        {
            token sep(p, 1);
            return ctr.process_found(*this, r, sep);
        }
        else
            return ctr.process_notfound(*this, r);
    }

    ///Cut left substring, searching for separator backwards
    token cut_left_back( const token& separators, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        token r;
        uints off = count_notingroup_back(separators);

        if( off > 0 )
        {
            uints ln = ctr.consume_other_separators()
                ? off - token(_ptr,off).count_ingroup_back(separators)
                : 1;

            token sep(_ptr+off-ln, ln);

            return ctr.process_found(*this, r, sep);
        }
        else
            return ctr.process_notfound(*this, r);
    }

    ///Cut left substring, searching for separator backwards
    token cut_left_back( const substring& ss, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
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
            token sep(_ptr+lastss, ss.len());

            return ctr.process_found(*this, r, sep);
        }
        else
            return ctr.process_notfound(*this, r);
    }


    ///Cut right substring
    token cut_right( const char c, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        return cut_left(c, ctr.make_swap());
    }

    ///Cut right substring
    token cut_right( const token& separators, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        return cut_left(separators, ctr.make_swap());
    }

    ///Cut right substring
    token cut_right( const substring& ss, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        return cut_left(ss, ctr.make_swap());
    }

    ///Cut right substring, searching for separator backwards
    token cut_right_back( const char c, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        return cut_left_back(c, ctr.make_swap());
    }

    ///Cut right substring, searching for separator backwards
    token cut_right_back( const token& separators, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        return cut_left_back(separators, ctr.make_swap());
    }

    ///Cut right substring, searching for separator backwards
    token cut_right_back( const substring& ss, cut_trait ctr = cut_trait(fREMOVE_SEPARATOR) )
    {
        return cut_left_back(ss, ctr.make_swap());
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
        if( off >= _len )  return _len;

        return sub.find( _ptr+off, _len-off );
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
        if( c == '\r' )
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
        token r = cut_left('\n');
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
    ///Helper class for string to number conversions
    template< class T >
    struct tonum
    {
        uint BaseN;
        bool success;


        tonum( uint BaseN=10 ) : BaseN(BaseN) {}

        ///@return true if the conversion failed
        bool failed() const             { return !success; }

        ///Deduce the numeric base (0x, 0o, 0b or decimal)
        void get_num_base( token& tok )
        {
            BaseN = 10;
            if( tok.len()>2  &&  tok[0] == '0' )
            {
                char c = tok[1];
                if( c == 'x' )  BaseN=16;
                else if( c == 'o' )  BaseN=8;
                else if( c == 'b' )  BaseN=2;
                else if( c >= '0' && c <= '9' )  BaseN=10;

                if( BaseN != 10 )
                    tok.shift_start(2);
            }
        }

        ///Convert part of the token to unsigned integer deducing the numeric base (0x, 0o, 0b or decimal)
        T xtouint( token& t )
        {
            get_num_base(t);
            return touint(t);
        }

        ///Convert part of the token to unsigned integer
        T touint( token& t )
        {
            success = false;
            T r=0;
            uints i=0;

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
                success = true;
            }

            t.shift_start(i);
            return r;
        }

        ///Convert part of the token to signed integer deducing the numeric base (0x, 0o, 0b or decimal)
        T xtoint( token& t )
        {
            if(t.is_empty()) {
                success = false;
                return 0;
            }
            char c = t[0];
            if(c == '-')  return -(T)xtouint(t.shift_start(1));
            if(c == '+')  return (T)xtouint(t.shift_start(1));
            return (T)xtouint(t);
        }

        ///Convert part of the token to signed integer
        T toint( token& t )
        {
            if(t.is_empty()) {
                success = false;
                return 0;
            }
            char c = t[0];
            if(c == '-')  return -(T)touint(t.shift_start(1));
            if(c == '+')  return (T)touint(t.shift_start(1));
            return (T)touint(t);
        }

        T touint( const char* s )
        {
            token t(s, UMAX);
            return touint(t);
        }

        T toint( const char* s )
        {
            if( *s == 0 )  return 0;
            token t(s, UMAX);
            return toint(t);
        }
    };

    ///Convert the token to unsigned int using as much digits as possible
    uint touint() const
    {
        token t(*this);
        tonum<uint> conv(10);
        return conv.touint(t);
    }

    ///Convert the token to unsigned int using as much digits as possible
    uint64 touint64( ) const
    {
        token t(*this);
        tonum<uint64> conv(10);
        return conv.touint(t);
    }

    ///Convert the token to unsigned int using as much digits as possible, deducing the numeric base
    uint xtouint() const
    {
        token t(*this);
        tonum<uint> conv;
        return conv.xtouint(t);
    }

    ///Convert the token to unsigned int using as much digits as possible, deducing the numeric base
    uint64 xtouint64( ) const
    {
        token t(*this);
        tonum<uint64> conv;
        return conv.xtouint(t);
    }

    ///Convert the token to unsigned int using as much digits as possible
    uint touint_and_shift()
    {
        tonum<uint> conv(10);
        return conv.touint(*this);
    }

    ///Convert the token to unsigned int using as much digits as possible
    uint64 touint64_and_shift()
    {
        tonum<uint64> conv(10);
        return conv.touint(*this);
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint xtouint_and_shift()
    {
        tonum<uint> conv;
        return conv.xtouint(*this);
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint64 xtouint64_and_shift()
    {
        tonum<uint> conv;
        return conv.xtouint(*this);
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint touint_and_shift_base( uint base )
    {
        tonum<uint> conv(base);
        return conv.touint(*this);
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    uint64 touint64_and_shift_base( uint base )
    {
        tonum<uint64> conv(base);
        return conv.touint(*this);
    }

    ///Convert the token to signed int using as much digits as possible
    int toint() const
    {
        token t(*this);
        tonum<int> conv(10);
        return conv.toint(t);
    }

    ///Convert the token to signed int using as much digits as possible
    int64 toint64() const
    {
        token t(*this);
        tonum<int64> conv(10);
        return conv.toint(t);
    }

    ///Convert the token to signed int using as much digits as possible, deducing the numeric base
    int xtoint() const
    {
        token t(*this);
        tonum<int> conv;
        return conv.xtoint(t);
    }

    ///Convert the token to signed int using as much digits as possible, deducing the numeric base
    int64 xtoint64() const
    {
        token t(*this);
        tonum<int64> conv;
        return conv.xtoint(t);
    }

    ///Convert the token to signed int using as much digits as possible
    int toint_and_shift()
    {
        tonum<int> conv(10);
        return conv.toint(*this);
    }

    ///Convert the token to signed int using as much digits as possible
    int64 toint64_and_shift()
    {
        tonum<int64> conv(10);
        return conv.toint(*this);
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    int xtoint_and_shift()
    {
        tonum<int> conv;
        return conv.xtoint(*this);
    }

    ///Convert a hexadecimal, decimal, octal or binary token to unsigned int using as much digits as possible
    int64 xtoint64_and_shift()
    {
        tonum<int64> conv;
        return conv.xtoint(*this);
    }

    //@{ Conversion to numbers, given size of the integer type and a destination address
    bool toint_any( void* dst, uint size ) const
    {
        token tok = *this;
        switch(size) {
        case sizeof(int8):  { tonum<int8> conv;  *(int8*)dst = conv.toint(tok); } break;
        case sizeof(int16): { tonum<int16> conv; *(int16*)dst = conv.toint(tok); } break;
        case sizeof(int32): { tonum<int32> conv; *(int32*)dst = conv.toint(tok); } break;
        case sizeof(int64): { tonum<int64> conv; *(int64*)dst = conv.toint(tok); } break;
        default:
            return false;
        }
        return true;
    }

    bool touint_any( void* dst, uint size ) const
    {
        token tok = *this;
        switch(size) {
        case sizeof(uint8):  { tonum<uint8> conv;  *(uint8*)dst = conv.touint(tok); } break;
        case sizeof(uint16): { tonum<uint16> conv; *(uint16*)dst = conv.touint(tok); } break;
        case sizeof(uint32): { tonum<uint32> conv; *(uint32*)dst = conv.touint(tok); } break;
        case sizeof(uint64): { tonum<uint64> conv; *(uint64*)dst = conv.touint(tok); } break;
        default:
            return false;
        }
        return true;
    }

    bool xtoint_any( void* dst, uint size ) const
    {
        token tok = *this;
        switch(size) {
        case sizeof(int8):  { tonum<int8> conv;  *(int8*)dst = conv.xtoint(tok); } break;
        case sizeof(int16): { tonum<int16> conv; *(int16*)dst = conv.xtoint(tok); } break;
        case sizeof(int32): { tonum<int32> conv; *(int32*)dst = conv.xtoint(tok); } break;
        case sizeof(int64): { tonum<int64> conv; *(int64*)dst = conv.xtoint(tok); } break;
        default:
            return false;
        }
        return true;
    }

    bool xtouint_any( void* dst, uint size ) const
    {
        token tok = *this;
        switch(size) {
        case sizeof(uint8):  { tonum<uint8> conv;  *(uint8*)dst = conv.xtouint(tok); } break;
        case sizeof(uint16): { tonum<uint16> conv; *(uint16*)dst = conv.xtouint(tok); } break;
        case sizeof(uint32): { tonum<uint32> conv; *(uint32*)dst = conv.xtouint(tok); } break;
        case sizeof(uint64): { tonum<uint64> conv; *(uint64*)dst = conv.xtouint(tok); } break;
        default:
            return false;
        }
        return true;
    }

    bool toint_any_and_shift( void* dst, uint size )
    {
        switch(size) {
        case sizeof(int8):  { tonum<int8> conv;  *(int8*)dst = conv.toint(*this); } break;
        case sizeof(int16): { tonum<int16> conv; *(int16*)dst = conv.toint(*this); } break;
        case sizeof(int32): { tonum<int32> conv; *(int32*)dst = conv.toint(*this); } break;
        case sizeof(int64): { tonum<int64> conv; *(int64*)dst = conv.toint(*this); } break;
        default:
            return false;
        }
        return true;
    }

    bool touint_any_and_shift( void* dst, uint size )
    {
        switch(size) {
        case sizeof(uint8):  { tonum<uint8> conv;  *(uint8*)dst = conv.touint(*this); } break;
        case sizeof(uint16): { tonum<uint16> conv; *(uint16*)dst = conv.touint(*this); } break;
        case sizeof(uint32): { tonum<uint32> conv; *(uint32*)dst = conv.touint(*this); } break;
        case sizeof(uint64): { tonum<uint64> conv; *(uint64*)dst = conv.touint(*this); } break;
        default:
            return false;
        }
        return true;
    }

    bool xtoint_any_and_shift( void* dst, uint size )
    {
        switch(size) {
        case sizeof(int8):  { tonum<int8> conv;  *(int8*)dst = conv.xtoint(*this); } break;
        case sizeof(int16): { tonum<int16> conv; *(int16*)dst = conv.xtoint(*this); } break;
        case sizeof(int32): { tonum<int32> conv; *(int32*)dst = conv.xtoint(*this); } break;
        case sizeof(int64): { tonum<int64> conv; *(int64*)dst = conv.xtoint(*this); } break;
        default:
            return false;
        }
        return true;
    }

    bool xtouint_any_and_shift( void* dst, uint size )
    {
        switch(size) {
        case sizeof(uint8):  { tonum<uint8> conv;  *(uint8*)dst = conv.xtouint(*this); } break;
        case sizeof(uint16): { tonum<uint16> conv; *(uint16*)dst = conv.xtouint(*this); } break;
        case sizeof(uint32): { tonum<uint32> conv; *(uint32*)dst = conv.xtouint(*this); } break;
        case sizeof(uint64): { tonum<uint64> conv; *(uint64*)dst = conv.xtouint(*this); } break;
        default:
            return false;
        }
        return true;
    }

    //@}


    ///Convert token to double value, consuming as much as possible
    double todouble() const
    {
        token t = *this;
        return t.todouble_and_shift();
    }
    
    ///Convert token to double value, shifting the consumed part
    double todouble_and_shift()
    {
        bool invsign=false;
        if( first_char() == '-' ) { ++_ptr; --_len; invsign=true; }
        else if( first_char() == '+' ) { ++_ptr; --_len; }

        double val = (double)touint_and_shift();

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

    ///Convert string (in local time) to datetime value
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

    ///Convert string (in gmt time) to datetime value
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

    ///Convert string (in specified timezone) to datetime value
    opcd todate( struct tm& tmm, const token& timezone )
    {
        //Tue, 15 Nov 1994 08:12:31 GMT

        cut_trait ctr(fREMOVE_SEPARATOR|fON_FAIL_RETURN_EMPTY);

        //skip the day name
        cut_left(',', ctr);
        cut_left(' ', ctr);

        tmm.tm_mday = touint_and_shift();
        if( tmm.tm_mday == 0 || tmm.tm_mday > 31 )  return ersINVALID_PARAMS;
        cut_left(' ', ctr);

        token monstr = cut_left(' ', ctr);
        static const char* mons[] = {
            "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"
        };
        uint mon;
        for( mon=0; mon<12; ++mon )  if( monstr.cmpeqi(mons[mon]) )  break;
        if( mon >= 12 )  return ersINVALID_PARAMS "timefmt: month name";
        tmm.tm_mon = mon;

        tmm.tm_year = touint_and_shift() - 1900;
        cut_left(' ', ctr);

        token h = cut_left(':', ctr);
        tmm.tm_hour = h.touint_and_shift();
        if( !h.is_empty() || tmm.tm_hour > 23 )  return ersINVALID_PARAMS "timefmt: hours";

        token m = cut_left(':', ctr);
        tmm.tm_min = m.touint_and_shift();
        if( !m.is_empty() || tmm.tm_min > 59 )  return ersINVALID_PARAMS "timefmt: minutes";

        uint fctr = fREMOVE_SEPARATOR;
        if(!timezone.is_empty())
            fctr |= fON_FAIL_RETURN_EMPTY;

        token s = cut_left(' ', cut_trait(fctr));
        tmm.tm_sec = s.touint_and_shift();
        if( !s.is_empty() || tmm.tm_sec > 59 )  return ersINVALID_PARAMS "timefmt: seconds";

        tmm.tm_isdst = 0;//-(int)timezone.is_empty();

        if( !consume_icase(timezone) )  return ersINVALID_PARAMS "timefmt: time zone";
        return 0;
    }

    ///Convert token to array of wchar_t characters
    template<class A>
    bool utf8_to_wchar_buf( dynarray<wchar_t,A>& dst ) const;

    ///Convert token to wide stl string
    bool utf8_to_wstring( std::wstring& dst ) const
    {
        dst.clear();
        return utf8_to_wstring_append(dst);
    }

    ///Append token to wide stl string
    bool utf8_to_wstring_append( std::wstring& dst ) const
    {
        uints i=dst.length(), n=len();
        const char* p = ptr();

        dst.resize(i+n);
        wchar_t* data = const_cast<wchar_t*>(dst.data());

        while(n>0)
        {
            if( (uchar)*p <= 0x7f ) {
                data[i++] = *p++;
                --n;
            }
            else
            {
                uint ne = get_utf8_seq_expected_bytes(p);
                if( ne > n )  return false;

                data[i++] = (wchar_t)read_utf8_seq(p);
                p += ne;
                n -= ne;
            }
        }

        dst.resize(i);

        return true;
    }

#ifdef SYSTYPE_WIN32
    ///Convert token to wide stl string
    bool codepage_to_wstring( uint cp, std::wstring& dst ) const
    {
        dst.clear();
        return codepage_to_wstring_append(cp,dst);
    }

    ///Append token to wide stl string
    bool codepage_to_wstring_append( uint cp, std::wstring& dst ) const;
#endif

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

