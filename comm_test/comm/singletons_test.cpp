#include <comm/singleton.h>
#include <comm/pthreadx.h>

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

void singleton_test()
{
    test_global_singletons();
    test_local_singletons();
    test_local_singleton_defs();
    test_thread_global_singletons();
    test_thread_local_singletons();
    test_thread_local_singleton_defs();
}
