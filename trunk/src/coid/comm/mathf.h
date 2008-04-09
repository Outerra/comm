
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

#ifndef __COID_COMM_MATHF__HEADER_FILE__
#define __COID_COMM_MATHF__HEADER_FILE__

#define _USE_MATH_DEFINES
#include <math.h>

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
inline float exp2f( float xx )
{
    return expf( xx * (float)M_LN2 );
}

////////////////////////////////////////////////////////////////////////////////
inline double exp2( double xx )
{
    return exp( xx * M_LN2 );
}

////////////////////////////////////////////////////////////////////////////////
inline float approx_fast_sqrt(float fx)
{
      float fret;
      __asm {

          mov eax,    fx
          sub eax,    0x3F800000
          sar eax,    1
          add eax,    0x3F800000
          mov [fret], eax
      }
      return fret;
}

////////////////////////////////////////////////////////////////////////////////
inline float approx_inv_sqrt (float x)
{
    float xhalf = 0.5f*x;
    int i = *(int*)&x;
    i = 0x5f3759df - (i >> 1); // This line hides a LOT of math!
    x = *(float*)&i;
    x = x*(1.5f - xhalf*x*x); // repeat this statement for a better approximation
    return x;
}

////////////////////////////////////////////////////////////////////////////////
inline long fast_ftol(float f)
{
    long x;
    float fx = floorf(f);
	_asm fld fx
	_asm fistp x
    return x;
}

////////////////////////////////////////////////////////////////////////////////
inline long fast_ftol(double f)
{
    long x;
    double fx = floor(f);
	_asm fld fx
	_asm fistp x
    return x;
}

COID_NAMESPACE_END


#endif //__COID_COMM_MATHF__HEADER_FILE__
