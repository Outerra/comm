
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
 * Ladislav Hrabcak
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
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

#ifndef __COMM_LOGGER_H__
#define __COMM_LOGGER_H__

#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../str.h"
#include "../ref.h"
#include "../singleton.h"
#include "policy_log.h"
#include "../atomic/queue.h"
#include "../atomic/pool.h"

namespace coid {
	class logmsg;
};
DEFAULT_POLICY(coid::logmsg,policy_queue_pooled);


COID_NAMESPACE_BEGIN

class logger;

class logger_file
{
	bofstream _logfile;
	txtstream _logtxt;

public:
	logger_file(const token& filename)
	{
		_logfile.open(filename);
		_logtxt.bind(_logfile);
	}

	void write_to_file(const charstr& lm) { _logtxt<<lm; }
};

typedef ref<logger_file> logger_file_ptr;

/* 
 * Log message object, returned by the logger.
 * Message is written in policy_log::release() upon calling destructor of ref
 * with policy policy_log.
 */
class logmsg : public charstr
{
protected:
	logger_file_ptr _lf;
	int _type;

public:
	void set_log_file(logger_file_ptr& lf,const int t) { _lf=lf; _type=t; }

	void reset() { coid::charstr::reset(); _lf.release(); }

	void write_to_file() { _lf->write_to_file(*this); }

	int get_type() const { return _type; }

	void set_type(const int t) { _type=t; }
};

//DEFAULT_POLICY(logmsg,policy_queue_pooled);
typedef ref<logmsg> logmsg_ptr;

/* 
 * USAGE :
 *
 * class fwmlogger : public logger
 * {
 * public:
 *     fwmlogger() : logger("fwm.log") {}
 * };
 * #define fwmlog(msg) (*SINGLETON(fwmlogger)()<<msg)
 * 
 * USAGE:
 * 
 * fwmlog("ugh" << x << "asd")
 */
class logger
{
public:
	enum ELogType {
		Exception=0,
		Error,
		Warning,
		Info,
		Debug,
		Last
	};

	class logmsg_local
	{
	protected:
		logmsg_ptr _lm;

	public:
		logmsg_local(logger_file_ptr& lf,const ELogType t) : _lm(CREATE_ME) { _lm->set_log_file(lf,t); }

		logmsg_local() : _lm() {}

		~logmsg_local();

		template<class T> charstr& operator<<(const T& o) { DASSERT(!_lm.is_empty()); return (*_lm)<<(o); }
	};

public:

    const token& type2tok( const ELogType t )
	{
		static token st[]={
			"EXCEPTION: ",
			"ERROR: ",
			"WARNING: ",
			"INFO: ",
			"DEBUG: ",
		};
		static token ud="userdefined";
		
		if (t<Last) return st[t];
		return ud;
	}

protected:
	logger_file_ptr _logfile;

public:
	logger(const token& filename);

public:
	logmsg_local operator()() {
		logmsg_local lm(_logfile,Info);
		return lm;
	}

	logmsg_local operator()(const ELogType t) {
		logmsg_local lm(_logfile,t);
		lm<<type2tok(t);
		return lm;
	}

	logmsg_local operator()( const ELogType t,const char* fnc ) {
		logmsg_local lm(_logfile,t);
		lm<<type2tok(t)<<fnc<<' ';
		return lm;
	}

	logmsg_local operator()( const ELogType t,const char* fnc,const int line ) {
		logmsg_local lm(_logfile,t);
		lm<<type2tok(t)<<fnc<<'('<<line<<')'<<' ';
		return lm;
	}
};

COID_NAMESPACE_END

#endif // __COMM_LOGGER_H__