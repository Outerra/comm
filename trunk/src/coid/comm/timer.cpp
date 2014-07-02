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
 * Brano Kemen.
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

#include "timer.h"


#ifdef SYSTYPE_MSVC
#include <windows.h>


#pragma comment(lib, "winmm.lib")

namespace coid {

////////////////////////////////////////////////////////////////////////////////
nsec_timer::nsec_timer()
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	_freq = freq.QuadPart;
	_freqd = 1.0 / double(_freq);

    reset();
}

////////////////////////////////////////////////////////////////////////////////
void nsec_timer::reset()
{
	QueryPerformanceCounter((LARGE_INTEGER*)&_start);
    _dtns = 0;
}

////////////////////////////////////////////////////////////////////////////////
double nsec_timer::time()
{
	return time_ns() * 1e-9;
}

////////////////////////////////////////////////////////////////////////////////
uint64 nsec_timer::time_ns()
{
	LARGE_INTEGER stop;
	QueryPerformanceCounter(&stop);

    uint64 d = stop.QuadPart - _start;
    uint64 w = d / _freq;
    uint64 f = d % _freq;

    static const uint64 NS = 1000000000;

    return w*NS + (f*NS)/_freq + _dtns;
}

////////////////////////////////////////////////////////////////////////////////















////////////////////////////////////////////////////////////////////////////////
void msec_timer::reset()
{
    _tstart = timeGetTime();
}

////////////////////////////////////////////////////////////////////////////////
void msec_timer::set( uint msec )
{
    _tstart = timeGetTime() - (uint64)msec;
}

////////////////////////////////////////////////////////////////////////////////
uint msec_timer::time() const
{
    uint tend = timeGetTime();
    return uint(tend - _tstart);
}

////////////////////////////////////////////////////////////////////////////////
uint msec_timer::time_reset()
{
	uint tend = timeGetTime();
	uint t = uint(tend - _tstart);
	_tstart = tend;
	return t;
}

////////////////////////////////////////////////////////////////////////////////
uint msec_timer::time_usec() const
{
    uint tend = timeGetTime();
    return uint( 1000 * (tend - _tstart) );
}

////////////////////////////////////////////////////////////////////////////////
uint msec_timer::ticks() const
{
    uint64 tend = timeGetTime();
    return uint( (1000 * (tend - _tstart)) / _period );
}

////////////////////////////////////////////////////////////////////////////////
int msec_timer::usec_to_tick( uint k ) const
{
    uint64 tend = timeGetTime() - _tstart;
    return int( k*(int64)_period - tend*1000 );
}

////////////////////////////////////////////////////////////////////////////////
uint msec_timer::get_time() {
	return timeGetTime();
}

} // namespace coid

#endif //SYSTYPE_MSVC
