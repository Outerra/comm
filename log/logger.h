#ifndef __COMM_LOGGER_H__
#define __COMM_LOGGER_H__

#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../str.h"
#include "../ref.h"
#include "../SINGLETON.h"
#include "policy_log.h"

COID_NAMESPACE_BEGIN

class logger;
class logmsg;
DEFAULT_POLICY(logmsg, policy_log);

/* 
 * returned by logger 
 * message is writen in policy_log::release() when destructor is called
 */
class logmsg 
	: public coid::charstr
	, public pooled<logmsg>
{
protected:
	logger * _logger;

public:
	void set_logger(logger * const l) { _logger = l; }

	logger * get_logger() const { return _logger; }
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
 * fwmlog("ugh")
 */
class logger
{
public:
	enum ELogType {
		Warning = 0,
		Info,
		Error,
	};

private:
	coid::bofstream _logfile;
	coid::txtstream _logtxt;

public:
	logger(const coid::token& filename)
	{
		_logfile.open(filename);
		_logtxt.bind(_logfile);
	}

public:
	void flush(const coid::charstr & lm) { _logtxt<<lm; }

	ref<logmsg> operator () ()
	{
		ref<logmsg> lm=logmsg::create();
		lm->set_logger(this);
		return lm;
	}
};

COID_NAMESPACE_END

#endif // __COMM_LOGGER_H__