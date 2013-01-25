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

#ifndef __COMM_REF_I_H__
#define __COMM_REF_I_H__

#include "commassert.h"
#include "ref_base.h"
#include "binstream/binstreambuf.h"
#include "metastream/metastream.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T> class irefw;

struct create_lock {};

template<class T> 
class iref
{
    friend irefw<T>;

private:
	T* _p;

	typedef iref<T> iref_t;
	typedef coid::pool< coid::policy_pooled_i<T>* > pool_type_t;

public:
	iref() : _p(0) {}

	iref(T* p) : _p(p) { add_ref_copy(); }
	iref(T *p, const create_lock&) : _p(r.add_ref_lock()) {}

	iref(const iref_t& r) : _p(r.add_ref_copy()) {}

	template< class T2 >
	iref( const iref<T2>& r ) : _p(0) {
        create(r.get());
    }

    T* add_ref_copy() const { if(_p) _p->add_ref_copy(); return _p; }
    T* add_ref_lock() const { if(_p && _p->add_ref_lock()) return _p; else return 0; }

    //
	explicit iref( const create_me& )
        : _p(new T())
    { add_ref_copy(); }

	// special constructor from default policy
	explicit iref( const create_pooled& ) 
		: _p( coid::policy_pooled_i<T>::create(true) ) 
	{}

	~iref() { release(); }

	void release() { if(_p) _p->release(); _p=0;  }

	void create(T* const p)
	{
        DASSERT_RETVOID(p!=_p);
		release();
		_p = p;
        _p->add_ref_copy(); 
	}

	bool create_lock(T *p) {
        release();
        if(p && p->add_ref_lock()) {
            _p = p;
            return true;
        }
        else
            return false;
    }

	void create_pooled() {
        T* p = coid::policy_pooled_i<T>::create();
        DASSERT(p!=_p);
		release();
		_p = p;
		add_ref_copy();
	}

	void create_pooled(pool_type_t *po) {
        T* p = coid::policy_pooled_i<T>::create(po);
		DASSERT(p!=_p);
		release();
		_p = p;
		add_ref_copy();
	}

	const iref_t& operator = (const iref_t& r) {
		T* p = r.get();
		r.add_ref_copy();
		release();
		_p = p;
		return *this; 
	}

	T* get() const { return _p; }

	T& operator*() const { DASSERT( _p!=0 ); return *_p; }

	T* operator->() const { DASSERT( _p!=0 ); return _p; }

	void swap(iref_t& r) {
		T* tmp=_p;
		_p=r._p;
		r._p=tmp;
	}

	bool is_empty() const { return (_p==0); }

	typedef T* iref<T>::*unspecified_bool_type;

	void forget() { _p=0; }

	template<class T2>
	void takeover(iref<T2>& p) {
		release();
		_p = p.get();
		p.forget();
	}

	coid::int32 refcount() const { return _p?_p->refcount():0; }

	///Automatic cast to unconvertible bool for checking via if
	operator unspecified_bool_type () const {
	return _p ? &iref<T>::_p : 0;
	}

	friend bool operator==( const iref<T>& a,const iref<T>& b ) {
	return a._p == b._p;
	}

	friend bool operator!=( const iref<T>& a,const iref<T>& b ) {
		return !operator==(a,b);
	}

	friend coid::binstream& operator << (coid::binstream& bin,const iref_t& s) {
		return bin<<*s.get(); 
	}

	friend coid::binstream& operator >> (coid::binstream& bin,iref_t& s) { 
		s.create(new T); return bin>>*s.get(); 
	}

	friend coid::metastream& operator << (coid::metastream& m,const iref_t& s) {
		MSTRUCT_OPEN(m,"ref")
			MMAT(m,"ptr",T)
		MSTRUCT_CLOSE(m)
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<class T>
class irefw
{
protected:

    T *_p;
    volatile coid::uint32 *_weaks;

public:
    irefw() : _p(0), _weaks(0) {}

    irefw(const iref<T> &o)
        : _p(o._p)
        , _weaks(o._p ? _p->add_weak_copy() : 0)
    {}

    irefw(const irefw<T> &o)
        : _p(p._p)
        , _weaks(o._p ? T::add_weak_copy(o._weaks) : 0)
    {}

    ~irefw() { if(_p) release(); }

    void release() {
        if(_p) {
            const coid::uint32 weaks =
                T::counter_t::dec(_weaks) & ~0x8000000;

            if(weaks == 0)
                delete _weaks;

            _p = 0;
            _weaks = 0;
        }        
    }

    bool lock(iref<T> &newref) {
        if(_p)
            for(;;) {
			    coid::int32 tmp = *_weaks;
                if(tmp & 0x80000000) {
                    if(tmp == T::counter_t::add(_weaks, 0))
                        return false;
                }
                else {
                    if(!newref.create_lock(_p) || tmp != T::counter_t::add(_weaks, 0))
                        newref.forget();
                    else
                        return true;
                }
            }
        else
            return false;
    }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __COMM_REF_I_H__
