#ifndef __ENG_PTR_H__
#define __ENG_PTR_H__

#include "assert.h"
#include "atomic/atomic.h"
#include "atomic/stack.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class ref_base
{
public:
	//! reference counter
	mutable volatile int32 _refcount;

protected:
	//! Default constructor
	ref_base() : _refcount(0) {}

	//! Default constructor
	explicit ref_base(const bool) : _refcount(-1) {}

	//! copy constructor
	ref_base(const ref_base &) : _refcount(0) {} // do not copy O._refcount

public:

	//! Virtual destructor
	virtual ~ref_base()
	{
		// this may prevent if any class contains more than one
		// of this instance
		DASSERT(_refcount==0 && "ref_base");
	}

	//! assign operator
	ref_base & operator = (const ref_base &)
	{
		// do not copy O._refcount
		return *this;
	}

public:
	//! return number of references of this object
	int32 refCount() const throw() { return _refcount; } 

	//! forced release of the object
	void destroy()
	{
		DASSERT(_refcount == -1);
		delete const_cast<ref_base*>(this);
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T> struct policy_refcount;

template <class T>
struct policy_trait
{
	typedef policy_refcount<T> policy;
};

#define DEFAULT_POLICY(t, p) \
	template<> struct policy_trait<t> { \
	typedef p<t> policy; \
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
void policy_addRef(const T * const obj)
{
	policy_trait<T>::policy::addRef(obj);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
void policy_release(const T * const obj)
{
	policy_trait<T>::policy::release(obj);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T, class P = typename policy_trait<T>::policy > 
class ref
{
private:
	typedef ref this_type;

public:
	ref() : p_(0) {}

	ref(T * p, bool add_ref = true)
		: p_(p)
	{
		if(p_ != 0 && add_ref) P::addRef(p_);
	}

	ref(ref const & rhs)
		: p_(rhs.p_)
	{
		if(p_ != 0) P::addRef(p_);
	}

	~ref()
	{
		if(p_ != 0) P::release(p_);
	}

	ref & operator=(ref const & rhs)
	{
		this_type(rhs).swap(*this);
		return *this;
	}

	ref & operator=(T * rhs)
	{
		this_type(rhs).swap(*this);
		return *this;
	}

	T * get() const         { return static_cast<T*>(p_); }

	T & operator*() const	{ return *static_cast<T*>(p_); }

	T * operator->() const
	{
		DASSERT(p_ != 0 && "You are trying to use released ptr!");
		return static_cast<T*>(p_);
	}
	/*
	typedef T * this_type::*unspecified_bool_type;

	operator unspecified_bool_type () const
	{
	return p_ == 0 ? 0 : &this_type::p_;
	}
	*/
	operator T* () const    { return static_cast<T*>(p_); }

	void swap(ref & rhs)
	{
		T * tmp = static_cast<T*>(p_);
		p_ = rhs.p_;
		rhs.p_ = tmp;
	}

private:

	ref_base * p_;
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T, class U> 
inline bool operator==(ref<T> const & a, ref<U> const & b)
{
	return a.get() == b.get();
}

template<class T, class U> 
inline bool operator!=(ref<T> const & a, ref<U> const & b)
{
	return a.get() != b.get();
}

template<class T> 
inline bool operator==(ref<T> const & a, T * b)
{
	return a.get() == b;
}

template<class T> 
inline bool operator!=(ref<T> const & a, T * b)
{
	return a.get() != b;
}

template<class T> 
inline bool operator==(T * a, ref<T> const & b)
{
	return a == b.get();
}

template<class T> 
inline bool operator!=(T * a, ref<T> const & b)
{
	return a != b.get();
}

template<class T> 
inline bool operator<(ref<T> const & a, ref<T> const & b)
{
	return std::less<T *>()(a.get(), b.get());
}

template<class T> 
void swap(ref<T> & lhs, ref<T> & rhs)
{
	lhs.swap(rhs);
}

// mem_fn support

template<class T> 
T * get_pointer(ref<T> const & p)
{
	return p.get();
}

template<class T, class U> 
ref<T> static_ptr_cast(ref<U> const & p)
{
	return static_cast<T *>(p.get());
}

template<class T, class U> 
ref<T> const_ptr_cast(ref<U> const & p)
{
	return const_cast<T *>(p.get());
}

template<class T, class U> 
ref<T> dynamic_ptr_cast(ref<U> const & p)
{
	return dynamic_cast<T *>(p.get());
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
struct policy_refcount
{
	//! add reference
	static void addRef(const ref_base * const obj) throw()
	{
		if (obj->_refcount < 0) return;
		atomic::inc(&obj->_refcount);
		DASSERT(atomic::add(&obj->_refcount, 0) != 0 && "Problem?");
	}

	//! add reference
	static void release(const ref_base * const obj) throw()
	{
		if (obj->_refcount < 0) return;
		DASSERT(atomic::add(&obj->_refcount, 0) != 0 && "already released!");
		if (atomic::dec(&obj->_refcount) == 0) {
			delete const_cast<ref_base*>(obj);
		}
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
struct policy_pooled
{
	//! static instance of stack
	static atomic::stack<T> & pool() { static atomic::stack<T> s; return s; }

	//! create instance or take one from pool
	static ref<T> create()
	{
		ref<T> p = pool().pop();
		return p ? p : new T();
	}

	//! destroy all instances in pool
	static void purge() 
	{ 
		T * p; 
		while ((p = pool().pop())!= 0) {
			DASSERT(p->_refcount == 0);
			delete const_cast<T*>(p);
		}
	}

	//! add reference
	static void addRef(const ref_base * const obj) throw()
	{
		atomic::inc(&obj->_refcount);
		DASSERT(atomic::add(&obj->_refcount, 0) != 0);
	}

	//! add reference
	static void release(const ref_base * const obj) throw()
	{
		DASSERT(atomic::add(&obj->_refcount,0)!=0);

		if (atomic::dec(&obj->_refcount) == 0) {
			// return it to the pool
			T * const p = static_cast<T*>(const_cast<ref_base*>(obj));
			p->reset();
			pool().push(p);
		}
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
class pooled 
	: public ref_base
	, public atomic::stack<T>::node_t
{
public:
	//! create instance
	static ref<T> create()
	{
		return policy_pooled<T>::create();
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __ENG_PTR_H__
