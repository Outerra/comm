#ifndef __COMM_REF_NG_H__
#define __COMM_REF_NG_H__

#include "singleton.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T> class policy_ref_count;
template<class T> struct policy_trait { typedef policy_ref_count<T> policy; };

#define DEFAULT_POLICY(t, p) \
	template<> struct policy_trait<t> { \
	typedef p<t> policy; \
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class policy_base
{
private:
	policy_base( policy_base const & );
	policy_base & operator=( policy_base const & );

	volatile int32 _count;

public:

	virtual void destroy()=0;

	policy_base() : _count(1) {}

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
	: public policy_base
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

struct create_me { };

static create_me CREATE_ME;

template<class T> class pool;

namespace atomic {
	template<class T> class queue_ng;
}

template<class T>
class ref 
{
public:
	typedef policy_base default_policy_t;

	friend atomic::queue_ng<T>;

public:
	typedef ref<T> ref_t;
	typedef pool<default_policy_t> pool_t;

	ref() : _p(0), _o(0){}

	explicit ref( T* o )
		: _p( policy_trait<T>::policy::create(o) )
		, _o(o) {}

	explicit ref( const create_me&)
		: _p( policy_trait<T>::policy::create() )
		, _o( static_cast<policy_trait<T>::policy*>(_p)->object_ptr() ) {}

	template< class T2 >
	ref( const ref<T2>& p )
		: _p( p.add_ref_copy() )
		, _o( p.get() ) {}
	
	ref( const ref_t& p )
		: _p( p.add_ref_copy() )
		, _o( p._o ) {}

	template<class T2>
	explicit ref(T2* const p)
		: _p( p )
		, _o( p->object_ptr() ) {}

	template<class T2>
	void create(T2* const p) { 
		release();
		_p=p;
		_o=p->object_ptr();
	}

	~ref() { if( _p ) { _p->release(); _p=0; _o=0; } }

	/// DO NOT USE !!!
	default_policy_t* add_ref_copy() const { if( _p ) _p->add_ref_copy(); return _p; }

	const ref_t& operator=(const ref_t& r)
	{
		_p=r.add_ref_copy();
		_o=r._o;
		return *this;
	}

	T * operator->() const { DASSERT( _p!=0 && "You are trying to use not uninitialized REF!" ); return _o; }

	T * operator->() { DASSERT( _p!=0 && "You are trying to use not initialized REF!" ); return _o; }

	T & operator*() const	{ return *_o; }

	void swap(ref_t& rhs) {
		default_policy_t* tmp_p = _p;
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
		_p=policy_trait<T>::policy::create(p); 
		_o=_p->object_ptr(); 
	}

	void create() { 
		release();
		policy_trait<T>::policy* p=policy_trait<T>::policy::create();
		_p=p;
		_o=p->object_ptr(); 
	}

	void create(T* o) {
		release();
		_p=policy_trait<T>::policy::create(o);
		_o=o;
	}

	T* get() const { return _o; }

	bool is_empty() const { return (_p==0); }

	void takeover(ref<T>& p) {
		release();
		_o=p._o;
		_p=p._p;
		p._o=0;
		p._p=0;
	}

	default_policy_t* give_me() { default_policy_t* tmp=_p; _p=0;_o=0; return tmp; }

	int32 refcount() const { return _p?_p->refcount():0; }

	friend coid::binstream& operator<<( coid::binstream& bin,const ref<T>& s ) { return bin<<(*s._o); }

	friend coid::binstream& operator>>( coid::binstream& bin,ref<T>& s ) { s.create(); return bin>>(*s._o); }

	friend coid::metastream& operator<<( coid::metastream& m,const ref<T>& s ) {
		MSTRUCT_OPEN(m,"ref")
			MMAT(m, "ptr", object)
		MSTRUCT_CLOSE(m)
	}

private:
	default_policy_t *_p;
	T *_o;
};

template<class T> 
inline bool operator==( const ref<T>& a,const ref<T>& b )
{
	return a.get()==b.get();
}

template<class T> 
inline bool operator!=( const ref<T>& a,const ref<T>& b )
{
	return !operator==(a,b);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __COMM_REF_NG_H__
