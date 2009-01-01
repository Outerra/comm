#ifndef __COMM_REF_NG_H__
#define __COMM_REF_NG_H__

#include "atomic/stack.h"
#include "atomic/queue.h"
#include "singleton.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class ref_counter
{
private:
	ref_counter( ref_counter const & );
	ref_counter & operator=( ref_counter const & );

	volatile int32 _count;

public:

	virtual void destroy()=0;

	ref_counter() : _count(1) {}

	void add_ref_copy() { atomic::inc(&_count); }

	bool add_ref_lock()
	{
		for( ;; ) {
			int32 tmp = _count;
			if( tmp==0 ) return false;
			if( atomic::b_cas( &_count,tmp, tmp+1 ) ) return true;
		}
	}

	void release() { if( atomic::dec( &_count )==0 ) destroy(); }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
class policy_ref_count
	: public ref_counter
{
private:
	policy_ref_count( policy_ref_count const & );
	policy_ref_count & operator=( policy_ref_count const & );

protected:
	T* _obj;

	typedef policy_ref_count<T> this_t;

public:
	policy_ref_count(T* const obj) : _obj(obj) {}

	virtual ~policy_ref_count() {
		if( _obj ) {
			delete _obj;
			_obj=0;
		}
	}

	virtual void destroy() { delete this; }

	T* object_ptr() const { return _obj; }

	static this_t* create(T*const obj) { return new this_t(obj); }

	static this_t* create() { return new this_t(new T); }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

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
		return p?p:P::internal_create(this);
	}

	void destroy( P* p) {
		//!TODO call reset?
		_stack.push(p);
	}
};

namespace atomic {
	template<class T> class queue_ng;
}

template<class T>
class policy_queue_pooled 
	: public policy_ref_count<T>
	, public atomic::stack_node<policy_queue_pooled<T> >
	, public atomic::queue_ng<T>::node_t
{
	typedef policy_queue_pooled<T> this_t;
	typedef pool<this_t> pool_t;

protected:
	explicit policy_queue_pooled(T* const obj, pool_t* const pl=0) 
		: policy_ref_count(obj)
		, _pool(pl) {}

public:
	virtual void destroy() { DASSERT(_pool!=0); _pool->destroy(this); }

	static this_t* create() { return SINGLETON(pool_t).create(); }

	static this_t* create(pool_t* p) { DASSERT(p!=0); return p->create(); }

	static this_t* internal_create(pool_t* pl) { return new this_t(new T,pl); }

	pool_t* _pool;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

struct create_me { };

static create_me CREATE_ME;

template<class T, class P=policy_ref_count<T> >
class refs 
{
	friend atomic::queue_ng<T>;

protected:
	refs(P* const p)
		: _p(p)
		, _o( _p->object_ptr() ) {}

public:
	typedef refs<T,P> refs_t;
	typedef pool<P> pool_t;

	/// DO NOT USE !!!
	P* add_ref_copy() const { _p->add_ref_copy(); return _p; }

	refs() : _p(0), _o(0){}

	explicit refs( T* o )
		: _p( P::create(o) )
		, _o( _p->object_ptr() ) {}

	explicit refs( const create_me&)
		: _p( P::create() )
		, _o( _p->object_ptr() ) {}

	template< class P2 >
	explicit refs( refs<T,P2> &p )
		: _p( p.add_ref_copy() )
		, _o( _p->object_ptr() ) {}

	refs( const refs_t& r )
		: _p( p.add_ref_copy() )
		, _o( r._o ) {}

	~refs() { if( _p ) { _p->release(); _p=0; _o=0; } }

	refs& operator=(refs_t const& r)
	{
		_p=r.add_ref_copy();
		_o=r._o;
		return *this;
	}

	template<class P2>
	refs& operator=(refs<T,P2>& p) 
	{
		//!TODO
		return *this;
	}

	T * operator->() const
	{
		DASSERT( _p!=0 && "You are trying to use not uninitialized REF!" );
		return _o;
	}

	T * operator->()
	{
		DASSERT( _p!=0 && "You are trying to use not initialized REF!" );
		return _o;
	}

	void swap(refs_t& rhs)
	{
		P* tmp_p = _p;
		T* tmp_o = _o;
		_p = rhs._p;
		_o = rhs._o;
		rhs._p = tmp_p;
		rhs._o = tmp_o;
	}

	void create(pool_t* p) {
		if( _p!=0 ) { _p->release(); _p=0; _o=0; } 
		_p=P::create(p); 
		_o=_p->object_ptr(); 
	}

	void create() { 
		if( _p!=0 ) { _p->release(); _p=0; _o=0; } 
		_p=P::create(); 
		_o=_p->object_ptr(); 
	}

protected:
	void create(P* po) { 
		if( _p!=0 ) { _p->release(); _p=0; _o=0; } 
		_p=po;
		_o=_p->object_ptr();
	}

private:
	P *_p;
	T *_o;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __COMM_REF_NG_H__
