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

#ifndef __COMM_REF_BASE_H__
#define __COMM_REF_BASE_H__

#include "atomic/atomic.h"
#include "singleton.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T> class policy_shared;
template<class T> struct policy_trait { typedef policy_shared<T> policy; };

#define DEFAULT_POLICY(t, p) \
	template<> struct policy_trait<t> { \
	typedef p<t> policy; \
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

struct atomic_counter
{
	static coid::int32 inc(volatile coid::int32* ptr) { return atomic::inc(ptr); }
	static coid::int32 add(volatile coid::int32* ptr,const coid::int32 v) { return atomic::add(ptr,v); }
	static coid::int32 dec(volatile coid::int32* ptr) { return atomic::dec(ptr); }
	static bool b_cas(volatile coid::int32 * ptr,const coid::int32 val,const coid::int32 cmp) { 
		return atomic::b_cas( ptr,val,cmp ); 
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

struct simple_counter
{
	static coid::int32 inc(volatile coid::int32* ptr) { return ++(*ptr); }
	static coid::int32 add(volatile coid::int32* ptr,const coid::int32 v) { return (*ptr)+=v; }
	static coid::int32 dec(volatile coid::int32* ptr) { return --(*ptr); }
	static bool b_cas(volatile coid::int32 * ptr,const coid::int32 val,const coid::int32 cmp) { 
		DASSERT(false && "should not be used!"); 
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class policy_destroy
{
protected:

	virtual void destroy() { delete this; }

};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class COUNTER=atomic_counter>
class policy_base_
{
private:
	policy_base_( policy_base_ const & );
	policy_base_ & operator=( policy_base_ const & );

	volatile coid::int32 _count;

public:

    virtual ~policy_base_() {}

	virtual void destroy()=0;

	policy_base_() : _count(1) {}

	coid::int32 add_ref_copy() { return COUNTER::inc(&_count); }

	bool add_ref_lock()
	{
		for( ;; ) {
			coid::int32 tmp = _count;
			if( tmp==0 ) return false;
			if( COUNTER::b_cas( &_count,tmp, tmp+1 ) ) return true;
		}
	}

	coid::int32 release() { 
		coid::int32 c=COUNTER::dec( &_count );
		if( c==0 ) destroy();
		return c;
	}

	coid::int32 refcount() { return COUNTER::add(&_count,0); }
};

typedef policy_base_<> policy_base;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class policy_intrusive_base
	: public policy_base
{
private:
	policy_intrusive_base( policy_intrusive_base const & );
	policy_intrusive_base & operator=( policy_intrusive_base const & );

protected:
	policy_intrusive_base() {}

    virtual ~policy_intrusive_base() {}

	virtual void destroy() { delete this; }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

class policy_shared_base
	: public policy_base
{
private:
	policy_shared_base( policy_shared_base const & );
	policy_shared_base & operator=( policy_shared_base const & );

protected:
	policy_shared_base() {}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
class policy_shared
	: public policy_shared_base
{
private:
	policy_shared( policy_shared const & );
	policy_shared & operator=( policy_shared const & );

protected:
	T* _obj;

	typedef policy_shared<T> this_t;

	virtual ~policy_shared() { if( _obj ) delete _obj; _obj=0; }

	policy_shared(T* const obj) : _obj(obj) {}

	virtual void destroy() { delete this; }

public:
	static this_t* create(T*const obj) { return new this_t(obj); }

	static this_t* create() { return new this_t(new T); }

	T* get() const { return _obj; }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
class policy_dummy
	: public policy_shared<T>
{
private:
	policy_dummy( policy_dummy const & );
	policy_dummy & operator=( policy_dummy const & );

protected:
	typedef policy_dummy<T> this_t;

	policy_dummy(T* const obj) : policy_shared(obj) {}

	virtual ~policy_dummy() { _obj=0; }

	virtual void destroy() { delete this; }

public:
	static this_t* create(T*const obj) { return new this_t(obj); }

	static this_t* create() { return new this_t(new T); }

	T* get() const { return _obj; }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __COMM_REF_BASE_H__
