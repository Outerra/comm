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

#ifndef __COID_COMM_PTHREADX__HEADER_FILE__
#define __COID_COMM_PTHREADX__HEADER_FILE__


#include "retcodes.h"
#include "token.h"
#include "function.h"

#ifndef SYSTYPE_WIN
#   include <pthread.h>
#   include <semaphore.h>
#endif

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_WIN
    typedef uint                thread_t;
#else
    typedef pthread_t           thread_t;
#endif

////////////////////////////////////////////////////////////////////////////////
struct thread
{
private:
    thread_t    _thread;

public:
    typedef void* (*fnc_entry) (void*);

    struct Exception {};
    struct CancelException : Exception { uint _code; CancelException(uint code):_code(code) {} };
    struct ExitException : Exception {};

public:
    operator thread_t() const       { return _thread; }

    ///An invalid thread id (is_invalid) - comm's thread manager created
    thread();

    /// @param t a thread's id - not checked
    thread( thread_t t );

    /// @param t a thread's id - not checked
    thread& operator = (thread_t t) { _thread = t;  return *this; }


    /// @param t a thread's id
    /// @return true if it's the id this object holds
    bool operator == (thread_t t) const;

    /// @param t a thread's id
    /// @return true if it's another id than this object holds
    bool operator != (thread_t t) const  { return !(*this == t); }

    /// @return true if the object contains invalid thread id value (not that the thread is invalid)
    bool is_invalid() const;

    /// @return true while the thread runs its function - only a thread comm's thread manager started (create...)
    bool exists() const {
        return exists(_thread);
    }

    ///Sets the calling thread's name - the debugger's (Windows: SetThreadDescription) and the thread manager's logs'; only a
    ///thread comm's thread manager started (create...), another one keeps its name
    /// @param name the name
    static void set_name(const coid::token& name);

    /// @param tid a thread's id
    /// @return true while the thread runs its function - only a thread comm's thread manager started (create...)
    static bool exists( thread_t tid );

    /// @return invalid thread id
    static thread_t invalid();

    ///Spawn new thread, setting up this object with reference to the new thread
    /// @param fn function to execute - its result the thread's
    /// @param context thread context, queryable from thread (context)
    /// @param name the new thread's name (set_name) - set before fn runs; empty - none
    thread& create_fn( const function<void*()>& fn, void* context=0, const token& name = token() )
    {
        _thread = create_new_fn(fn, context, name);
        return *this;
    }

    ///Spawn new thread, returning the thread object
    /// @param fn function to execute - its result the thread's
    /// @param context thread context, queryable from thread (context)
    /// @param name the new thread's name (set_name) - set before fn runs; empty - none
    /// @return the new thread
    static thread create_new_fn( const function<void*()>& fn, void* context=0, const token& name = token() );

    ///Spawn new thread, setting up this object with reference to the new thread
    /// @param f function to execute - its result the thread's
    /// @param arg argument given to the function
    /// @param context thread context, queryable from thread (context)
    /// @param name the new thread's name (set_name) - set before f runs; empty - none
    thread& create( fnc_entry f, void* arg, void* context=0, const token& name = token() )
    {
        _thread = create_new(f, arg, context, name);
        return *this;
    }

    ///Spawn new thread, returning the thread object
    /// @param f function to execute - its result the thread's
    /// @param arg argument given to the function
    /// @param context thread context, queryable from thread (context)
    /// @param name the new thread's name (set_name) - set before f runs; empty - none
    /// @return the new thread
    static thread create_new( fnc_entry f, void* arg, void* context=0, const token& name = token() );

    ///Sets the calling thread's affinity - a hard limit: the thread runs only on the given logical processors
    /// @param mask the logical processors of the thread's processor group, a bit each (Windows: SetThreadAffinityMask - a
    ///        failure not reported)
    static void set_affinity_mask(uint64 mask);

    ///Sets the calling thread's ideal processor - a scheduling hint, not an affinity (Windows; no effect elsewhere)
    /// @param group the logical processor's processor group (Windows' - at most 64 logical processors each)
    /// @param number the logical processor's number in its group, 0 - 63
    /// @return true when it's set
    static bool set_ideal_processor(uint16 group, uint8 number);

    ///Lowers the calling thread's priority below normal or puts it back to normal - a background worker's (Windows; no
    ///effect elsewhere)
    /// @param background true - below normal, false - normal
    /// @return true when it's set
    static bool set_background_priority(bool background);

    /// @{ Static methods dealing with the thread currently running

    /// @return context info given when current thread was created
    static void* context();

    /// @return thread object for the thread we are currently in
    static thread self();

    ///Exits from the current thread - throws thread::CancelException, which comm's thread manager catches: the thread's
    ///function ends with the result 0
    /// @param code carried by the exception - the thread manager doesn't read it
    static void self_exit( uint code );

    ///Requests the current thread's cancellation - the flag self_should_cancel and self_test_cancel read
    static void self_cancel();

    /// @return true if the current thread should be cancelled
    static bool self_should_cancel();

    ///Exits from the current thread (self_exit) if its cancellation was requested
    /// @param exitcode self_exit's code
    static void self_test_cancel( uint exitcode );

    ///Sleeps the current thread
    /// @param ms milliseconds; 0 - gives up the rest of its time slice
    static void wait( uint ms );
    /// @}


    ///Request cancellation of the thread referred to by this object
    /// @note the thread must check itself if it should be cancelled
    /// @return 0 when requested; ersINVALID_PARAMS for a thread comm's thread manager doesn't run
    opcd cancel();

    /// @return true if the thread referred by this object should be cancelled
    bool should_cancel() const;

    ///Request cancellation of referred thread and wait for it to cancel itself
    /// @param mstimeout maximum time to wait for the thread to end, in milliseconds
    /// @return true if thread was successfully terminated in time, or comm's thread manager doesn't run it
    bool cancel_and_wait( uint mstimeout );

    ///Waits until a thread ends - any thread, comm's thread manager's or not
    /// @param tid the thread's id; an ended thread's or an invalid one returns at once
    static void join( thread_t tid );


protected:

/*
    static void cancel_callback()
    {
        throw CancelException();
    }
*/
};

typedef thread::Exception   ThreadException;

////////////////////////////////////////////////////////////////////////////////
///A thread-local slot - each thread's own value (Windows: TlsAlloc)
struct thread_key
{
#ifdef SYSTYPE_WIN
    uint _key;
#else
    pthread_key_t _key;
#endif

    thread_key();
    thread_key(const thread_key&) = delete;
    ~thread_key();

    ///Sets the calling thread's value
    /// @param v the value
    void set( void* v );

    /// @return the calling thread's value; nullptr until it's set
    void* get() const;
};

///Get per-thread on-demand created variable
/// @param tk its key
/// @return the calling thread's object - created by new T at its first call, never deleted
template<class T>
T* thread_object( thread_key& tk ) {
    T* t = (T*)tk.get();
    if(!t)
        tk.set( t = new T );

    return t;
}

////////////////////////////////////////////////////////////////////////////////
///A counting semaphore
struct thread_semaphore
{
#ifdef SYSTYPE_WIN
    uints _handle;
#else
    sem_t* _handle;
    int _init;
#endif

    /// @param initial its count (Windows: its maximum count too - a release above it is lost; 0 - not created, Windows'
    ///        maximum count can't be 0)
    explicit thread_semaphore( uint initial );

    ///Not created - init creates it
    explicit thread_semaphore( NOINIT_t );
    ~thread_semaphore();

    ///Creates a semaphore constructed with NOINIT
    /// @param initial its count (Windows: its maximum count too)
    /// @return false when it's created already
    bool init( uint initial );

    ///Waits until its count is above 0 and takes one
    /// @return true when it's taken
    bool acquire();

    ///Adds one to its count - wakes a waiting thread
    void release();
};

////////////////////////////////////////////////////////////////////////////////
///An auto-reset event - a wait passes once per signal (Windows; Linux: a semaphore - its signals counted, signaled at its
///creation)
struct thread_event
{
#ifdef SYSTYPE_WIN
    uints _handle;
#else
    sem_t* _handle;
    int _init;
#endif

    ///Not signaled (Windows)
    explicit thread_event();

    ///Not created - init creates it
    explicit thread_event( NOINIT_t );
    ~thread_event();

    ///Creates an event constructed with NOINIT
    /// @return false when it's created already
    bool init();

    ///Waits until it's signaled, resets it
    /// @return true when it was signaled
    bool wait();

    ///Resets it when it's signaled - doesn't wait
    /// @return true when it was signaled
    bool try_wait();

    ///Signals it - wakes a waiting thread or lets the next wait pass (Windows: one signal kept at most)
    void signal();
};

COID_NAMESPACE_END


#endif //__COID_COMM_PTHREADX__HEADER_FILE__
