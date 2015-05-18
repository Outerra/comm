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
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
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

#ifndef __COID_COMM_SINGLETON__HEADER_FILE__
#define __COID_COMM_SINGLETON__HEADER_FILE__

#include "namespace.h"
#include "commtypes.h"

#include "pthreadx.h"

#ifdef _DEBUG

///Retrieves process-global singleton object of given type T
# define SINGLETON(T) \
    coid::singleton<T>::instance(#T,__FILE__,__LINE__)

#else

///Retrieves process-global singleton object of given type T
# define SINGLETON(T) \
    coid::singleton<T>::instance()

#endif

///Evaluates to true if a global singleton of given type 
#define SINGLETON_EXISTS(T) \
    (coid::singleton<T>::ptr() != 0)

///Used for function-local singleton objects 
/// usage:
/// LOCAL_SINGLETON(class) name = new class;
#define LOCAL_SINGLETON(T) \
    static coid::singleton<T>


///Returns thread-global singleton (the same one when called from different code)
#define THREAD_SINGLETON(T) \
    coid::thread_singleton<T>::instance()

///Used for function-local thread singleton objects
/// usage:
/// THREAD_LOCAL_SINGLETON(class) name = new class;
#define THREAD_LOCAL_SINGLETON(T) \
    static coid::thread_singleton<T>


////////////////////////////////////////////////////////////////////////////////
COID_NAMESPACE_BEGIN

void* singleton_register_instance(
    void* p,
    void (*fn_destroy)(void*),
    const char* type,
    const char* file,
    int line);

void singletons_destroy();

////////////////////////////////////////////////////////////////////////////////
///Class for global and local singletons
template <class T>
class singleton
{
public:

    //@return global singleton
	static T& instance()
	{
		T*& node = ptr();

		if(node)
			return *node;

        node = (T*)singleton_register_instance(new T, &destroy, 0, 0, 0);
        return *node;
	}

    //@return global singleton, registering the place of birth
	static T& instance(const char* type, const char* file, int line)
	{
		T*& node = ptr();

		if(node)
			return *node;

        node = (T*)singleton_register_instance(new T, &destroy, type, file, line);
        return *node;
	}

    //@return global singleton reference
    static T*& ptr() {
        static T* node = 0;
        return node;
    }

    ///Local singleton constructor, use through LOCAL_SINGLETON macro
    singleton() {
        _p = (T*)singleton_register_instance(new T, &destroy, 0, 0, 0);
    }

    ///Local singleton constructor, use through LOCAL_SINGLETON macro
    singleton(T* obj) {
        _p = (T*)singleton_register_instance(obj, &destroy, 0, 0, 0);
    }

    T* operator -> () { return _p; }

    T& operator * () { return *_p; }

    T* get() { return _p; }

private:

    T* _p;

    static void destroy(void* p) {
        delete (T*)p;
    }
};


////////////////////////////////////////////////////////////////////////////////
///Class for global and local thread singletons
template<class T>
class thread_singleton
{
public:

    //@return global thread singleton (always the same one from multiple places where used)
    static T& instance()
    {
        thread_key& ts = get_key();
        T* p = reinterpret_cast<T*>(ts.get());
        if(!p) {
            p = (T*)singleton_register_instance(new T, &destroy, 0, 0, 0);
            ts.set(p);
        }
        return *p;
    }

    ///Local thread singleton constructor, use through LOCAL_THREAD_SINGLETON macro
    thread_singleton() {
        _tkey.set(singleton_register_instance(new T, &destroy, 0, 0, 0));
    }

    ///Local thread singleton constructor, use through LOCAL_THREAD_SINGLETON macro
    thread_singleton(T* obj) {
        _tkey.set(singleton_register_instance(obj, &destroy, 0, 0, 0));
    }


    T* operator -> () { return static_cast<T*>(_tkey.get()); }

    T& operator * () { return static_cast<T*>(_tkey.get()); }

    T* get() { return static_cast<T*>(_tkey.get()); }

private:

    //@return global thread key
    static thread_key& get_key() {
        static thread_key _tkey;
        return _tkey;
    }

    thread_key _tkey;                   //< local thread key

    static void destroy(void* p) {
        delete static_cast<T*>(p);
    }
};


COID_NAMESPACE_END

#endif //__COID_COMM_SINGLETON__HEADER_FILE__
