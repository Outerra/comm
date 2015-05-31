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

#include "mutex.h"
#include "../retcodes.h"
#include "../pthreadx.h"


#ifdef _DEBUG

#   ifdef SYSTYPE_WIN
//#	    include <windows.h>
#   endif

#	include "../singleton.h"
#	include "../timer.h"
#endif


namespace coid {


#ifdef _DEBUG
static msec_timer& get_msec_timer()
{
    static msec_timer _mst;
    return _mst;
}
#endif

////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
comm_mutex::comm_mutex( uint spincount, bool recursive, const char * name)
    : _comm_mutex(spincount, recursive)
{
	SINGLETON(MX_REGISTER).add( this );
	_locktime = 0;
	_objid = UMAX32;
    _name = name;
}
#else
comm_mutex::comm_mutex( uint spincount, bool recursive, const char *)
    : _comm_mutex(spincount, recursive)
{
}
#endif

////////////////////////////////////////////////////////////////////////////////
comm_mutex::comm_mutex( NOINIT_t n ) : _comm_mutex(n)
{
#ifdef _DEBUG
	SINGLETON(MX_REGISTER).add( this );
	_locktime = 0;
	_name = NULL;
    _objid = UMAX32;
#endif
}

////////////////////////////////////////////////////////////////////////////////
comm_mutex::~comm_mutex()
{
#ifdef _DEBUG
	SINGLETON(MX_REGISTER).del( this );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void comm_mutex::lock()
{
    _comm_mutex::lock();

#ifdef _DEBUG
	_locktime = get_msec_timer().time();
#endif
}

////////////////////////////////////////////////////////////////////////////////
void comm_mutex::unlock()
{
#ifdef _DEBUG
	_locktime = 0;
#endif
    _comm_mutex::unlock();
}

////////////////////////////////////////////////////////////////////////////////
bool comm_mutex::try_lock()
{
#ifdef _DEBUG
	bool r = _comm_mutex::try_lock();
    if(r)
		_locktime = get_msec_timer().time();
    return r;
#else
    return _comm_mutex::try_lock();
#endif
}

/*
////////////////////////////////////////////////////////////////////////////////
bool comm_mutex::timed_lock (uint delaymsec)
{
//#ifdef _MSC_VER
#ifndef SYSTYPE_CYGWIN
    timespec ts;
    get_abstime( delaymsec, &ts );
    return 0 == pthread_mutex_timedlock( &_mutex, &ts );
#else
    return false;
#endif
}
*/

} // namespace coid
