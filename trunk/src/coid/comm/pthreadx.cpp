
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
    SINGLETON(thread_manager);

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
bool thread::operator == (thread_t t) const
{
#ifdef SYSTYPE_MSVC
    return _thread == t;
#else
    return pthread_equal( t, _thread ) != 0;
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
thread thread::create_new( fnc_entry f, void* arg, void* context, const token& name )
{
    return SINGLETON(thread_manager).thread_create( arg, f, context, name );
}

////////////////////////////////////////////////////////////////////////////////
void thread::self_exit( uint code )
{
    throw thread::CancelException(code);
    //SINGLETON(thread_manager).thread_delete( self() );
    //_end(code);
}

////////////////////////////////////////////////////////////////////////////////
opcd thread::cancel()
{
    return SINGLETON(thread_manager).request_cancellation(_thread);
}

////////////////////////////////////////////////////////////////////////////////
void thread::self_cancel()
{
    SINGLETON(thread_manager).request_cancellation( self() );
}

////////////////////////////////////////////////////////////////////////////////
bool thread::should_cancel() const
{
    return SINGLETON(thread_manager).test_cancellation(_thread);
}

////////////////////////////////////////////////////////////////////////////////
bool thread::self_should_cancel()
{
	return SINGLETON(thread_manager).self_test_cancellation();
}

////////////////////////////////////////////////////////////////////////////////
void thread::self_test_cancel( uint exitcode )
{
    if( SINGLETON(thread_manager).self_test_cancellation() )
        self_exit(exitcode);
}

////////////////////////////////////////////////////////////////////////////////
bool thread::cancel_and_wait( uint mstimeout )
{
    if( ersINVALID_PARAMS == cancel() )
        return true;

    sysMilliSecondSleep(0);

    while(mstimeout) {
        if( !exists() )
            return true;

        sysMilliSecondSleep(1);
        --mstimeout;
    }

    return !exists();
}

////////////////////////////////////////////////////////////////////////////////
void thread::join( thread_t tid )
{
    //this is a cancellation point too
    while(1)
    {
        if( !SINGLETON(thread_manager).thread_exists(tid) )
            return;

        self_test_cancel(0);
        sysMilliSecondSleep(20);
    }
}

////////////////////////////////////////////////////////////////////////////////
bool thread::exists( thread_t tid )
{
    return SINGLETON(thread_manager).thread_exists(tid);
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
typedef struct tagTHREADNAME_INFO
{
	uint dwType;    // must be 0x1000
	const char* szName; // pointer to name (in user addr space)
	uint dwThreadID; // thread ID (-1=caller thread)
	uint dwFlags; // reserved for future use, must be zero
} THREADNAME_INFO;


//! Usage: set_thread_name (-1, "MainThread");
static void set_thread_name(const uint dwThreadID, const char * const szThreadName)
{
#ifdef SYSTYPE_WIN32
	THREADNAME_INFO info = {0x1000, szThreadName, dwThreadID, 0};

	__try {
		RaiseException(
			0x406D1388, 0, sizeof(info) / sizeof(DWORD), (DWORD*)&info );
	}
	__except(EXCEPTION_CONTINUE_EXECUTION) {
	}
#endif
}


////////////////////////////////////////////////////////////////////////////////
void* thread_manager::def_thread( void* pinfo )
{
    info* ti = (info*)pinfo;

    if( !ti->name.is_empty() )
        set_thread_name( -1, ti->name.c_str() );

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
	catch(thread::CancelException &) {
		res = 0;
	}
    catch(...) {
		DASSERT(false && "unknown exception thrown!");
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
    _handle = (uints)CreateSemaphore( 0, initial, initial, 0 );
#else
    _handle = new sem_t;
    RASSERT( 0 == sem_init( _handle, false, initial ) );
    _init = 1;
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread_semaphore::thread_semaphore( NOINIT_t )
{
#ifdef SYSTYPE_WIN32
    _handle = (uints)INVALID_HANDLE_VALUE;
#else
    _init = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
thread_semaphore::~thread_semaphore()
{
#ifdef SYSTYPE_WIN32
    if( _handle != (uints)INVALID_HANDLE_VALUE )
        CloseHandle( (HANDLE)_handle );
#else
    if(_init)
        sem_destroy( _handle );
    delete _handle;
#endif
}

////////////////////////////////////////////////////////////////////////////////
bool thread_semaphore::init( uint initial )
{
#ifdef SYSTYPE_WIN32
    if( _handle != (uints)INVALID_HANDLE_VALUE )
        return false;
    _handle = (uints)CreateSemaphore( 0, initial, initial, 0 );
#else
    if(_init)
        return false;
    _handle = new sem_t;
    RASSERT( 0 == sem_init( _handle, false, initial ) );
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
    return 0 == sem_wait( _handle );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void thread_semaphore::release()
{
#ifdef SYSTYPE_WIN32
    ReleaseSemaphore( (HANDLE)_handle, 1, 0 );
#else
    sem_post( _handle );
#endif
}


COID_NAMESPACE_END
