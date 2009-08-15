#ifndef __COMM_ATOMIC_POOL_H__
#define __COMM_ATOMIC_POOL_H__

#include "stack.h"
#include "queue.h"
#include "../ref.h"

template<class P>
class pool
{
protected:
	atomic::stack<P> _stack;

public:
	//! static instance of stack
	static pool & get() { static pool p; return p; }

	//! create instance or take one from pool
	P* create() {
		P* p = _stack.pop();
		if( p!=0 ) {
			p->add_ref_copy();
			return p;
		}
		else {
			return P::internal_create(this);
		}
	}

	void destroy( P* p) {
		p->object_ptr()->reset();
		_stack.push(p);
	}
};

template<class T>
class policy_queue_pooled 
	: public policy_ref_count<T>
	, public atomic::stack_node
	, public atomic::queue_node
{
public:
	typedef policy_queue_pooled<T> this_t;
	typedef pool<this_t> pool_t;

protected:
	explicit policy_queue_pooled(T* const obj, pool_t* const pl=0) 
		: policy_ref_count(obj)
		, _pool(pl) {}

public:
	virtual void destroy() { DASSERT(_pool!=0); _pool->destroy(this); }

	static this_t* create() { return SINGLETON(pool<policy_queue_pooled<T>>).create(); }

	static this_t* create(pool_t* p) { DASSERT(p!=0); return p->create(); }

	static this_t* internal_create(pool_t* pl) { return new this_t(new T,pl); }

	pool_t* _pool;
};

#endif // __COMM_ATOMIC_POOL_H__
