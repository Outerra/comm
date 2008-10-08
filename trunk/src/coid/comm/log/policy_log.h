#ifndef __COMM_LOG_POLICY_LOG_H__
#define __COMM_LOG_POLICY_LOG_H__

#include "../ref.h"

class ref_base;

COID_NAMESPACE_BEGIN

struct log_flusher {
	static void flush(const ref_base * const obj);
};

template<class T>
struct policy_log : public policy_pooled<T>
{
	//! release reference
	static void release(const ref_base * const obj) throw()
	{
		DASSERT(atomic::add(&obj->_refcount,0)!=0);

		if (atomic::dec(&obj->_refcount) == 0) {
			// flush it 
			log_flusher::flush(obj);

			/*// return it to the pool
			T * const p = static_cast<T*>(const_cast<ref_base*>(obj));
			p->reset();
			pool().push(p);*/
		}
	}
};

COID_NAMESPACE_END

#endif // __COMM_LOG_POLICY_LOG_H__
