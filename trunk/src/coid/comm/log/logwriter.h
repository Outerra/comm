
#ifndef __COMM_LOGWRITTER_H__
#define __COMM_LOGWRITTER_H__

#include "logger.h"

COID_NAMESPACE_BEGIN

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

#endif // __COMM_LOGWRITTER_H__
