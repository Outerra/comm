#ifndef __COMM_LOGGER_H__
#define __COMM_LOGGER_H__

#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../str.h"
#include "../ref.h"
#include "../singleton.h"
#include "policy_log.h"
#include "../atomic/queue.h"

COID_NAMESPACE_BEGIN

class logger;
class logmsg;
DEFAULT_POLICY(logmsg, policy_log);

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
	logger* _logger;

public:
	void set_logger(logger* l) { _logger = l; }

	logger* get_logger() const { return _logger; }
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

private:
	bofstream _logfile;
	txtstream _logtxt;

public:
	logger(const token& filename)
	{
		_logfile.open(filename);
		_logtxt.bind(_logfile);
	}

public:
	void flush(const charstr& lm) { _logtxt<<lm; }

	ref<logmsg> operator () ()
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(this);
		return lm;
	}

	ref<logmsg> operator () (const ELogType t)
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(this);
		(*lm)<<type2tok(t);
		return lm;
	}

	ref<logmsg> operator () (const ELogType t, const char * fnc)
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(this);
		(*lm)<<type2tok(t)<<fnc<<' ';
		return lm;
	}

	ref<logmsg> operator () (const ELogType t, const char * fnc, const int line)
	{
		ref<logmsg> lm = logmsg::create();
		lm->set_logger(this);
		(*lm)<<type2tok(t)<<fnc<<'('<<line<<')'<<' ';
		return lm;
	}
};

COID_NAMESPACE_END

#endif // __COMM_LOGGER_H__
