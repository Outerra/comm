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

#include "hptimer.h"


#ifdef SYSTYPE_MSVC
#include <windows.h>


#pragma comment(lib, "winmm.lib")

namespace coid {


////////////////////////////////////////////////////////////////////////////////
void MsecTimer::reset()
{
    _tstart = timeGetTime();
}

////////////////////////////////////////////////////////////////////////////////
void MsecTimer::set( uint msec )
{
    _tstart = timeGetTime() - (uint64)msec;
}

////////////////////////////////////////////////////////////////////////////////
uint MsecTimer::time() const
{
    uint tend = timeGetTime();
    return uint(tend - _tstart);
}

////////////////////////////////////////////////////////////////////////////////
uint MsecTimer::time_reset()
{
	uint tend = timeGetTime();
	uint t = uint(tend - _tstart);
	_tstart = tend;
	return t;
}

////////////////////////////////////////////////////////////////////////////////
uint MsecTimer::time_usec() const
{
    uint tend = timeGetTime();
    return uint( 1000 * (tend - _tstart) );
}

////////////////////////////////////////////////////////////////////////////////
uint MsecTimer::ticks() const
{
    uint64 tend = timeGetTime();
    return uint( (1000 * (tend - _tstart)) / _period );
}

////////////////////////////////////////////////////////////////////////////////
int MsecTimer::usec_to_tick( uint k ) const
{
    uint64 tend = timeGetTime() - _tstart;
    return int( k*(int64)_period - tend*1000 );
}


////////////////////////////////////////////////////////////////////////////////
typedef	void	(*CCPU_FUNC)(ulong param);


#pragma warning ( push )
#pragma warning ( disable : 4035 )

static	int64	GetCyclesDifference(CCPU_FUNC func, ulong param)
{
	unsigned	int	r_edx1, r_eax1;
	unsigned	int	r_edx2, r_eax2;

	// Calculation
	__asm {
		push	param			;push parameter param
		mov	ebx, func	;store func in ebx

		_emit	0Fh			;RDTSC
		_emit	31h

		mov	esi, eax		;esi=eax
		mov	edi, edx		;edi=edx

		call	ebx			;call func

		_emit	0Fh			;RDTSC
		_emit	31h

		pop	ebx

		mov	r_edx2, edx	;r_edx2=edx
		mov	r_eax2, eax	;r_eax2=eax

		mov	r_edx1, edi	;r_edx2=edi
		mov	r_eax1, esi	;r_eax2=esi
	}

	return(CCPU_I32TO64(r_edx2, r_eax2) - CCPU_I32TO64(r_edx1, r_eax1));
}

#pragma warning ( pop )


static	void	_Delay(ulong ms) {
	LARGE_INTEGER	freq, c1, c2;
	int64			x;

	if (!QueryPerformanceFrequency(&freq))
		return;
	x = freq.QuadPart/1000*ms;

	QueryPerformanceCounter(&c1);
	do
	{
		QueryPerformanceCounter(&c2);
	}while(c2.QuadPart-c1.QuadPart < x);
}

static	void	_DelayOverhead(ulong ms)
{
	LARGE_INTEGER	freq, c1, c2;
	__int64			x;

	if (!QueryPerformanceFrequency(&freq))
		return;
	x = freq.QuadPart/1000*ms;

	QueryPerformanceCounter(&c1);
	do
	{
		QueryPerformanceCounter(&c2);
	}while(c2.QuadPart-c1.QuadPart == x);
}

////////////////////////////////////////////////////////////////////////////////
//
// Returns the MHz of the CPU
//
// Parameter             Description
// ------------------    ---------------------------------------------------
//    uTimes             The number of times to run the test. The function
//                       runs the test this number of times and returns the
//                       average. Defaults to 1.
//    uMsecPerTime       Milliseconds each test will run. Defaults to 50.
//    nThreadPriority    If different than THREAD_PRIORITY_ERROR_RETURN,
//                       it will set the current thread's priority to
//                       this value, and will restore it when the tests
//                       finish. Defaults to THREAD_PRIORITY_TIME_CRITICAL.
//    dwPriorityClass    If different than 0, it will set the current
//                       process's priority class to this value, and will
//                       restore it when the tests finish.
//                       Defaults to REALTIME_PRIORITY_CLASS.
//
// Notes
// -------------------------------------------------------------------------
// 1. The default parameter values should be ok.
//    However, the result may be wrong if (for example) the cache
//    is flushing to the hard disk at the time of the test.
// 2. Requires a Pentium+ class processor (RDTSC)
// 3. Requires support of high resolution timer. Most (if not all) Windows
//    machines are ok.
//
int64 get_hz(
    ulong uTimes=4,
    ulong uMsecPerTime=50,
    int nThreadPriority=15/*THREAD_PRIORITY_TIME_CRITICAL*/,
    ulong dwPriorityClass=0x00000100/*REALTIME_PRIORITY_CLASS*/
    )
{
	int64	total=0, overhead=0;

	if (!uTimes || !uMsecPerTime)
		return(0);

	ulong		dwOldPC;												// old priority class
	int		nOldTP;												// old thread priority

	// Set Process Priority Class
	if (dwPriorityClass != 0)									// if it's to be set
	{
		HANDLE	hProcess = GetCurrentProcess();			// get process handle

		dwOldPC = GetPriorityClass(hProcess);				// get current priority
		if (dwOldPC != 0)											// if valid
			SetPriorityClass(hProcess, dwPriorityClass);	// set it
		else															// else
			dwPriorityClass = 0;									// invalidate
	}
	// Set Thread Priority
	if (nThreadPriority != THREAD_PRIORITY_ERROR_RETURN)	// if it's to be set
	{
		HANDLE	hThread = GetCurrentThread();				// get thread handle

		nOldTP = GetThreadPriority(hThread);				// get current priority
		if (nOldTP != THREAD_PRIORITY_ERROR_RETURN)		// if valid
			SetThreadPriority(hThread, nThreadPriority);	// set it
		else															// else
			nThreadPriority = THREAD_PRIORITY_ERROR_RETURN;	// invalidate
	}

	for(ulong i=0; i<uTimes; i++)
	{
		total += GetCyclesDifference(_Delay, uMsecPerTime);
		overhead += GetCyclesDifference(_DelayOverhead, uMsecPerTime);
	}

	// Restore Thread Priority
	if (nThreadPriority != THREAD_PRIORITY_ERROR_RETURN)	// if valid
		SetThreadPriority(GetCurrentThread(), nOldTP);		// set the old one
	// Restore Process Priority Class
	if (dwPriorityClass != 0)										// if valid
		SetPriorityClass(GetCurrentProcess(), dwOldPC);		// set the old one


	total -= overhead;
	total /= uTimes;
	total *= 1000;
	total /= uMsecPerTime;

	return total;
}


////////////////////////////////////////////////////////////////////////////////
HPTIMER::HPTIMER()
{
    _Irate = get_hz();
    _Isttime = cpu_ticks();
}


#undef	CCPU_I32TO64

} // namespace coid

#endif //SYSTYPE_MSVC
