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
#include "net_ul.h"

#ifndef SYSTYPE_WIN32
#   include <pthread.h>
#   include <semaphore.h>
#endif

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
#ifdef SYSTYPE_MSVC
    typedef uint                thread_t;
#else
    typedef pthread_t           thread_t;
#endif

////////////////////////////////////////////////////////////////////////////////
struct thread
{
    typedef void* (*fnc_entry) (void*);

    struct Exception {};
    struct CancelException : Exception {};
    struct ExitException : Exception {};


    thread_t    _thread;


    operator thread_t() const       { return _thread; }

    thread();
    thread( thread_t );

    thread& operator = (thread_t t) { _thread = t;  return *this; }

    
    int operator == (thread_t t) const;
    int operator != (thread_t t) const  { return !(*this == t); }


    bool is_invalid() const;

    static thread self();
    static thread_t invalid();

    thread& create( fnc_entry f, void* arg, void* context=0 )
    {
        _thread = create_new( f, arg, context );
        return *this;
    }
    
    static thread create_new( fnc_entry f, void* arg, void* context=0 );
    static void exit_self( uint code );

    static bool exists( thread_t tid );

    opcd cancel();
    static void cancel_self();

    bool should_cancel() const;
    static bool should_cancel_self();
    static void test_cancel_self( uint exitcode );

    static void join( thread_t tid );

protected:

    static void _end( uint v );

    static void cancel_callback()
    {
        throw CancelException();
    }
};

//for backward compatibility
typedef thread::Exception       ThreadException;

////////////////////////////////////////////////////////////////////////////////
struct thread_key
{
#ifdef SYSTYPE_WIN32
    uint _key;
#else
    pthread_key_t _key;
#endif

    thread_key();
    ~thread_key();

    void set( void* v );
    void* get() const;
};

////////////////////////////////////////////////////////////////////////////////
struct thread_semaphore
{
#ifdef SYSTYPE_WIN32
    uints _handle;
#else
    sem_t* _handle;
    int _init;
#endif

    explicit thread_semaphore( uint initial );
    explicit thread_semaphore( NOINIT_t );
    ~thread_semaphore();

    bool init( uint initial );

    bool acquire();
    void release();
};

COID_NAMESPACE_END


#endif //__COID_COMM_PTHREADX__HEADER_FILE__
