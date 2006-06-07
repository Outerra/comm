
#include "pthreadx.h"
#include "sync/thread_mgr.h"
#include "singleton.h"

#ifdef SYSTYPE_WIN32
#   include <windows.h>
#   include <process.h>
#endif



COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
thread::thread()
{
#ifdef SYSTYPE_MSVC
    _thread = UMAX;
#else
    _thread = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread::thread( thread_t t )
{
    _thread = t;
}

////////////////////////////////////////////////////////////////////////////////
int thread::operator == (thread_t t) const
{
#ifdef SYSTYPE_MSVC
    return _thread == t;
#else
    return pthread_equal( t, _thread );
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread thread::self()
{
#ifdef SYSTYPE_MSVC
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread_t thread::invalid()
{
#ifdef SYSTYPE_MSVC
    return UMAX;
#else
    return 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool thread::is_invalid() const
{
#ifdef SYSTYPE_MSVC
    return _thread == UMAX;
#else
    return _thread == 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
void thread::_end( uint v )
{
#ifdef SYSTYPE_MSVC
    _endthreadex(v);
#else
    pthread_exit((void*)v);
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread thread::create_new( fnc_entry f, void* arg, void* context )
{
    return SINGLETON(thread_manager).thread_create( arg, f, context );
}

////////////////////////////////////////////////////////////////////////////////
void thread::exit_self( uint code )
{
    throw thread::CancelException();
    //SINGLETON(thread_manager).thread_delete( self() );
    //_end(code);
}

////////////////////////////////////////////////////////////////////////////////
opcd thread::cancel()
{
    return SINGLETON(thread_manager).request_cancellation(_thread);
}

////////////////////////////////////////////////////////////////////////////////
void thread::cancel_self()
{
    SINGLETON(thread_manager).request_cancellation( self() );
}

////////////////////////////////////////////////////////////////////////////////
bool thread::should_cancel() const
{
    return SINGLETON(thread_manager).test_cancellation(_thread);
}

////////////////////////////////////////////////////////////////////////////////
bool thread::should_cancel_self()
{
    return SINGLETON(thread_manager).test_cancellation( self() );
}

////////////////////////////////////////////////////////////////////////////////
void thread::test_cancel_self( uint exitcode )
{
    if( SINGLETON(thread_manager).test_cancellation( self() ) )
        exit_self(exitcode);
}

////////////////////////////////////////////////////////////////////////////////
void thread::join( thread_t tid )
{
    //this is a cancellation point too
    while(1)
    {
        if( !SINGLETON(thread_manager).thread_exists(tid) )
            return;

        test_cancel_self(0);
        sysMilliSecondSleep(20);
    }
}







////////////////////////////////////////////////////////////////////////////////
thread thread_manager::thread_start( thread_manager::info* ti )
{
    thread_t tid;

#ifdef SYSTYPE_MSVC
    ti->handle = (void*) CreateThread( 0, 0, (LPTHREAD_START_ROUTINE)thread_manager::def_thread, ti, 0, (ulong*)&tid );
#else
    pthread_create( &tid, 0, thread_manager::def_thread, ti );
#endif

    ti->tid = tid;
    return tid;
}

////////////////////////////////////////////////////////////////////////////////
void* thread_manager::def_thread( void* pinfo )
{
    info* ti = (info*)pinfo;

    //wait until the spawner inserts the id
    while( ti->tid == thread::invalid() )
        sysMilliSecondSleep(0);

    ti->mgr->thread_register(ti);


    //we're not using pthread's cancellation
    //pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, 0 );
    //pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, 0 );

    void* res;
    try {
        res = ti->entry( ti->arg );
    }
    catch(...)
    {
        res = 0;
    }

    ti->mgr->thread_unregister( ti->tid );
    delete ti;

    return res;
}







////////////////////////////////////////////////////////////////////////////////
thread_key::thread_key()
{
#ifdef SYSTYPE_WIN32
    _key = TlsAlloc();
#else
    pthread_key_create( &_key, 0 );
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread_key::~thread_key()
{
#ifdef SYSTYPE_WIN32
    TlsFree(_key);
#else
    pthread_key_delete( _key );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void thread_key::set( void* v )
{
#ifdef SYSTYPE_WIN32
    TlsSetValue( _key, v );
#else
    pthread_setspecific( _key, v );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void* thread_key::get() const
{
#ifdef SYSTYPE_WIN32
    return TlsGetValue(_key);
#else
    return pthread_getspecific( _key );
#endif
}





////////////////////////////////////////////////////////////////////////////////
thread_semaphore::thread_semaphore( uint initial )
{
#ifdef SYSTYPE_WIN32
    _handle = (uint)CreateSemaphore( 0, initial, initial, 0 );
#else
    RASSERT( 0 == sem_init( &_handle, false, initial ) );
    _init = 1;
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread_semaphore::thread_semaphore( NOINIT_t )
{
#ifdef SYSTYPE_WIN32
    _handle = (uint)INVALID_HANDLE_VALUE;
#else
    _init = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread_semaphore::~thread_semaphore()
{
#ifdef SYSTYPE_WIN32
    if( _handle != (uint)INVALID_HANDLE_VALUE )
        CloseHandle( (HANDLE)_handle );
#else
    if(_init)
        sem_destroy( &_handle );
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool thread_semaphore::init( uint initial )
{
#ifdef SYSTYPE_WIN32
    if( _handle != (uint)INVALID_HANDLE_VALUE )
        return false;
    _handle = (uint)CreateSemaphore( 0, initial, initial, 0 );
#else
    if(_init)
        return false;
    RASSERT( 0 == sem_init( &_handle, false, initial ) );
    _init = 1;
#endif
    return true;
}


////////////////////////////////////////////////////////////////////////////////
bool thread_semaphore::acquire()
{
#ifdef SYSTYPE_WIN32
    return WAIT_OBJECT_0 == WaitForSingleObject( (HANDLE)_handle, INFINITE );
#else
    return 0 == sem_wait( &_handle );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void thread_semaphore::release()
{
#ifdef SYSTYPE_WIN32
    ReleaseSemaphore( (HANDLE)_handle, 1, 0 );
#else
    sem_post( &_handle );
#endif
}


COID_NAMESPACE_END
