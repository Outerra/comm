
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * Ladislav Hrabcak
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __ENG_PTR_H__
#define __ENG_PTR_H__

#include "commassert.h"
#include "atomic/atomic.h"
#include "atomic/stack.h"
#include "binstream/binstreambuf.h"
#include "metastream/metastream.h"

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

	/*
	void cas(T * o, T * n)
	{
		atomic::cas_ptr(reinterpret_cast<void*volatile*>(&p_), n, o);
	}
	*/

	friend coid::binstream& operator << (coid::binstream& bin, const ref<T,P>& s)
	{   return bin << *static_cast<T*>(s.p_); }

	friend coid::binstream& operator >> (coid::binstream& bin, ref<T,P>& s)
	{   s=new T(); return bin >> *static_cast<T*>(s.p_); }

	friend coid::metastream& operator << (coid::metastream& m, const ref<T,P>& s)
	{
		MSTRUCT_OPEN(m,"ref")
			MMAT(m, "ptr", object)
		MSTRUCT_CLOSE(m)
	}

private:

	ref_base * volatile p_;
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
		return p.get() ? p : new T();
	}

	//! create instance or take one from pool
	template<class P>
	static ref<T> create(P param)
	{
		ref<T> p = pool().pop();
		return p.get() ? p : new T(param);
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
