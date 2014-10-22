
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
 * Brano Kemen
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef __COID_COMM_MATHI__HEADER_FILE__
#define __COID_COMM_MATHI__HEADER_FILE__

#include "commtypes.h"

COID_NAMESPACE_BEGIN


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

///Return next power-of-2 number higher or equal to x (macro form for enums)
#define NEXTPOW2_32(x) NEXTPOW2_32_((x-1))
#define NEXTPOW2_32_(x) ((x|(x>>1)|(x>>2)|(x>>4)|(x>>8)|(x>>16))+1)

///Return next power-of-2 number higher or equal to x (macro form for enums)
#define NEXTPOW2_64(x) NEXTPOW2_64_((x-1))
#define NEXTPOW2_64_(x) ((x|(x>>1)|(x>>2)|(x>>4)|(x>>8)|(x>>16)|(x>>32))+1)

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

//@return 2's exponent of nearest lower power of two number
inline uchar int_low_pow2( uints x )        { uchar i;  for(i=0; x; ++i) x>>=1;  return i-1; }

//@return 2's exponent of nearest higher power of two number
inline uchar int_high_pow2( uints x )       { uchar i; --x; for(i=0; x; ++i) x>>=1;  return i; }

/// Checks if a value is power of two
#define IS_2_POW_N(_X)   (((_X)&(_X-1)) == 0)


/// Align value to the nearest greater multiplier of specified chunk size
inline uint align_value( uint val, uint size )
{ return ((val+size-1)/size)*size; }

inline uint64 align_value( uint64 val, uint64 size )
{ return ((val+size-1)/size)*size; }

inline uints align_offset( uints val, uints size )
{ return ((val+size-1)/size)*size; }



/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uints align_to_chunks( uints uval, uints usize )
{ return uints((uval+usize-1)/usize); }

/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uint64 align_to_chunks64( uint64 uval, uint64 usize )
{ return (uval+usize-1)/usize; }


/// Aligns value to nearest multiplier of 2 pow rsize chunk size
inline uints align_value_to_power2( uints uval, uchar rsize )
{ return uints((uval+(uints(1)<<rsize)-1) &~ ((uints(1)<<rsize)-1)); }

/// Aligns value to nearest multiplier of 2 pow rsize chunk size
inline uint64 align_value_to_power2_64( uint64 uval, uchar rsize )
{ return (uval+(uint64(1)<<rsize)-1) &~ ((uint64(1)<<rsize)-1); }


/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uints align_to_chunks_pow2( uints uval, uchar rsize )
{ return uints((uval+((uints(1)<<rsize)-1))>>rsize); }

/// Aligns value to given chunk size (enlarges to next chunk boundary)
inline uint64 align_to_chunks_pow2_64( uint64 uval, uchar rsize )
{ return (uval+((uint64(1)<<rsize)-1))>>rsize; }


////////////////////////////////////////////////////////////////////////////////
template< class INT >
inline INT int_min( INT a, INT b )      { return a<b ? a : b; }

template< class INT >
inline INT uint_min( INT a, INT b )     { return a<b ? a : b; } // b + ((a - b) & -(a < b));

////////////////////////////////////////////////////////////////////////////////
template< class INT >
inline INT int_max( INT a, INT b )      { return a<b ? b : a; }

template< class INT >
inline INT uint_max( INT a, INT b )     { return a<b ? b : a; } // a - ((a - b) & -(a < b));

////////////////////////////////////////////////////////////////////////////////
///Absolute value of int without branching
template< class INT >
inline INT int_abs( INT a )
{
    typedef typename SIGNEDNESS<INT>::SIGNED SINT;
    return a - ((a+a) & ((SINT)a>>(sizeof(INT)*8-1)));
}

//@return a<0 ? onminus : onplus
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


////////////////////////////////////////////////////////////////////////////////
/// fast modulo-3, nearly 7 times faster than x%3
inline unsigned long mod3(unsigned int x) {
    x *= 0xAAAAAAAB;
    if ((x - 0x55555556) < 0x55555555) return 2;
    return x >> 31;
}

/// test whether \a x module 3 equals 0
inline int ismod3eq0(unsigned int x) {
    x *= 0xAAAAAAAB;
    return (x < 0x55555556);
}


COID_NAMESPACE_END


#endif //__COID_COMM_MATHI__HEADER_FILE__
