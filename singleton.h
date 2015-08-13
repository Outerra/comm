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

typedef void* (*fn_singleton_creator)();
typedef void (*fn_singleton_destroyer)(void*);
typedef void (*fn_singleton_initmod)(void*);    //< optional initialization of singleton in different module

void* singleton_register_instance(
    fn_singleton_creator create,
    fn_singleton_destroyer destroy,
    fn_singleton_initmod initmod,
    const char* type,
    const char* file,
    int line,
    bool invisible);

void singletons_destroy();

fn_singleton_creator singleton_local_creator( void* p );

////////////////////////////////////////////////////////////////////////////////
///Class for global and local singletons
template <class T>
class singleton
{
    /*! The template `has_void_foo_no_args_const<T>` exports a
        boolean constant `value` that is true iff `T` provides
        `void foo() const`

        It also provides `static void eval(T const & t)`, which
        invokes void `T::foo() const` upon `t` if such a public member
        function exists and is a no-op if there is no such member.
    */ 
    template< typename T>
    struct has_singleton_initialize_module_method
    {
        // SFINAE foo-has-correct-sig
        template<typename A>
        static std::true_type test(void (A::*)(void*)) {
            return std::true_type();
        }

        // SFINAE foo-exists
        template <typename A>
        static decltype(test(&A::singleton_initialize_module))
        test(decltype(&A::singleton_initialize_module),void *) {
            // foo exists. check sig
            typedef decltype(test(&A::singleton_initialize_module)) return_type;
            return return_type();
        }

        // SFINAE game over 
        template<typename A>
        static std::false_type test(...) {
            return std::false_type(); 
        }

        // This will be either `std::true_type` or `std::false_type`
        typedef decltype(test<T>(0,0)) type;

        static void evalfn(std::true_type) {
            T::singleton_initialize_module();
        }

        static void evalfn(...){
        }

        static void eval() {
            evalfn(type());
        }
    };

public:

    static void init_module( void* p ) {
        has_singleton_initialize_module_method<T>::eval();
    }


    //@return global singleton
	static T& instance()
	{
		T*& node = ptr();

		if(node)
			return *node;

        node = (T*)singleton_register_instance(
            &create, &destroy, &init_module,
            typeid(T).name(), 0, 0, false);
        return *node;
	}

    //@return global singleton, registering the place of birth
	static T& instance(const char* type, const char* file, int line)
	{
		T*& node = ptr();

		if(node)
			return *node;

        node = (T*)singleton_register_instance(
            &create, &destroy, &init_module,
            typeid(T).name(), file, line, false);

        has_singleton_initialize_module_method<T>::eval();

        return *node;
	}

    //@return global singleton reference
    static T*& ptr() {
        static T* node = 0;
        return node;
    }

    ///Local singleton constructor, use through LOCAL_SINGLETON macro
    singleton() {
        _p = (T*)singleton_register_instance(
            &create, &destroy, &init_module,
            typeid(T).name(), 0, 0, true);
    }

    ///Local singleton constructor, use through LOCAL_SINGLETON macro
    singleton(T* obj) {
        _p = (T*)singleton_register_instance(
            singleton_local_creator(obj), &destroy, &init_module,
            typeid(T).name(), 0, 0, true);
    }

    T* operator -> () { return _p; }

    T& operator * () { return *_p; }

    T* get() { return _p; }

private:

    T* _p;

    static void destroy(void* p) {
        delete (T*)p;
    }

    static void* create() {
        return new T;
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
            p = (T*)singleton_register_instance(
                &create, &destroy, &singleton<T>::init_module,
                typeid(T).name(), 0, 0, false);
            ts.set(p);
        }
        return *p;
    }

    ///Local thread singleton constructor, use through LOCAL_THREAD_SINGLETON macro
    thread_singleton() {
        _tkey.set(singleton_register_instance(
            &create, &destroy, &singleton<T>::init_module,
            typeid(T).name(), 0, 0, true));
    }

    ///Local thread singleton constructor, use through LOCAL_THREAD_SINGLETON macro
    thread_singleton(T* obj) {
        _tkey.set(singleton_register_instance(
            singleton_local_creator(obj), &destroy, &singleton<T>::init_module,
            typeid(T).name(), 0, 0, true));
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
    static void* create() {
        return new T;
    }
};


COID_NAMESPACE_END

#endif //__COID_COMM_SINGLETON__HEADER_FILE__
