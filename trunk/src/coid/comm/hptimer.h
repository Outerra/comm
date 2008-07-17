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

#ifndef __COID_COMM_HPTIMER__HEADER_FILE__
#define __COID_COMM_HPTIMER__HEADER_FILE__

#include "namespace.h"

#include "commtypes.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
#define	CCPU_I32TO64(high, low)	(((int64)high<<32)+low)

#ifdef _MSC_VER
inline __declspec( naked ) int64 cpu_ticks() {
    __asm {
        rdtsc
        ret
    }
}
#else
inline uint64 cpu_ticks() {
    unsigned int ra;
    unsigned int rd;
    asm (   "rdtsc"
            : "=a" (ra), "=d" (rd)
            : /*no inputs*/ );
    return CCPU_I32TO64(rd,ra);
}
#endif

////////////////////////////////////////////////////////////////////////////////
class HPTIMER
{
public:

    static const char* class_name() { return "HPTIMER"; }

    HPTIMER();

    void reset() {
        _Isttime = cpu_ticks();
    }

    uint time() const {
        uint64 Ietm;
        Ietm = cpu_ticks();
        return (uint) (((Ietm-_Isttime)<<10)/_Irate);    //in 1/1024ths of second
    }

    double ftime() const {
        int64 Ietm;
        Ietm = cpu_ticks();
        return (double) (Ietm-(int64)_Isttime)/((double)(int64)_Irate);    //in seconds
    }

    operator uint(void) const {
        return time();
    }

    operator double(void) const {
        return ftime();
    }

private:
    uint64 _Isttime;
    uint64 _Irate;
};


////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_WIN32

extern "C" {
    __declspec(dllimport) uint __stdcall timeBeginPeriod( uint );
    __declspec(dllimport) uint __stdcall timeEndPeriod( uint );
    __declspec(dllimport) ulong __stdcall timeGetTime();
}

#endif


class MsecTimer
{
    uint64 _tstart;
    uint _period;

public:
    MsecTimer()             { _tstart = 0; _period = 1000; }
    ~MsecTimer()            {}

    void set_period_usec( uint usec )   { _period = usec; }
    uint get_period_usec() const        { return _period; }

#ifdef SYSTYPE_WIN32
    static void init()      { timeBeginPeriod(1); }
    static void term()      { timeEndPeriod(1); }
#else
    static void init()      {}
    static void term()      {}
#endif

    void reset();
    void set( uint msec );

    uint time() const;
    uint time_usec() const;
	uint time_reset();

    uint ticks() const;
    int usec_to_tick( uint k ) const;
};

////////////////////////////////////////////////////////////////////////////////

COID_NAMESPACE_END


#endif //#ifndef __COID_COMM_HPTIMER__HEADER_FILE__
