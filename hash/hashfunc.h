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

#ifndef __COID_COMM_HASHFUNC__HEADER_FILE__
#define __COID_COMM_HASHFUNC__HEADER_FILE__


#include "../namespace.h"
#include <ctype.h>
#include <functional>


COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
template<class KEYSTORE, class KEYLOOKUP=KEYSTORE>
struct equal_to	: public std::binary_function<KEYSTORE, KEYLOOKUP, bool>
{
	bool operator()(const KEYSTORE& _Left, const KEYLOOKUP& _Right) const
    {
        return (_Left == _Right);
    }
};


////////////////////////////////////////////////////////////////////////////////
template<class KEY, bool INSENSITIVE=false> struct hash
{
    typedef KEY     key_type;
};

///FNV-1a hash
inline uint __coid_hash_c_string( const char* s, uint seed = 2166136261u )
{
    for( ; *s; ++s)
        seed = (seed ^ *s)*16777619u;

    return seed;
}


inline uint __coid_hash_string( const char* s, uints n, uint seed = 2166136261u )
{
    //for( ; n>0; ++s,--n)
    //    seed = (seed ^ *s)*16777619u;

    //unrolled
    for(uints i=0; i < (n&-2); i+=2) {
        seed = (seed ^ s[i])   * 16777619u;
        seed = (seed ^ s[i+1]) * 16777619u;
    }
    if(n&1)
        seed = (seed ^ s[n-1])*16777619u;

    return seed;
}

inline uint __coid_hash_c_string_insensitive( const char* s, uint seed = 2166136261u )
{
    for( ; *s; ++s)
        seed = (seed ^ ::tolower(*s))*16777619u;

    return seed;
}


inline uint __coid_hash_string_insensitive( const char* s, uints n, uint seed = 2166136261u )
{
    for( ; n>0; ++s,--n)
        seed = (seed ^ ::tolower(*s))*16777619u;

    return seed;
}

////////////////////////////////////////////////////////////////////////////////
template<bool INSENSITIVE> struct hash<char*, INSENSITIVE>
{
    typedef char* key_type;
    uint operator()(const char* s) const {
        return INSENSITIVE
            ? __coid_hash_c_string_insensitive(s)
            : __coid_hash_c_string(s);
    }
};

template<bool INSENSITIVE> struct hash<const char*, INSENSITIVE>
{
    typedef const char* key_type;
    uint operator()(const char* s) const {
        return INSENSITIVE
            ? __coid_hash_c_string_insensitive(s)
            : __coid_hash_c_string(s);
    }
};

#define DIRECT_HASH_FUNC(TYPE) template<> struct hash<TYPE> { typedef TYPE key_type;  uint operator()(TYPE x) const { return (uint)(uints)x; } }

DIRECT_HASH_FUNC(bool);
DIRECT_HASH_FUNC(uint8);
DIRECT_HASH_FUNC(int8);
DIRECT_HASH_FUNC(int16);
DIRECT_HASH_FUNC(uint16);
DIRECT_HASH_FUNC(int32);
DIRECT_HASH_FUNC(uint32);
DIRECT_HASH_FUNC(int64);
DIRECT_HASH_FUNC(uint64);
DIRECT_HASH_FUNC(char);

#ifdef SYSTYPE_WIN
# ifdef SYSTYPE_32
DIRECT_HASH_FUNC(ints);
DIRECT_HASH_FUNC(uints);
# else //SYSTYPE_64
DIRECT_HASH_FUNC(int);
DIRECT_HASH_FUNC(uint);
# endif
#elif defined(SYSTYPE_32)
DIRECT_HASH_FUNC(long);
DIRECT_HASH_FUNC(ulong);
#endif


//String literal hashing

template<size_t I>
inline coid_constexpr uint literal_hash( const char* str ) {
    return (literal_hash<I-1>(str) ^ str[I]) * 16777619ULL;
}
 
template<>
inline coid_constexpr uint literal_hash<0>( const char* str ) {
    return str[0] * 16777619ULL;
}

template<size_t N>
inline coid_constexpr uint literal_hash( const char (&str)[N] ) {
    return literal_hash<N-1>(str);
}

template<size_t N>
inline coid_constexpr uint literal_hash( char (&str)[N] ) {
    return literal_hash<N-1>(str);
}

uint string_hash( const token& tok );

inline uint string_hash( const char* czstr ) {
    return __coid_hash_c_string(czstr);
}


COID_NAMESPACE_END

#endif //__COID_COMM_HASHFUNC__HEADER_FILE__
