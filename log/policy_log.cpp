#include "policy_log.h"
#include "logger.h"

using namespace coid;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void log_flusher::flush(const ref_base *const obj)
{
	SINGLETON(log_writer).addmsg(static_cast<logmsg*>(const_cast<ref_base*>(obj)));
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
