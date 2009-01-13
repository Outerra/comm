#ifndef __COMM_REF_NG_H__
#define __COMM_REF_NG_H__

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

	int32 refcount() { return atomic::add(&_count,0); }
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

	virtual void destroy() { 
		//DASSERT(_CrtCheckMemory());
		delete this; 
		//DASSERT(_CrtCheckMemory());
	}

	T* object_ptr() const { return _obj; }

	static this_t* create(T*const obj) { return new this_t(obj); }

	static this_t* create() { return new this_t(new T); }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

struct create_me { };

static create_me CREATE_ME;

template<class T> class pool;

namespace atomic {
	template<class T,class P> class queue_ng;
}

template<class T, class P=policy_ref_count<T> >
class refs 
{
	friend atomic::queue_ng<T,P>;

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

	refs( const refs_t& p )
		: _p( p.add_ref_copy() )
		, _o( p._o ) {}

	~refs() { if( _p ) { _p->release(); _p=0; _o=0; } }

	const refs_t& operator=(const refs_t& r)
	{
		_p=r.add_ref_copy();
		_o=r._o;
		return *this;
	}

	/*template<class P2>
	refs_t& operator=(refs<T,P2>& p) 
	{
		//!TODO
		return *this;
	}*/

	T * operator->() const { DASSERT( _p!=0 && "You are trying to use not uninitialized REF!" ); return _o; }

	T * operator->() { DASSERT( _p!=0 && "You are trying to use not initialized REF!" ); return _o; }

	T & operator*() const	{ return *_o; }

	void swap(refs_t& rhs)
	{
		P* tmp_p = _p;
		T* tmp_o = _o;
		_p = rhs._p;
		_o = rhs._o;
		rhs._p = tmp_p;
		rhs._o = tmp_o;
	}

	void release() {
		if( _p!=0 ) { 
			_p->release(); _p=0; _o=0; 
		} 
	}

	void create(pool_t* p) {
		release();
		_p=P::create(p); 
		_o=_p->object_ptr(); 
	}

	void create() { 
		release();
		_p=P::create(); 
		_o=_p->object_ptr(); 
	}

	T* get() const { return _o; }

	bool is_empty() const { return (_p==0); }

	template<class T>
	void takeover(refs<T,P>& p) {
		release();
		_o=p._o;
		_p=p._p;
		p._o=0;
		p._p=0;
	}

	P* give_me() { P*tmp=_p; _p=0;_o=0; return tmp; }

	int32 refcount() const { return _p?_p->refcount():0; }

protected:
	void create(P* po) { 
		release();
		_p=po;
		_o=_p->object_ptr();
	}

private:
	P *_p;
	T *_o;
};

template<class T,class P> 
inline bool operator==( const refs<T,P>& a,const refs<T,P>& b )
{
	return a.get()==b.get();
}

template<class T,class P> 
inline bool operator!=( const refs<T,P>& a,const refs<T,P>& b )
{
	return !operator==(a,b);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __COMM_REF_NG_H__
