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

#ifndef __COID_COMM_MUTEX__HEADER_FILE__
#define __COID_COMM_MUTEX__HEADER_FILE__

#include "../namespace.h"

#include "../comm.h"
#include "../assert.h"

#include "../pthreadx.h"
#include <sys/timeb.h>

#include "guard.h"
#include "_mutex.h"

#ifdef _DEBUG
#	include <list>
#endif


COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
class comm_mutex : public _comm_mutex
{
public:
#ifdef _DEBUG
	const char* _name;
	uint       _objid;
	uint       _locktime;
#endif


public:
    void lock ();
    void unlock ();

    bool try_lock();

//    bool timed_lock( uint msec );

    comm_mutex( bool recursive = true, const char * name=NULL );
    explicit comm_mutex( NOINIT_t );

    ~comm_mutex();

    void init( bool recursive = true );

    void rd_lock()                      { lock(); }
    void wr_lock()                      { lock(); }

    bool try_rd_lock()                  { return try_lock(); }
    bool try_wr_lock()                  { return try_lock(); }

    //bool timed_rd_lock( uint msec )     { return timed_lock(msec); }
    //bool timed_wr_lock( uint msec )     { return timed_lock(msec); }


    static void get_abstime( uint delaymsec, timespec* out )
    {
        struct ::timeb tb;
        ftime(&tb);

        out->tv_sec = delaymsec/1000 + (uint)tb.time;
        out->tv_nsec = (delaymsec%1000 + tb.millitm) * 1000000;
    }

#ifdef _DEBUG
	void set_objid( uint id )           { _objid = id; }
	uint get_objid() const              { return _objid; }
	void set_name( const char * name )  { _name = name; }
	const char * get_name() const       { return _name; }
	time_t get_locktime() const         { return _locktime; }
#else
	void set_objid( uint id ) {}
	void set_name( const char * ) {}
#endif
};


#ifdef _DEBUG

////////////////////////////////////////////////////////////////////////////////
struct MX_REGISTER
{
	_comm_mutex       _mx;
	std::list<const void *> _reg;
	std::list<const void *>::iterator _it;

	void add( const comm_mutex * m ) {
		comm_mutex_guard<_comm_mutex> mxg( _mx );
		_reg.push_back( (const void *) m );
	}
	void del( const comm_mutex * m ) {
		comm_mutex_guard<_comm_mutex> mxg( _mx );
        _reg.remove( (const void *) m );
	}
};


////////////////////////////////////////////////////////////////////////////////
struct MX_REGISTER_ITERATOR
{
	MX_REGISTER & _reg;
	std::list<const void *>::iterator _it;


	MX_REGISTER_ITERATOR( MX_REGISTER & reg ) : _reg(reg)
	{
		_reg._mx.lock();
		reset();
	}
	~MX_REGISTER_ITERATOR() {_reg._mx.unlock();}

	void reset() {_it = _reg._reg.begin();}
	const comm_mutex * next() {
		_it++;
		if( _it == _reg._reg.end() ) return NULL;
		return (const comm_mutex *) (*_it);
	}
	uint size() const                   {return (uint)_reg._reg.size();}
};


#define MUTEX_ITERATOR(mxit)    MX_REGISTER_ITERATOR  mxit(SINGLETON(MX_REGISTER))


#endif	// _DEBUG



#define MXGUARD(mut)                    comm_mutex_guard<comm_mutex> ___mxg (mut)
#define MXGUARD_TIMED(mut,dly)          comm_mutex_guard<comm_mutex> ___mxg (mut, dly)

#define MXGUARDRW(mut)                  comm_mutex_guard<comm_mutex_rw> ___mxg (mut)
#define MXGUARDRW_TIMED(mut,dly)        comm_mutex_guard<comm_mutex_rw> ___mxg (mut, dly)

#define MXGUARDREG(mut)                 comm_mutex_guard<comm_mutex_eg::refmutex> ___mxg (mut)
#define MXGUARDREG_TIMED(mut,dly)       comm_mutex_guard<comm_mutex_eg::refmutex> ___mxg (mut, dly)


COID_NAMESPACE_END


#endif // __COID_COMM_MUTEX__HEADER_FILE__
