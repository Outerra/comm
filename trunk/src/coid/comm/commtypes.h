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

#ifndef __COID_COMM_COMMTYPES__HEADER_FILE__
#define __COID_COMM_COMMTYPES__HEADER_FILE__

#include "namespace.h"
#include "trait.h"

#if defined(__CYGWIN__)

# define SYSTYPE_WIN        1
# define SYSTYPE_CYGWIN     1

# if !defined(NDEBUG) && !defined(_DEBUG)
#  define _DEBUG
# endif

#elif defined(_MSC_VER)

# define SYSTYPE_WIN        1
# define SYSTYPE_MSVC       1

# if _MSC_VER >= 1400
#  define SYSTYPE_MSVC8plus
# endif
# if _MSC_VER >= 1300
#  define SYSTYPE_MSVC7plus
# endif
# if _MSC_VER >= 1200
#  define SYSTYPE_MSVC6plus
# endif

# if _MSC_VER < 1200
#  error unsupported compiler
# elif _MSC_VER < 1300
#  define SYSTYPE_MSVC6
# elif _MSC_VER < 1400
#  define SYSTYPE_MSVC7
# else
#  define SYSTYPE_MSVC8
# endif

# if !defined(_DEBUG) && !defined(NDEBUG)
#  define NDEBUG
# endif

#elif defined(__linux__)

# define SYSTYPE_LINUX     1

# if !defined(NDEBUG) && !defined(_DEBUG)
#  define _DEBUG
# endif

#endif


#ifdef SYSTYPE_MSVC
# if defined(_WIN64)
#  define SYSTYPE_64
# else
#  define SYSTYPE_32
# endif
#else
# ifdef __LP64__
#  define SYSTYPE_64
# else
#  define SYSTYPE_32
# endif
#endif

//#define _USE_32BIT_TIME_T
#include <sys/types.h>
#include <stddef.h>

/// Operator new for preallocated storage
inline void * operator new (size_t, const void *p) { return (void*)p; }
inline void operator delete (void *, const void *) {}


namespace coid {

#ifdef SYSTYPE_MSVC
typedef short               __int16_t;
typedef long                __int32_t;
typedef __int64             __int64_t;

typedef unsigned short      __uint16_t;
typedef unsigned long       __uint32_t;
typedef unsigned __int64    __uint64_t;

typedef signed char     	__int8_t;
typedef unsigned char     	__uint8_t;
#endif

// standart data types
typedef __int8_t            int8;
typedef __int16_t           int16;
typedef __int32_t           int32;
typedef __int64_t           int64;
typedef __uint8_t           uint8;
typedef __uint16_t          uint16;
typedef __uint32_t          uint32;
typedef __uint64_t      	uint64;
typedef float               flt32;
typedef double              flt64;


#ifndef __USE_MISC      // defined on linux systems in sys/types.h
#define COID_UINT_DEFINED
    typedef unsigned int		uint;
    typedef unsigned long       ulong;
    typedef unsigned short      ushort;
#endif


typedef unsigned char       uchar;
typedef signed char         schar;

///Integer types with the same size as pointer on the platform
typedef size_t              uints;
typedef ptrdiff_t           ints;

} //namespace coid

#ifndef COID_COMMTYPES_IN_NAMESPACE
using coid::int8;
using coid::int16;
using coid::int32;
using coid::int64;
using coid::uint8;
using coid::uint16;
using coid::uint32;
using coid::uint64;
using coid::uchar;
using coid::schar;
using coid::uints;
using coid::ints;
# ifdef COID_UINT_DEFINED
using coid::uint;
using coid::ulong;
using coid::ushort;
# endif
#endif


COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
TYPE_TRIVIAL(bool);

TYPE_TRIVIAL(uint8);
TYPE_TRIVIAL(int8);
TYPE_TRIVIAL(int16);
TYPE_TRIVIAL(uint16);
TYPE_TRIVIAL(int32);
TYPE_TRIVIAL(uint32);
TYPE_TRIVIAL(int64);
TYPE_TRIVIAL(uint64);

TYPE_TRIVIAL(char);

#ifdef SYSTYPE_32
TYPE_TRIVIAL(ints);
TYPE_TRIVIAL(uints);
#else
//TYPE_TRIVIAL(long);
//TYPE_TRIVIAL(ulong);
#endif

TYPE_TRIVIAL(float);
TYPE_TRIVIAL(double);
TYPE_TRIVIAL(long double);



////////////////////////////////////////////////////////////////////////////////
template<class INT>
struct SIGNEDNESS
{
    //typedef int     SIGNED;
    //typedef uint    UNSIGNED;
};

#define SIGNEDNESS_MACRO(T,S,U) \
template<> struct SIGNEDNESS<T> { typedef S SIGNED; typedef U UNSIGNED; };


SIGNEDNESS_MACRO(int,int,uint);
SIGNEDNESS_MACRO(uint,int,uint);

SIGNEDNESS_MACRO(int8,int8,uint8);
SIGNEDNESS_MACRO(uint8,int8,uint8);
SIGNEDNESS_MACRO(int16,int16,uint16);
SIGNEDNESS_MACRO(uint16,int16,uint16);
SIGNEDNESS_MACRO(int32,int32,uint32);
SIGNEDNESS_MACRO(uint32,int32,uint32);
SIGNEDNESS_MACRO(int64,int64,uint64);
SIGNEDNESS_MACRO(uint64,int64,uint64);
/*
#ifdef SYSTYPE_MSVC
    //SIGNEDNESS_MACRO(__int64);
    template<> struct SIGNEDNESS<int32> { typedef int32 SIGNED; typedef uint32 UNSIGNED; };
    template<> struct SIGNEDNESS<uint32> { typedef int32 SIGNED; typedef uint32 UNSIGNED; };
    template<> struct SIGNEDNESS<int64> { typedef int64 SIGNED; typedef uint64 UNSIGNED; };
    template<> struct SIGNEDNESS<uint64> { typedef int64 SIGNED; typedef uint64 UNSIGNED; };
#else
    SIGNEDNESS_MACRO(int);
# if (__WORDSIZE==32)
    template<> struct SIGNEDNESS<__S64_TYPE> { typedef __S64_TYPE SIGNED; typedef __U64_TYPE UNSIGNED; };
    template<> struct SIGNEDNESS<__U64_TYPE> { typedef __S64_TYPE SIGNED; typedef __U64_TYPE UNSIGNED; };
# endif
#endif //_MSC_VER
*/

////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_MSVC
# define TEMPLFRIEND
#else
# define TEMPLFRIEND <>
#endif


////////////////////////////////////////////////////////////////////////////////
///Structure used as argument to constructors that should not initialize the object
struct NOINIT_t
{};

#define NOINIT      NOINIT_t()

////////////////////////////////////////////////////////////////////////////////
///Shift pointer address in bytes
template <class T>
T* ptr_byteshift( T* p, ints b )
{
    return (T*) ((char*)p + b);
}


////////////////////////////////////////////////////////////////////////////////
void *_xmemcpy( void *dest, const void *src, size_t count );
#if 0//defined _DEBUG
#define xmemcpy     _xmemcpy
#else
#define xmemcpy     ::memcpy
#endif


///Find occurence of 0xcdcd in a two-part buffer
///@return true if not found (use with ASSERT)
bool cdcd_memcheck( const uchar* a, const uchar* ae, const uchar* b, const uchar* be );


COID_NAMESPACE_END

#ifdef SYSTYPE_64 
	#define UMAXS       static_cast<coid::uints>(0xffffffffffffffffULL)
#else
	#define UMAXS       static_cast<coid::uints>(0xffffffffUL)
#endif

#define UMAX32      0xffffffffUL
#define UMAX64      0xffffffffffffffffULL
#define WMAX        0xffff


////////////////////////////////////////////////////////////////////////////////
///Maximum default type alignment. Can be overriden for particular type by
/// specializing alignment_trait for T
#ifdef SYSTYPE_32
static const int MaxAlignment = 8;
#else
static const int MaxAlignment = 16;
#endif

template<class T>
static int alignment_size()
{
    int alignment = alignment_trait<T>::alignment;

    if(!alignment)
        alignment = sizeof(T) > MaxAlignmentPow2
            ?  MaxAlignment
            :  sizeof(T);

    return alignment;
}

///Align pointer to proper boundary, in forward direction
template<class T>
static T* align_forward( void* p )
{
    int mask = alignment_size<T>() - 1;

    return static_cast<T*>( (static_cast<uints>(p) + mask) &~ mask );
}



#include "net_ul.h"


#endif //__COID_COMM_COMMTYPES__HEADER_FILE__
