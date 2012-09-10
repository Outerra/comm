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
//#include "../dynarray.h"


COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
template<class KEY> struct hash
{
    typedef KEY     key_type;
};

inline uint __coid_hash_string( const char* s )
{
    uint h = 2166136261u;
    for( ; *s; ++s)
        h = (h ^ *s)*16777619u;
        //h = (h ^ *s) + (h<<26) + (h>>6);

    return h;
}


inline uint __coid_hash_string( const char* s, uints n )
{
    uint h = 2166136261u;
    for( ; n>0; ++s,--n)
        h = (h ^ *s)*16777619u;
        //h = (h ^ *s) + (h<<26) + (h>>6);

    return h;
}

////////////////////////////////////////////////////////////////////////////////
template<> struct hash<char*>
{
    typedef char* key_type;
    uint operator()(const char* s) const { return __coid_hash_string(s); }
};

template<> struct hash<const char*>
{
    typedef const char* key_type;
    uint operator()(const char* s) const { return __coid_hash_string(s); }
};

template<> struct hash<char> {
    typedef char key_type;
    uint operator()(char x) const { return x; }
};
template<> struct hash<unsigned char> {
    typedef unsigned char key_type;
    uint operator()(unsigned char x) const { return x; }
};
template<> struct hash<signed char> {
    typedef signed char key_type;
    uint operator()(unsigned char x) const { return x; }
};

template<> struct hash<short> {
    typedef short key_type;
    uint operator()(short x) const { return x; }
};
template<> struct hash<unsigned short> {
    typedef unsigned short key_type;
    uint operator()(unsigned short x) const { return x; }
};
template<> struct hash<int> {
    typedef int key_type;
    uint operator()(int x) const { return x; }
};
template<> struct hash<unsigned int> {
    typedef unsigned int key_type;
    uint operator()(unsigned int x) const { return x; }
};
template<> struct hash<long> {
    typedef long key_type;
    uint operator()(long x) const { return x; }
};
template<> struct hash<unsigned long> {
    typedef unsigned long key_type;
    uint operator()(unsigned long x) const { return x; }
};

#if defined(SYSTYPE_MSVC) || defined(SYSTYPE_32)
template<> struct hash<int64> {
    typedef int64 key_type;
    uint operator()(int64 x) const { return (uint)x; }
};
template<> struct hash<uint64> {
    typedef uint64 key_type;
    uint operator()(uint64 x) const { return (uint)x; }
};
#endif



//String literal hashing

template <uint N, uint I>
struct FnvHash
{
    FORCEINLINE static uint hash(const char (&str)[N])
    {
        return (FnvHash<N, I-1>::hash(str) ^ str[I-1])*16777619u;
    }
};
 
template <uint N>
struct FnvHash<N,1>
{
    FORCEINLINE static uint hash(const char (&str)[N])
    {
        return (2166136261u ^ str[0])*16777619u;
    }
};

struct token;

///Compile-time string hash
class string_hash
{
public:
    ///String literal constructor
    template <unsigned int N>
    FORCEINLINE string_hash(const char (&str)[N])
        : _hash(FnvHash<N,N>::hash(str))
    {}

    string_hash(const token& t);

    operator uint() const {
        return _hash;
    }

private:
    uint _hash;
};


/*
#define HASH_STRING64(x) (uint32((x)[0])+0x1b*uint32((x)[1])+0x2d9*uint32((x)[2])+0x4ce3*uint32((x)[3])+0x81bf1*uint32((x)[4])+0xdaf26b*uint32((x)[5])+0x17179149*uint32((x)[6])+0x6f7c52b3*uint32((x)[7])+0xc21cb8e1*uint32((x)[8])+0x79077fbb*uint32((x)[9])+0xc3ca78b9*uint32((x)[10])+0xa65abb83*uint32((x)[11])+0x8b91c6d1*uint32((x)[12])+0xb85ff80b*uint32((x)[13])+0x721f2929*\
uint32((x)[14])+0x9495753*uint32((x)[15])+0xfabc35c1*uint32((x)[16])+0x71d9ab5b*uint32((x)[17])+0x1f51299*uint32((x)[18])+0x34d8f623*uint32((x)[19])+0x92e1f5b1*uint32((x)[20])+0x7dd4e9ab*uint32((x)[21])+0x4574a509*uint32((x)[22])+0x534d67f3*uint32((x)[23])+0xc929f6a1*uint32((x)[24])+0x376d02fb*uint32((x)[25])+0xd87f5079*uint32((x)[26])+0xd56d7cc3*\
uint32((x)[27])+0x828c2891*uint32((x)[28])+0xc4c8474b*uint32((x)[29])+0xc11f84e9*uint32((x)[30])+0x5e530493*uint32((x)[31])+0xdeadbeef*uint32((x)[32])+0x7c532335*uint32((x)[33])+0x1cc4b697*uint32((x)[34])+0x8bf41ed*uint32((x)[35])+0xec2bf3ff*uint32((x)[36])+0xe8a2bbe5*uint32((x)[37])+0x8929d127*uint32((x)[38])+0x77690f1d*uint32((x)[39])+\
0x9814980f*uint32((x)[40])+0xa2c0995*uint32((x)[41])+0x12a502b7*uint32((x)[42])+0xf767494d*uint32((x)[43])+0x17e4bb1f*uint32((x)[44])+0x851fbc45*uint32((x)[45])+0xa58db47*uint32((x)[46])+0x175f207d*uint32((x)[47])+0x77086d2f*uint32((x)[48])+0x8de383f5*uint32((x)[49])+0xf6feead7*uint32((x)[50])+0xce2c4ad*uint32((x)[51])+0x5beabe3f*uint32((x)[52])+\
0xb1c210a5*uint32((x)[53])+0xbf77c167*uint32((x)[54])+0x31a165dd*uint32((x)[55])+0x3c05be4f*uint32((x)[56])+0x549b1255*uint32((x)[57])+0xec5aeef7*uint32((x)[58])+0xed97340d*uint32((x)[59])+0xef27d5f*uint32((x)[60])+0x93933905*uint32((x)[61])+0x90870387*uint32((x)[62])+0x3e3d5f3d*uint32((x)[63]))

///Compile-time string literal hashing, strings up to 64 bytes
#define HASH_LITERAL(t) HASH_STRING64(t "                                                                ")
*/



COID_NAMESPACE_END

#endif //__COID_COMM_HASHFUNC__HEADER_FILE__
