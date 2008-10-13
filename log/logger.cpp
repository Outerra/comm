
#include "logwriter.h"

using namespace coid;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void* log_writer::thread_run()
{
	for ( ;; ) {
		logmsg * m;
		
		while ( (m=_queue.pop())!=0) { 
			m->get_logger()->flush(*m);
			logmsg * const p = static_cast<logmsg*>(const_cast<logmsg*>(m));
			p->reset();
			policy_log<logmsg>::pool().push(p);
		}
		if ( coid::thread::self_should_cancel() ) break;
		coid::sysMilliSecondSleep(500);
	}
	return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
