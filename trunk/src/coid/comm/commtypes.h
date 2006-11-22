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

# define SYSTYPE_WIN32     1
# define SYSTYPE_CYGWIN    1

# if !defined(NDEBUG) && !defined(_DEBUG)
#  define _DEBUG
# endif

#elif defined(_WIN32) || defined(__WIN32__) || defined(_MSC_VER)

# define SYSTYPE_WIN32     1
# define SYSTYPE_MSVC      1

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
# ifdef _WIN64
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

#ifndef SYSTYPE_MSVC6
    /// Operator new for preallocated storage
    inline void * operator new (size_t, const void *p) { return (void*)p; }
    inline void operator delete (void *, const void *) {}
#endif


COID_NAMESPACE_BEGIN

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


#ifndef __USE_MISC      /// defined on linux systems in sys/types.h
    typedef unsigned int		uint;
    typedef unsigned long       ulong;
    typedef unsigned short      ushort;
#endif


typedef unsigned char       uchar;
typedef signed char         schar;

///Integer types with the same size as pointer on the platform
typedef size_t              uints;
typedef ptrdiff_t           ints;

#ifdef SYSTYPE_64
typedef uint64              uints_to;
typedef int64               ints_to;
#else
typedef uint                uints_to;
typedef int                 ints_to;
#endif

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

#ifdef _MSC_VER
TYPE_TRIVIAL(ints);
TYPE_TRIVIAL(uints);
#else
TYPE_TRIVIAL(long);
TYPE_TRIVIAL(ulong);
#endif

TYPE_TRIVIAL(float);
TYPE_TRIVIAL(double);
TYPE_TRIVIAL(long double);



#ifndef SYSTYPE_MSVC6
# define UMAX                ((uints)0xffffffffffffffffULL)
#else
# define UMAX                ((uints)0xffffffffUL)
#endif

#define WMAX                0xffff



////////////////////////////////////////////////////////////////////////////////
template<class INT>
struct SIGNEDNESS
{
    typedef int     SIGNED;
    typedef uint    UNSIGNED;
};

#define SIGNEDNESS_MACRO(T) \
template<> struct SIGNEDNESS<T> { typedef T SIGNED; typedef unsigned T UNSIGNED; }; \
template<> struct SIGNEDNESS<unsigned T> { typedef T SIGNED; typedef unsigned T UNSIGNED; }

SIGNEDNESS_MACRO(char);
SIGNEDNESS_MACRO(short);
SIGNEDNESS_MACRO(long);

#ifdef _MSC_VER
    //SIGNEDNESS_MACRO(__int64);
    template<> struct SIGNEDNESS<ints> { typedef ints SIGNED; typedef uints UNSIGNED; };
    template<> struct SIGNEDNESS<uints> { typedef ints SIGNED; typedef uints UNSIGNED; };
    template<> struct SIGNEDNESS<int64> { typedef int64 SIGNED; typedef uint64 UNSIGNED; };
    template<> struct SIGNEDNESS<uint64> { typedef int64 SIGNED; typedef uint64 UNSIGNED; };
#else
    SIGNEDNESS_MACRO(int);
# if (__WORDSIZE==32)
    template<> struct SIGNEDNESS<__S64_TYPE> { typedef __S64_TYPE SIGNED; typedef __U64_TYPE UNSIGNED; };
    template<> struct SIGNEDNESS<__U64_TYPE> { typedef __S64_TYPE SIGNED; typedef __U64_TYPE UNSIGNED; };
# endif
#endif //_MSC_VER


////////////////////////////////////////////////////////////////////////////////
template<class INT>
struct INTBASE
{
    typedef int     SIGNED;
    typedef uint    UNSIGNED;
};

template<> struct INTBASE<int64> { typedef int64   SIGNED;  typedef uint64  UNSIGNED; };
template<> struct INTBASE<uint64> { typedef int64   SIGNED;  typedef uint64  UNSIGNED; };

template<> struct INTBASE<ints> { typedef ints_to   SIGNED;  typedef uints_to  UNSIGNED; };
template<> struct INTBASE<uints> { typedef ints_to   SIGNED;  typedef uints_to  UNSIGNED; };

////////////////////////////////////////////////////////////////////////////////
inline bool valid_int_range( int64 v, uint bytes )
{
    int64 vmax = ((uint64)1<<(8*bytes-1)) - 1;
    int64 vmin = ~vmax;
    return v >= vmin  &&  v <= vmax;
}

inline bool valid_uint_range( uint64 v, uint bytes )
{
    uint64 vmax = ((uint64)1<<(8*bytes)) - 1;
    return v <= vmax;
}

////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_MSVC

#pragma warning ( disable : 4786 )      //identifier truncated

#   ifdef SYSTYPE_MSVC7plus

#       define typenamex typename
#       define TEMPLFRIEND
#       define DEDUCE_T_HACK(T)
#       define INDUCE_T_HACK(T)
#   else
#       define typenamex
#       define TEMPLFRIEND
#       define DEDUCE_T_HACK(T)    , T*
#       define INDUCE_T_HACK(T)    , (T*)0
#   endif

#else

#   define typenamex typename
#   define TEMPLFRIEND <>
#   define DEDUCE_T_HACK(T)
#   define INDUCE_T_HACK(T)

#endif



////////////////////////////////////////////////////////////////////////////////
///Structure used as argument to constructors that should not initialize the object
struct NOINIT_t
{
};

#define NOINIT      NOINIT_t()

////////////////////////////////////////////////////////////////////////////////
template <class T>
T* ptr_byteshift( T* p, ints b )
{
    return (T*) ((char*)p + b);
}


////////////////////////////////////////////////////////////////////////////////

void *_xmemcpy( void *dest, const void *src, size_t count );
#ifdef _DEBUG
#define xmemcpy     _xmemcpy
#else
#define xmemcpy     ::memcpy
#endif


///Find occurence of 0xcdcd in a two-part buffer
///@return true if not found (use with ASSERT)
bool cdcd_memcheck( const uchar* a, const uchar* ae, const uchar* b, const uchar* be );



COID_NAMESPACE_END


#include "net_ul.h"


#endif //__COID_COMM_COMMTYPES__HEADER_FILE__
