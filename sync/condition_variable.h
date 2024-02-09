#pragma once
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
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2023
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

#ifndef __COID___COMM_CONDVAR__HEADER_FILE__
#define __COID___COMM_CONDVAR__HEADER_FILE__

#include "../namespace.h"

#include "../comm.h"
#include "../commassert.h"
#include "../pthreadx.h"

#include <sys/timeb.h>

#include "_mutex.h"



COID_NAMESPACE_BEGIN

/// Condition variable is used to put to sleep one or more threads until they are awakened
/// (notified) by some other thread when some condition is met. Must be used along 
/// with locked mutex when putting the thread to sleep. The mutex unlocked when the
/// thread put to sleep and the lock is acquired again when the thread is awaken.
/// 
/// Example:
/// 
/// mutex comsumer;
/// mutex producer;
/// uint8 data[8];
/// atomic_bool data_ready = false;
/// condition_variable cv;
/// 
/// void producer_routine
/// {
///     producer.lock();
///     prepare_data();
///     data_ready = true;
///     producer.unlock()
///     cv.notify_one();
/// }
/// 
/// void comsumer_routine()
/// {
///     consumer.lock();
///     while(!data_ready)
///         cv.wait(consumer)
/// 
///     process_data();
/// }
/// 
/// Note that 'data_ready' is also under the lock and the notify call must be made after 
/// the given mutext is unlocked to the 'data_ready' value change propagate correctly to
/// the consumer thread

////////////////////////////////////////////////////////////////////////////////
class condition_variable
{
public:
#ifdef SYSTYPE_WIN
    __declspec(align(8)) struct win32_condition_variable
    {
#ifdef SYSTYPE_64
        static const size_t CV_SIZE = 8;
#else
        static const size_t CV_SIZE = 4;
#endif
        uint8   _tmp[CV_SIZE];
    };
#endif

private:
#ifdef SYSTYPE_WIN
    win32_condition_variable _cv;
#else
    pthread_cond_t _cv;
#endif

public:

    /// @brief Notifies one of the waiting thread
    void notify_one();
    /// @brief Notifies all of the waithing threads
    void notify_all();

    /// @brief Put the thread to sleep
    /// @param mx Locked mutex 
    void wait(_comm_mutex& mx);
    bool wait_for(_comm_mutex& mx, uint ms);

    condition_variable();
    ~condition_variable();

private:

    condition_variable(const condition_variable&);
};


COID_NAMESPACE_END

#endif // __COID___COMM_CONDVAR__HEADER_FILE__
