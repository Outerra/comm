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
#include <atomic>
#include <type_traits>
#include <typeinfo>

#if __cplusplus < 202002L && !defined(_HAS_CXX20)
#error C++20 required
#endif

#include <source_location>


///Retrieves module (current dll/exe) singleton object of given type T
#define SINGLETON(...) \
    coid::singleton<__VA_ARGS__>::instance(true, std::source_location::current())

///Retrieves process-wide global singleton object of given type T - its module pointer apart from SINGLETON's (another
/// instantiation of the class)
#define PROCWIDE_SINGLETON(...) \
    coid::singleton<__VA_ARGS__, "", 0, false>::instance(false, std::source_location::current())


///Used for function-local singleton objects, unique for each module (dll)
/// usage:
/// LOCAL_FUNCTION_SINGLETON_DEF(class) name = new class;
#define LOCAL_FUNCTION_SINGLETON_DEF(...) \
    static coid::singleton<__VA_ARGS__, __FUNCSIG__, std::source_location::current().line()>

///Used for function-local singleton objects returning process-wide singleton
/// usage:
/// LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(class) name = new class;
#define LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(...) \
    static coid::singleton<__VA_ARGS__, __FUNCSIG__, std::source_location::current().line(), false>

///Used for file-local singleton objects, unique for each module (dll)
/// usage:
/// LOCAL_FILE_SINGLETON_DEF(class) name = new class;
#define LOCAL_FILE_SINGLETON_DEF(...)\
    static coid::singleton<__VA_ARGS__, __FILE__, std::source_location::current().line()>

///Used for file-local singleton objects, returning process-wide singleton
/// usage:
/// LOCAL_FILE_PROCWIDE_SINGLETON_DEF(class) name = new class;
#define LOCAL_FILE_PROCWIDE_SINGLETON_DEF(...)\
    static coid::singleton<__VA_ARGS__, __FILE__, std::source_location::current().line(), false>

///Returns thread singleton instance (the same one when called from different code within module)
#define THREAD_SINGLETON(...) \
    coid::thread_singleton<__VA_ARGS__>::instance(true)

///Returns a procwide thread singleton instance - the thread's own, the same in every module
#define THREAD_PROCWIDE_SINGLETON(...) \
    coid::thread_singleton<__VA_ARGS__>::instance(false)

///Used for function-local thread singleton objects
/// usage:
/// THREAD_LOCAL_SINGLETON_DEF(class) name = new class;
#define THREAD_LOCAL_SINGLETON_DEF(...) \
    __declspec(thread) static coid::thread_singleton<__VA_ARGS__, __FUNCSIG__, std::source_location::current().line()>


///Same as LOCAL_FUNCTION_SINGLETON_DEF (compatibility)
#define LOCAL_SINGLETON_DEF(...) LOCAL_FUNCTION_SINGLETON_DEF(__VA_ARGS__)

///Same as LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF (compatibility)
#define LOCAL_PROCWIDE_SINGLETON_DEF(...) LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(__VA_ARGS__)

///Same as LOCAL_SINGLETON_DEF (compatibility)
#define LOCAL_SINGLETON(...) LOCAL_SINGLETON_DEF(__VA_ARGS__)


///Evaluates to true if a singleton of given type exists - the module's SINGLETON or PROCWIDE_SINGLETON
#define SINGLETON_EXISTS(...) \
    (coid::singleton<__VA_ARGS__>::ptr() != 0 || coid::singleton<__VA_ARGS__, "", 0, false>::ptr() != 0)


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
    bool invisible,
    const char* unique_indentifier);

///Registers a thread's object of a process-wide thread singleton in the process's registry - created at every
/// registration, never found by a lookup: the process's thread key finds it
void* singleton_register_thread_instance(
    fn_singleton_creator create,
    fn_singleton_destroyer destroy,
    fn_singleton_initmod initmod,
    const char* type,
    int line,
    const char* unique_indentifier);

void singleton_unregister_instance(void*);

void singletons_destroy();

fn_singleton_creator singleton_local_creator(void* p);

///Lock of the module's singleton registry, held by a SINGLETON's first call while it checks and registers the module's
/// instance - the module's registry creates an object at every registration (recursive: the registration locks it again)
class singleton_module_lock
{
public:
    ///@param lock lock the module's registry; false for none (a PROCWIDE_SINGLETON's first call)
    explicit singleton_module_lock(bool lock);
    ~singleton_module_lock();

    singleton_module_lock(const singleton_module_lock&) = delete;
    singleton_module_lock& operator = (const singleton_module_lock&) = delete;

private:
    bool _locked;
};


///This will run singleton_initialize_module() static method if T has one
template< typename T>
struct has_singleton_initialize_module_method
{
    // SFINAE foo-has-correct-sig
    template<typename A>
    static std::true_type test(void (A::*)()) {
        return std::true_type();
    }

    // SFINAE foo-exists
    template <typename A>
    static decltype(test(&A::singleton_initialize_module))
        test(decltype(&A::singleton_initialize_module), void*) {
        // foo exists. check sig
        typedef decltype(test(&A::singleton_initialize_module)) return_type;
        return return_type();
        //return std::true_type();
    }

    // SFINAE game over
    template<typename A>
    static std::false_type test(...) {
        return std::false_type();
    }

    // This will be either `std::true_type` or `std::false_type`
    typedef decltype(test<T>(0, 0)) type;

    static const bool value = type::value;

    static void evalfn(T* p, std::true_type) {
        p->singleton_initialize_module();
    }

    static void evalfn(...) {
    }

    static void eval(void* p) {
        evalfn(static_cast<T*>(p), type());
    }
};


////////////////////////////////////////////////////////////////////////////////
///Class for global and local singletons
//@tparam T - type of singleton
//@tparam Unique_identifier - signature of function when LOCAL_FUNCTION_SINGLETON used, file path when LOCAL_FILE_SINGLETON used
//@tparam Line - line in file form where the singleton was created(0 for global singletons)
//@tparam Module true if locally created singletons should be unique in each module (dll), false if process-wide

namespace detail
{
template<uints N>
struct string_literal
{
    constexpr string_literal(const char(&str)[N]) {
        for (uints i = 0; i < N; ++i) value[i] = str[i];
    }
    char value[N];
};
}

template <class T, detail::string_literal Unique_identifier = "", int Line = 0, bool Module = true>
class singleton
{
public:

    static void init_module(void* p) {
        has_singleton_initialize_module_method<T>::eval(p);
    }


    /// @return global singleton, registering the place of birth
    /// @param module_local create a singleton that's local to the current module
    /// @note Thread safe: concurrent first calls get one instance - SINGLETON's registered under the module registry's
    ///  lock, PROCWIDE_SINGLETON's found by the process's registry under its lock, by its type alone (whichever line or
    ///  module made the first call)
    static T& instance(bool module_local, const std::source_location& loc)
    {
        std::atomic<T*>& node = ptr();

        T* p = node.load(std::memory_order_acquire);
        if (p)
            return *p;

#if !defined(SYSTYPE_MSVC) || SYSTYPE_MSVC >= 1700
        static_assert(std::is_default_constructible<T>::value, "type is not default constructible");
#endif

        //the module's registry creates an object at every registration: SINGLETON's first call checks again and registers
        //under its lock; the process's registry finds the instance under its own lock - PROCWIDE_SINGLETON takes none here
        singleton_module_lock lock(module_local);

        p = node.load(std::memory_order_acquire);
        if (p)
            return *p;

        p = (T*)singleton_register_instance(
            &create, &destroy, &init_module,
            typeid(T).name(), loc.file_name(), loc.line(), module_local, "");

        node.store(p, std::memory_order_release);
        return *p;
    }

    /// @return the module's pointer to the singleton - nullptr before the first call; SINGLETON's in singleton<T>,
    ///  PROCWIDE_SINGLETON's in singleton<T, "", 0, false>
    static std::atomic<T*>& ptr() {
        static std::atomic<T*> node = nullptr;
        return node;
    }

    ///Local singleton constructor, use through LOCAL_SINGLETON macro
    singleton() {
#if !defined(SYSTYPE_MSVC) || SYSTYPE_MSVC >= 1700
        static_assert(std::is_default_constructible<T>::value, "type is not default constructible");
#endif

        _p = (T*)singleton_register_instance(&create, &destroy, &init_module, typeid(T).name(), "", Line, Module, Unique_identifier.value);
    }

    ///Local singleton constructor, use through LOCAL_SINGLETON macro
    /// @param obj the singleton's object - deleted when the process's registry has the singleton (another module's)
    singleton(T* obj) {
        _p = (T*)singleton_register_instance(
            singleton_local_creator(obj), &destroy, &init_module,
            typeid(T).name(), "", Line, Module, Unique_identifier.value);

        if (_p != obj)
            delete obj;
    }

    singleton(T&& obj) : singleton(new T(std::forward<T>(obj))) {
    }

    ~singleton() {
        if (_p)
            singleton_unregister_instance(_p);
    }

    T* operator -> () { return _p; }

    T& operator * () { return *_p; }

    T* get() { return _p; }

private:

    T* _p = 0;

    static void destroy(void* p) {
        delete (T*)p;
    }

    static void* create() {
        return new T();
    }
};


////////////////////////////////////////////////////////////////////////////////
///The thread key of THREAD_PROCWIDE_SINGLETON(T) - a process-wide singleton, so every module finds a thread's object by
/// it (a TLS index is the process's)
template<class T>
struct thread_singleton_key
{
    thread_key key;
};

////////////////////////////////////////////////////////////////////////////////
///Class for global and local thread singletons
template<class T, detail::string_literal Function_signature = "", int Line = -1>
class thread_singleton
{
public:

    /// @return global thread singleton - the thread's object (always the same one from multiple places where used);
    ///  THREAD_SINGLETON's the module's (its thread key), THREAD_PROCWIDE_SINGLETON's the same in every module (the
    ///  process's thread key)
    static T& instance(bool module_local)
    {
        thread_key& tk = module_local ? get_key() : PROCWIDE_SINGLETON(thread_singleton_key<T>).key;
        return *thread_instance(tk, module_local, nullptr);
    }

    ///Local thread singleton constructor, use through LOCAL_THREAD_SINGLETON macro
    thread_singleton() {
        thread_instance(_tkey, true, nullptr);
    }

    ///Local thread singleton constructor, use through LOCAL_THREAD_SINGLETON macro
    thread_singleton(T* obj) {
        thread_instance(_tkey, true, obj);
    }


    T* operator -> () { return thread_instance(_tkey, true, nullptr); }

    T& operator * () { return *thread_instance(_tkey, true, nullptr); }

    T* get() { return thread_instance(_tkey, true, nullptr); }

private:

    static T* thread_instance(thread_key& tk, bool module_local, T* obj) {
        T* p = reinterpret_cast<T*>(tk.get());
        if (!p) {
            fn_singleton_creator creator = obj == nullptr ? &create : singleton_local_creator(obj);

            //the module's registry creates an object at every registration; the process's would find another thread's
            //object by its type and line - a thread's object is registered there without a lookup, the key finds it
            p = (T*)(module_local
                ? singleton_register_instance(creator, &destroy, &singleton<T>::init_module,
                    typeid(T).name(), "", Line, true, Function_signature.value)
                : singleton_register_thread_instance(creator, &destroy, &singleton<T>::init_module,
                    typeid(T).name(), Line, Function_signature.value));
            tk.set(p);
        }
        return p;
    }

    /// @return the module's thread key - THREAD_SINGLETON's
    static thread_key& get_key() {
        static thread_key _gtkey;
        return _gtkey;
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
