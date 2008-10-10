#ifndef __COMM_LOGGER_H__
#define __COMM_LOGGER_H__

#include "../binstream/filestream.h"
#include "../binstream/txtstream.h"
#include "../str.h"
#include "../ref.h"
#include "../SINGLETON.h"
#include "policy_log.h"
#include "../atomic/queue.h"

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
	, public atomic::queue<logmsg>::node_t
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
		Info=0,
		Warning,
		Exception,
		Error,
		Debug,
		Last
	};

	coid::token & type2tok(const ELogType t)
	{
		static coid::token st[]={
			"INFO: ",
			"WARNING: ",
			"EXCEPTION: ",
			"ERROR: ",
			"DEBUG: ",
		};
		static coid::token ud="userdefined";
		
		if (t<Last) return st[t];
		return ud;
	}

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

	ref<logmsg> operator () (const ELogType t)
	{
		ref<logmsg> lm=logmsg::create();
		lm->set_logger(this);
		(*lm)<<type2tok(t);
		return lm;
	}

	ref<logmsg> operator () (const ELogType t, const char * fnc)
	{
		ref<logmsg> lm=logmsg::create();
		lm->set_logger(this);
		(*lm)<<type2tok(t)<<fnc<<' ';
		return lm;
	}

	ref<logmsg> operator () (const ELogType t, const char * fnc, const int line)
	{
		ref<logmsg> lm=logmsg::create();
		lm->set_logger(this);
		(*lm)<<type2tok(t)<<fnc<<'('<<line<<')'<<' ';
		return lm;
	}
};

class log_writer
{
protected:
	coid::thread _thread;
	atomic::queue<logmsg> _queue;

public:
	log_writer()
		: _thread()
	{
		_thread.create( thread_run_fn, this, 0, "log_writer" );
	}

    static void* thread_run_fn( void* p )
    {
        return reinterpret_cast<log_writer*>(p)->thread_run();
    }

	void* thread_run();

	void addmsg(logmsg * m) { _queue.push(m); }
};

COID_NAMESPACE_END

#endif // __COMM_LOGGER_H__