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

#ifndef __COID_COMM_COMM__HEADER_FILE__
#define __COID_COMM_COMM__HEADER_FILE__

//#include <new>
#include "namespace.h"

#include "commtypes.h"
//#include <stddef.h>


COID_NAMESPACE_BEGIN

uints memaligned_used();
void * memaligned_alloc( size_t size, size_t alignment );
void * memaligned_realloc( void * memblock, size_t size, size_t alignment );
void   memaligned_free( void * memblock );



/*
#ifdef _MSC_VER
#define NEWTHROWTYPE
#define DELTHROWTYPE
#else
#define NEWTHROWTYPE    throw (std::bad_alloc)
#define DELTHROWTYPE    throw ()
#endif


void * operator new (size_t size) NEWTHROWTYPE;
void operator delete (void * ptr) DELTHROWTYPE;
void * operator new[] (size_t size) NEWTHROWTYPE;
void operator delete[] (void *ptr) DELTHROWTYPE;
*/

////////////////////////////////////////////////////////////////////////////////
/// Check if all set bits of \a b are also set in \a a
inline bool bits_set (long a, long b)          { return (a&b) == b; }

/// Efficiently returns the least power of two greater than or equal to X
template< class UINT >
inline UINT nextpow2( UINT x )
{
    if(!x)  return 1;
    --x;
    for( uint n=1; n<sizeof(x)*8; n<<=1 )
        x |= x>>n;
    return ++x;
}

template<>
inline uints nextpow2<uints>( uints x )
{
    if(!x)  return 1;
    --x;
    for( uint n=1; n<sizeof(x)*8; n<<=1 )
        x |= x>>n;
    return ++x;
}


inline uchar getpow2( uints x )     { uchar i;  for( i=0; x; ++i,x>>=1 );  return i-1; }

/// Checks if a value is power of two
#define IS_2_POW_N(X)   (((X)&(X-1)) == 0)

template<class INT>
inline typename INTBASE<INT>::UNSIGNED align_value( INT uval, typename INTBASE<INT>::UNSIGNED usize )
{ return typename INTBASE<INT>::UNSIGNED( ((uval-1)/usize)*usize +usize ); }


/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uint align_to_chunks( uint uval, uint usize )
{ return (uval+usize-1)/usize; }

/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uint64 align_to_chunks64( uint64 uval, uint64 usize )
{ return (uval+usize-1)/usize; }


/// Aligns value to nearest multiplier of 2 pow rsize chunk size
inline uint align_value_to_power2( uint uval, uint rsize )
{ return (uval+(1<<rsize)-1) &~ ((1UL<<rsize)-1); }

/// Aligns value to nearest multiplier of 2 pow rsize chunk size
inline uint64 align_value_to_power2_64( uint64 uval, uint rsize )
{ return (uval+((uint64)1<<rsize)-1) &~ (((uint64)1<<rsize)-1); }


/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uint align_to_chunks_pow2( uint uval, uint rsize )
{ return (uval+((1<<rsize)-1))>>rsize; }

/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uint64 align_to_chunks_pow2_64( uint64 uval, uint rsize )
{ return (uval+(((uint64)1<<rsize)-1))>>rsize; }


////////////////////////////////////////////////////////////////////////////////
template< class INT >
inline INT int_min( INT a, INT b )
{
    b = b-a;            // b>a => MSB(o)==1 => (o>>31) == UMAX
    a += b & ((typename SIGNEDNESS<INT>::SIGNED)b>>(sizeof(INT)*8-1));
    return a;
}

template< class INT >
inline INT uint_min( INT a, INT b )
{
    return  b + ((a - b) & -(a < b));
}

////////////////////////////////////////////////////////////////////////////////
template< class INT >
inline INT int_max( INT a, INT b )
{
    b = a-b;
    a -= b & ((typename SIGNEDNESS<INT>::SIGNED)b>>(sizeof(INT)*8-1));
    return a;
}

template< class INT >
inline INT uint_max( INT a, INT b )
{
    return  a - ((a - b) & -(a < b));
}

////////////////////////////////////////////////////////////////////////////////
template< class INT >
inline typename SIGNEDNESS<INT>::UNSIGNED int_abs( INT a )
{
    return typename SIGNEDNESS<INT>::UNSIGNED( a - ((a+a) & ((typename SIGNEDNESS<INT>::SIGNED)a>>(sizeof(INT)*8-1))) );
}

///@return a<0 ? onminus : onplus
template< class INT >
inline INT int_select_by_sign( INT a, INT onplus, INT onminus )
{
    return (((typename SIGNEDNESS<INT>::SIGNED)a>>(sizeof(INT)*8-1))&(onminus-onplus)) + onplus;
}

///change sign of val when pattern is negative
template< class INT >
inline INT int_change_sign( INT pattern, INT val )
{
    return val - (((typename SIGNEDNESS<INT>::SIGNED)pattern>>(sizeof(INT)*8-1)) & (val+val));
}

////////////////////////////////////////////////////////////////////////////////
///Always return unsigned modulo, a difference between the \a v number and its nearest
/// lower multiple of \a m 
template< class INT >
inline typename SIGNEDNESS<INT>::UNSIGNED int_umod( INT v, INT m )
{
    INT r = v%m;
    return typename SIGNEDNESS<INT>::UNSIGNED( int_select_by_sign(r,r,m+r) );   // r<0 ? m+r : r
}

///Divide nearest lower multiple of \a m of given number \a v by \a m
template< class INT >
inline INT int_udiv( INT v, typename SIGNEDNESS<INT>::UNSIGNED m )
{
    INT r = v/m;
    return int_select_by_sign( v,r,r-1 );   //v<0  ?  r-1  :  r;
}

COID_NAMESPACE_END

#endif // __COID_COMM_COMM__HEADER_FILE__
