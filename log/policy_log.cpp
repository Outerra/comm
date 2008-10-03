#include "policy_log.h"
#include "logger.h"

using namespace coid;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void log_flusher::flush(const ref_base *const obj)
{
	const logmsg * const lm = static_cast<const logmsg*>(obj);

	lm->get_logger()->flush(*lm);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
