
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
#include "../binstream/txtcachestream.h"
#include "../str.h"
#include "../ref.h"
#include "../singleton.h"
#include "policy_log.h"
#include "../atomic/queue.h"

COID_NAMESPACE_BEGIN

class logger;
class logmsg;
DEFAULT_POLICY(logmsg, policy_log);

class logger_file : public ref_base
{
	bofstream _logfile;
	txtcachestream _logtxt;

public:
	logger_file(const token& filename)
	{
		_logfile.open(filename);
		_logtxt.bind(_logfile);
	}

	void flush(const charstr& lm) { _logtxt<<lm; }
};

/* 
 * Log message object, returned by the logger.
 * Message is written in policy_log::release() upon calling destructor of ref
 * with policy policy_log.
 */
class logmsg 
	: public charstr
	, public pooled<logmsg>
	, public atomic::queue<logmsg>::node_t
{
protected:
	ref<logger_file> _logger;

public:
	void set_logger(ref<logger_file> & lf) { _logger = lf; }

	const ref<logger_file>& get_logger() const { return _logger; }

	void reset()
	{
		coid::charstr::reset();
		_logger=0;
	}
};

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
		Info=0,
		Warning,
		Exception,
		Error,
		Debug,
		Last
	};

    const token& type2tok( const ELogType t )
	{
		static token st[]={
			"INFO: ",
			"WARNING: ",
			"EXCEPTION: ",
			"ERROR: ",
			"DEBUG: ",
		};
		static token ud="userdefined";
		
		if (t<Last) return st[t];
		return ud;
	}

protected:
	ref<logger_file> _logfile;

public:
	logger(const token& filename);

public:
	//void flush(const charstr& lm) { _logfile->flush(lm); }

	ref<logmsg> operator () ()
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(_logfile);
		return lm;
	}

	ref<logmsg> operator () (const ELogType t)
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(_logfile);
		(*lm)<<type2tok(t);
		return lm;
	}

	ref<logmsg> operator () (const ELogType t, const char * fnc)
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(_logfile);
		(*lm)<<type2tok(t)<<fnc<<' ';
		return lm;
	}

	ref<logmsg> operator () (const ELogType t, const char * fnc, const int line)
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(_logfile);
		(*lm)<<type2tok(t)<<fnc<<'('<<line<<')'<<' ';
		return lm;
	}
};

COID_NAMESPACE_END

#endif // __COMM_LOGGER_H__
