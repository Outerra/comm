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

#include "hptimer.h"

#ifndef SYSTYPE_MSVC

#include <sys/time.h>

namespace coid {


////////////////////////////////////////////////////////////////////////////////
struct timezone tz;

////////////////////////////////////////////////////////////////////////////////
void MsecTimer::reset()
{
    struct timeval ts;
    gettimeofday(&ts,&tz);
    _tstart = uint64(ts.tv_sec)*1000*1000 + ts.tv_usec;
}

////////////////////////////////////////////////////////////////////////////////
void MsecTimer::set( uint msec )
{
    struct timeval ts;
    gettimeofday(&ts,&tz);
    _tstart = uint64(ts.tv_sec)*1000*1000 + ts.tv_usec - uint64(msec)*1000;
}

////////////////////////////////////////////////////////////////////////////////
uint MsecTimer::time() const
{
    struct timeval ts;
    gettimeofday(&ts,&tz);

    uint64 t = uint64(ts.tv_sec)*1000*1000 + ts.tv_usec;
    return uint((t - _tstart) / 1000);
}

////////////////////////////////////////////////////////////////////////////////
uint MsecTimer::time_usec() const
{
    struct timeval ts;
    gettimeofday(&ts,&tz);

    return uint64(ts.tv_sec)*1000*1000 + ts.tv_usec - _tstart;
}

////////////////////////////////////////////////////////////////////////////////
uint MsecTimer::ticks() const
{
    struct timeval ts;
    gettimeofday(&ts,&tz);

    uint64 usec = uint64(ts.tv_sec)*1000*1000 + ts.tv_usec - _tstart;
    return uint( usec / _period );
}

////////////////////////////////////////////////////////////////////////////////
int MsecTimer::usec_to_tick( uint k ) const
{
    struct timeval ts;
    gettimeofday(&ts,&tz);

    uint64 usec = uint64(ts.tv_sec)*1000*1000 + ts.tv_usec - _tstart;
    return int( k*uint64(_period) - usec );
}

////////////////////////////////////////////////////////////////////////////////
uint64 get_hz( uint times = 4, uint msec = 50 )
{
    struct timeval tva, tvb;
    uint64 ca, cb, val=0;

    for( uint i=0; i<times; ++i )
    {
        gettimeofday( &tva, 0 );
        ca = cpu_ticks();

        sysMilliSecondSleep(msec);

        gettimeofday( &tvb, 0 );
        cb = cpu_ticks();

        uint64 d = cb - ca;
        uint db = tvb.tv_sec*1000000 + tvb.tv_usec - tva.tv_sec*1000000 - tva.tv_usec;  //microseconds
        
        val += (d*1000000) / db;
    }

    return val / times;
}

////////////////////////////////////////////////////////////////////////////////
HPTIMER::HPTIMER()
{
    _Irate = get_hz();
    _Isttime = cpu_ticks();
}

} // namespace coid


#endif //SYSTYPE_MSVC
