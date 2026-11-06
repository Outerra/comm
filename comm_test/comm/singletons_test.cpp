#include <comm/singleton.h>
#include <comm/pthreadx.h>
#include <comm/commassert.h>
#include <comm/alloc/commalloc.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

struct test_struct 
{
    int some_member = -1;
};

test_struct* get_global_singleton0()
{
    test_struct& result = SINGLETON(test_struct);
    return &result;
}

test_struct* get_global_singleton1()
{
    test_struct& result = SINGLETON(test_struct);
    return &result;
}

test_struct* get_local_singleton0() 
{
    LOCAL_SINGLETON(test_struct) result;
    return result.get();
}

test_struct* get_local_singleton1()
{
    LOCAL_SINGLETON(test_struct) result;
    return result.get();
}

test_struct* get_local_singleton_def0()
{
    LOCAL_SINGLETON_DEF(test_struct) result = new test_struct;
    return result.get();
}

test_struct* get_local_singleton_def1()
{
    LOCAL_SINGLETON_DEF(test_struct) result = new test_struct;
    return result.get();
}

test_struct* get_thread_global_singleton0()
{
    test_struct& result = THREAD_SINGLETON(test_struct);;
    return &result;
}

test_struct* get_thread_global_singleton1()
{
    test_struct& result = THREAD_SINGLETON(test_struct);;
    return &result;
}

test_struct* get_thread_local_singleton0()
{
    THREAD_LOCAL_SINGLETON_DEF(test_struct) result;
    return result.get();
}

test_struct* get_thread_local_singleton1()
{
    THREAD_LOCAL_SINGLETON_DEF(test_struct) result;
    return result.get();
}

test_struct* get_thread_local_singleton_def0()
{
    THREAD_LOCAL_SINGLETON_DEF(test_struct) result = new test_struct();
    return result.get();
}

test_struct* get_thread_local_singleton_def1()
{
    THREAD_LOCAL_SINGLETON_DEF(test_struct) result = new test_struct();
    return result.get();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void test_global_singletons()
{
    bool result = get_global_singleton0() == get_global_singleton1();
    DASSERT(result);
}

void test_local_singletons()
{
    bool result = get_local_singleton0() != get_local_singleton1();
    DASSERT(result);
}

void test_local_singleton_defs()
{
    bool result = get_local_singleton_def0() != get_local_singleton_def1();
    DASSERT(result);
}

struct thread_arg
{
    test_struct* ptr_0 = nullptr;
    test_struct* ptr_1 = nullptr;
};

void* test_thread_global_singletons_thread_fnc(void* arg)
{
    thread_arg* out = reinterpret_cast<thread_arg*>(arg);
    out->ptr_0 = get_thread_global_singleton0();
    out->ptr_1 = get_thread_global_singleton1();
    return nullptr;
}

void* test_thread_local_singletons_thread_fnc(void* arg)
{
    thread_arg* out = reinterpret_cast<thread_arg*>(arg);
    out->ptr_0 = get_thread_local_singleton0();
    out->ptr_1 = get_thread_local_singleton1();
    return nullptr;
}

void* test_thread_local_singleton_defs_thread_fnc(void* arg)
{
    thread_arg* out = reinterpret_cast<thread_arg*>(arg);
    out->ptr_0 = get_thread_local_singleton_def0();
    out->ptr_1 = get_thread_local_singleton_def1();
    return nullptr;
}


void test_thread_global_singletons()
{
    thread_arg t0_arg;
    thread_arg t1_arg;

    coid::thread t0, t1;
    t0.create(test_thread_global_singletons_thread_fnc, &t0_arg);
    t1.create(test_thread_global_singletons_thread_fnc, &t1_arg);

    coid::thread::join(t0);
    coid::thread::join(t1);

    DASSERT(t0_arg.ptr_0 == t0_arg.ptr_1);
    DASSERT(t0_arg.ptr_1 != t1_arg.ptr_0);
    DASSERT(t1_arg.ptr_0 == t1_arg.ptr_1);
}

void test_thread_local_singletons()
{
    thread_arg t0_arg;
    thread_arg t1_arg;

    coid::thread t0, t1;
    t0.create(test_thread_local_singletons_thread_fnc, &t0_arg);
    t1.create(test_thread_local_singletons_thread_fnc, &t1_arg);

    coid::thread::join(t0);
    coid::thread::join(t1);

    DASSERT(t0_arg.ptr_0 != t0_arg.ptr_1);
    DASSERT(t1_arg.ptr_0 != t1_arg.ptr_1);
    DASSERT(t0_arg.ptr_0 != t1_arg.ptr_0);
    DASSERT(t0_arg.ptr_1 != t1_arg.ptr_1);
}

void test_thread_local_singleton_defs()
{
    thread_arg t0_arg;
    thread_arg t1_arg;

    coid::thread t0, t1;
    t0.create(test_thread_local_singleton_defs_thread_fnc, &t0_arg);
    t1.create(test_thread_local_singleton_defs_thread_fnc, &t1_arg);

    coid::thread::join(t0);
    coid::thread::join(t1);

    DASSERT(t0_arg.ptr_0 != t0_arg.ptr_1);
    DASSERT(t1_arg.ptr_0 != t1_arg.ptr_1);
    DASSERT(t0_arg.ptr_0 != t1_arg.ptr_0);
    DASSERT(t0_arg.ptr_1 != t1_arg.ptr_1);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
/*
    Tests of SINGLETON's and PROCWIDE_SINGLETON's first calls (singleton::instance), of the process registry's keys and of
    the process-wide thread and local singletons.

    Concurrent first calls get one instance: SINGLETON's registered under the module registry's lock (the module's
    registry creates an object at every registration), PROCWIDE_SINGLETON's found by the process's registry under its
    lock, whichever line made the first call; each macro has its own module pointer - SINGLETON(T) the module's object,
    PROCWIDE_SINGLETON(T) the process's. The process's registry finds a PROCWIDE_SINGLETON's instance by its type alone:
    another module's first call from another line (registered as that module's singleton::instance registers it) gets
    the same instance and initializes it for its module. The other keys stay: the process-wide local singletons by their
    function and line, a module's SINGLETON its own.

    THREAD_PROCWIDE_SINGLETON gives each thread its own object, the same in every module (the process's thread key); a
    process-wide local singleton of two modules is one instance - the second module's object deleted.

    The concurrent tests release their threads together at a type whose construction takes 50 ms, so every thread makes
    its first call before the first construction ends. Each test has types of its own - a type's first call happens once
    in a process. Like ref_tests, these assert with RASSERT, which is live in every configuration.
*/

///The tests' singleton types - in a named namespace: the process's registry finds an instance by its type's name, the
/// same for the anonymous namespaces' types of every file
namespace singleton_test_types {

///A singleton type counting its constructions and its initializations for other modules; Tag gives each test its own
/// types, a construction takes ConstructionMs
template <int Tag, int ConstructionMs = 0>
struct counted_singleton
{
    static inline std::atomic<int> constructed = 0;
    static inline std::atomic<int> modules_initialized = 0;

    counted_singleton() {
        constructed.fetch_add(1);
        if (ConstructionMs)
            std::this_thread::sleep_for(std::chrono::milliseconds(ConstructionMs));
    }

    void singleton_initialize_module() { modules_initialized.fetch_add(1); }
};

///A local singleton's type counting its objects' constructions and destructions; Tag gives each test its own types
template <int Tag>
struct counted_object
{
    static inline std::atomic<int> constructed = 0;
    static inline std::atomic<int> destroyed = 0;

    counted_object() { constructed.fetch_add(1); }
    counted_object(counted_object&&) noexcept { constructed.fetch_add(1); }
    ~counted_object() { destroyed.fetch_add(1); }
};

} //namespace singleton_test_types

namespace {

using singleton_test_types::counted_singleton;
using singleton_test_types::counted_object;

constexpr int construction_ms = 50;
constexpr int thread_count = 8;

///Calls fn(thread index) from thread_count threads released together
/// @return the pointers the calls returned, by thread
template <class Fn>
std::vector<void*> call_from_threads(Fn fn)
{
    std::vector<void*> results(thread_count, nullptr);
    std::latch start(thread_count);
    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i)
        threads.emplace_back([&, i] {
            start.arrive_and_wait();
            results[i] = fn(i);
        });

    for (std::thread& t : threads)
        t.join();

    return results;
}

///@return true when every pointer is instance
bool all_same_as(const std::vector<void*>& pointers, void* instance)
{
    for (void* p : pointers)
        if (p != instance)
            return false;
    return true;
}

///Registers T in the process's registry as another module's first PROCWIDE_SINGLETON(T) does - from another file and line
template <class T>
void* register_procwide_in_another_module()
{
    return coid::singleton_register_instance(
        []() -> void* { return new T; }, [](void* p) { delete static_cast<T*>(p); }, &coid::singleton<T>::init_module,
        typeid(T).name(), "another_module.cpp", 4321, false, "");
}

///Registers T in the process's registry as an older comm's THREAD_PROCWIDE_SINGLETON(T) does - a thread's object for a
/// lookup by its type and line -1, without a file
template <class T>
void* register_thread_in_older_module()
{
    return coid::singleton_register_instance(
        []() -> void* { return new T; }, [](void* p) { delete static_cast<T*>(p); }, &coid::singleton<T>::init_module,
        typeid(T).name(), "", -1, false, "");
}

///Registers T in the module's registry as a first SINGLETON(T) does
template <class T>
void* register_in_this_module()
{
    return coid::singleton_register_instance(
        []() -> void* { return new T; }, [](void* p) { delete static_cast<T*>(p); }, &coid::singleton<T>::init_module,
        typeid(T).name(), "this_module.cpp", 1234, true, "");
}

///SINGLETON's concurrent first calls: one construction, every thread the module's instance
void singleton_first_calls_from_threads()
{
    using type = counted_singleton<1, construction_ms>;

    std::vector<void*> results = call_from_threads([](int) -> void* { return &SINGLETON(type); });

    RASSERT(type::constructed == 1);
    RASSERT(all_same_as(results, &SINGLETON(type)));
}

///PROCWIDE_SINGLETON's concurrent first calls from two lines: one construction, every thread the same instance
void procwide_singleton_first_calls_from_two_lines()
{
    using type = counted_singleton<2, construction_ms>;

    std::vector<void*> results = call_from_threads([](int i) -> void* {
        if (i % 2)
            return &PROCWIDE_SINGLETON(type);
        return &PROCWIDE_SINGLETON(type);
    });

    RASSERT(type::constructed == 1);
    RASSERT(all_same_as(results, &PROCWIDE_SINGLETON(type)));
}

///SINGLETON's and PROCWIDE_SINGLETON's concurrent first calls of one type: each macro its own instance - the module's and
/// the process's (their own module pointers); comm's allocations the module's mspace, logmsg's the process's
void singleton_and_procwide_singleton_first_calls_from_threads()
{
    using type = counted_singleton<3, construction_ms>;

    std::vector<void*> results = call_from_threads([](int i) -> void* {
        return i % 2 ? &SINGLETON(type) : &PROCWIDE_SINGLETON(type);
    });

    void* module = &SINGLETON(type);
    void* process = &PROCWIDE_SINGLETON(type);

    RASSERT(module != process);
    for (int i = 0; i < thread_count; ++i)
        RASSERT(results[i] == (i % 2 ? module : process));
    RASSERT(type::constructed == 2);

    RASSERT(&SINGLETON(coid::comm_array_mspace) != &PROCWIDE_SINGLETON(coid::comm_array_mspace));
}

///SINGLETON_EXISTS: the module's SINGLETON or PROCWIDE_SINGLETON
void singleton_exists()
{
    using type = counted_singleton<10>;

    RASSERT(!SINGLETON_EXISTS(type));
    PROCWIDE_SINGLETON(type);
    RASSERT(SINGLETON_EXISTS(type));
}

///Another module's first PROCWIDE_SINGLETON call from another line, after this module's and before it: the process's
/// instance, initialized for the other module
void procwide_singleton_of_another_module()
{
    {
        using type = counted_singleton<4>;

        type& here = PROCWIDE_SINGLETON(type);
        void* there = register_procwide_in_another_module<type>();

        RASSERT(there == &here);
        RASSERT(type::constructed == 1 && type::modules_initialized == 1);
    }
    {
        using type = counted_singleton<5>;

        void* there = register_procwide_in_another_module<type>();
        type& here = PROCWIDE_SINGLETON(type);

        RASSERT(&here == there);
        RASSERT(type::constructed == 1 && type::modules_initialized == 1);
    }
}

using local_key_type = counted_singleton<6>;

void* local_procwide_singleton_a()
{
    LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(local_key_type) instance;
    return instance.get();
}

void* local_procwide_singleton_b()
{
    LOCAL_FUNCTION_PROCWIDE_SINGLETON_DEF(local_key_type) instance;
    return instance.get();
}

///The other keys: two functions' process-wide local singletons apart, PROCWIDE_SINGLETON's instance none of them nor a
/// process-wide thread singleton's (an older comm's either - registered for a lookup by its line); a module's SINGLETON
/// its own - the module's registry creates an object at every registration, the process's doesn't have it
void registry_keys()
{
    {
        using type = local_key_type;

        void* a = local_procwide_singleton_a();
        void* b = local_procwide_singleton_b();
        void* procwide = &PROCWIDE_SINGLETON(type);
        void* thread = &THREAD_PROCWIDE_SINGLETON(type);
        void* older_thread = register_thread_in_older_module<type>();

        RASSERT(a != b);
        RASSERT(procwide != a && procwide != b);
        RASSERT(thread != a && thread != b && thread != procwide);
        RASSERT(older_thread != a && older_thread != b && older_thread != procwide && older_thread != thread);
        RASSERT(type::constructed == 5);
    }
    {
        using type = counted_singleton<7>;

        void* mine = &SINGLETON(type);
        void* first = register_in_this_module<type>();
        void* second = register_in_this_module<type>();
        void* process = register_procwide_in_another_module<type>();

        RASSERT(first != mine && second != mine && first != second);
        RASSERT(process != mine && process != first && process != second);
        RASSERT(type::constructed == 4);
    }
}

///THREAD_PROCWIDE_SINGLETON: an object per thread - the same at its every call; THREAD_SINGLETON's another one
void thread_procwide_singleton_per_thread()
{
    using type = counted_singleton<8>;

    std::vector<void*> results = call_from_threads([](int) -> void* {
        type* first = &THREAD_PROCWIDE_SINGLETON(type);
        type* again = &THREAD_PROCWIDE_SINGLETON(type);
        type* module = &THREAD_SINGLETON(type);
        return first == again && first != module ? first : nullptr;
    });

    for (size_t i = 0; i < results.size(); ++i) {
        RASSERT(results[i] != nullptr);
        for (size_t j = 0; j < i; ++j)
            RASSERT(results[i] != results[j]);
    }
    RASSERT(type::constructed == 2 * thread_count);
}

///THREAD_PROCWIDE_SINGLETON of another module on this thread: its thread key from the process's registry (registered from
/// another file and line) - this thread's object
void thread_procwide_singleton_of_another_module()
{
    using type = counted_singleton<9>;
    using key_type = coid::thread_singleton_key<type>;

    type& here = THREAD_PROCWIDE_SINGLETON(type);
    key_type* key = static_cast<key_type*>(register_procwide_in_another_module<key_type>());

    RASSERT(key->key.get() == &here);
}

using shared_local = coid::singleton<counted_object<11>, "singletons_test shared local", 11, false>;
using shared_local_moved = coid::singleton<counted_object<12>, "singletons_test shared local moved", 12, false>;

void* shared_local_of_one_module()
{
    static shared_local instance = new counted_object<11>;
    return instance.get();
}

void* shared_local_of_another_module()
{
    static shared_local instance = new counted_object<11>;
    return instance.get();
}

void* shared_local_moved_of_one_module()
{
    static shared_local_moved instance = counted_object<12>();
    return instance.get();
}

void* shared_local_moved_of_another_module()
{
    static shared_local_moved instance = counted_object<12>();
    return instance.get();
}

///A process-wide local singleton of two modules (two statics of one key - two modules' copies of a function): one
/// instance, the second module's object deleted - made by new and moved in
void local_procwide_singleton_of_two_modules()
{
    {
        using type = counted_object<11>;

        void* first = shared_local_of_one_module();
        void* second = shared_local_of_another_module();

        RASSERT(first == second);
        RASSERT(type::constructed == 2 && type::destroyed == 1);
    }
    {
        using type = counted_object<12>;

        void* first = shared_local_moved_of_one_module();
        void* second = shared_local_moved_of_another_module();

        RASSERT(first == second);
        RASSERT(type::constructed == 4 && type::destroyed == 3);   //the temporaries and the second module's object
    }
}

} //namespace

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void singleton_test()
{
    test_global_singletons();
    test_local_singletons();
    test_local_singleton_defs();
    test_thread_global_singletons();
    test_thread_local_singletons();
    test_thread_local_singleton_defs();

    singleton_first_calls_from_threads();
    procwide_singleton_first_calls_from_two_lines();
    singleton_and_procwide_singleton_first_calls_from_threads();
    singleton_exists();
    procwide_singleton_of_another_module();
    registry_keys();
    thread_procwide_singleton_per_thread();
    thread_procwide_singleton_of_another_module();
    local_procwide_singleton_of_two_modules();
}
