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

template<class T> 
class iref
{
private:
	typedef iref<T> iref_t;

public:
	iref() : _p(0) {}

	explicit iref(T* p) : _p(p) {}

	iref(const iref_t& r) : _p(r.add_ref_copy()) {}

	template< class T2 >
	iref( const iref<T2>& r ) : _p(r.add_ref_copy()) {}

	~iref() { release(); }

	T* add_ref_copy() const { if( _p ) _p->add_ref_copy(); return _p; }

	void release() { if( _p!=0 ) { _p->release(); _p=0; } }

	void create(T* const p) { release(); _p=p; }

	const iref_t& operator=(const iref_t& r) { _p=r.add_ref_copy(); return *this; }

	T* get() const { DASSERT( _p!=0 ); return _p; }

	T& operator*() const { DASSERT( _p!=0 ); return *_p; }

	T* operator->() const { DASSERT( _p!=0 ); return _p; }

	void swap(iref_t& r) {
		T* tmp=_p;
		_p=r._p;
		r._p=tmp;
	}

	bool is_empty() const { return (_p==0); }

    typedef T* iref<T>::*unspecified_bool_type;

    ///Automatic cast to unconvertible bool for checking via if
	operator unspecified_bool_type () const {
        return _p ? &iref<T>::_p : 0;
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

private:
	T* _p;
};

template<class T> 
inline bool operator==( const iref<T>& a,const iref<T>& b ) {
	return a.get()==b.get();
}

template<class T> 
inline bool operator!=( const iref<T>& a,const iref<T>& b ) {
	return !operator==(a,b);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#endif // __COMM_REF_I_H__
