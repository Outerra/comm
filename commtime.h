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


#ifndef __COID_COMM_TIME__HEADER_FILE__
#define __COID_COMM_TIME__HEADER_FILE__

#include "namespace.h"
#include <ctime>

#ifdef SYSTYPE_MINGW
time_t timelocal (struct tm *tm);
time_t timegm (struct tm *tm);

#ifndef _TIMESPEC_DEFINED
struct timespec {
    long tv_sec;
    long tv_nsec;
};
#endif
#endif

COID_NAMESPACE_BEGIN

///Time in seconds
struct timet
{
    int64  t;

    timet()                         { set_now(); }
    timet( time_t tx )              { t = tx; }

    operator time_t () const        { return (time_t)t; }

    timet& operator = ( time_t tx ) { t = tx;  return *this; }

    /// @return difference in seconds
    int64 diff( time_t tx ) const   { return t - tx; }

    ///Set to current time
    timet& set_now()
    {
#ifdef SYSTYPE_WIN
        t = ::_time64(0);
#else
        t = (int64) ::time(0);
#endif
        return *this;
    }

    /// @return current time
    static timet now()
    {
        timet t;
        t.set_now();
        return t;
    }

    static timet last()
    {
        timet t;
        t.t = INT64_MAX;
        return t;
    }

    /// @return current time (alias for now())
    static timet current() {
        return now();
    }
};


COID_NAMESPACE_END

#endif //__COID_COMM_TIME__HEADER_FILE__
