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
* Portions created by the Initial Developer are Copyright (C) 2017-2023
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

#include "namespace.h"

#include "trait.h"
#include "alloc/slotalloc.h"
#include "bitrange.h"
#include "sync/queue.h"
#include "sync/mutex.h"
#include "sync/condition_variable.h"
#include "pthreadx.h"
#include "log/logger.h"

COID_NAMESPACE_BEGIN

/**
    Taskmaster manages a pool of worker threads and a queue of tasks to be executed by them.

    Tasks are processed according to their priority: higher-priority tasks are executed before
    lower-priority ones. LOW-priority tasks may be restricted to a limited subset of worker threads.

    Each job can be associated with a wait counter, which allows the user to wait until all jobs
    linked to that counter have completed. Multiple jobs may share the same wait counter, but a
    single job cannot be associated with more than one wait counter.

    While a thread is waiting on a wait counter, it will continue processing other tasks in the queue.

    Basic usage example:
        coid::taskmaster::wait_counter wait_counter;
        for (int i = 0; i < 10; ++i) {
            taskmaster->push(coid::taskmaster::EPriority::NORMAL, &wait_counter, [i]() { foo(i); });
        }
        taskmaster->wait(wait_counter);
**/

class taskmaster
{
public:
    struct critical_section
    {
        volatile int32 value = 0;
    };

    using wait_counter = std::atomic<uint32>; 

    enum class EPriority {
        HIGH,
        NORMAL,
        LOW,                            //< for limited "long run" threads

        COUNT
    };

    /// @param nthreads       Total number of job threads to spawn.
    /// @param nlong_threads  Number of low-priority job threads (must be <= nthreads).
   taskmaster(uint nthreads, uint nlowprio_threads);

    ~taskmaster();

    uints get_workers_count() const { return _threads.size(); }

    /// @brief Run fn(index) in parallel in task level 0
    /// @param first begin index value
    /// @param last end index value
    /// @param fn function(index) to run
    template <typename Index, typename Fn>
    void parallel_for(Index first, Index last, const Fn& fn) {
        wait_counter counter;
        for (; first != last; ++first) {
            push(EPriority::HIGH, &counter, fn, first);
        }

        wait(counter);
    }

    /// @brief Pushes a task (functor, e.g., lambda) into the queue for processing by worker threads.
    /// @param priority     Task priority. Higher-priority tasks are processed before lower-priority ones.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Functor to execute.
    template <typename Fn>
    void push_functor(EPriority priority, wait_counter* wait_counter_ptr, const Fn& fn)
    {
        push(priority, wait_counter_ptr, [](const Fn& fn) {
            fn();
            }, fn);
    }

    /// @brief Pushes a task (function and its arguments) into the queue for processing by worker threads.
    /// @param priority     Task priority. Higher-priority tasks are processed before lower-priority ones.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Function to execute.
    /// @param args         Arguments to pass to the function.
    template <typename Fn, typename ...Args>
    void push(EPriority priority, wait_counter* wait_counter_ptr, const Fn& fn, Args&& ...args)
    {
        using callfn = invoker<Fn, Args...>;

        {
            //lock to access allocator and semaphore
            comm_mutex_guard<comm_mutex> lock(_task_sync);

            granule* p = alloc_data(sizeof(callfn));
            if (wait_counter_ptr)
            {
                wait_counter_ptr->fetch_add(1, std::memory_order_relaxed);
            }
            auto task = new(p) callfn(wait_counter_ptr, fn, std::forward<Args>(args)...);

            _ready_jobs[(int)priority].push_front(task);

            ++_qsize;
            if (priority != EPriority::LOW) ++_hqsize;
        }
        _cv.notify_one();
        if (priority != EPriority::LOW) {
            _hcv.notify_one();
        }
    }

    /// @brief Pushes a task (function and its arguments) into the queue for processing by worker threads.
    /// @param priority     Task priority. Higher-priority tasks are processed before lower-priority ones.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Member function to execute.
    /// @param obj          Pointer to the object on which to invoke the member function.
    /// @param args         Arguments to pass to the function.
    template <typename Fn, typename C, typename ...Args>
    void push_memberfn(EPriority priority, wait_counter* wait_counter_ptr, Fn fn, C* obj, Args&& ...args)
    {
        static_assert(std::is_member_function_pointer<Fn>::value, "fn must be a function that can be invoked as ((*obj).*fn)(args)");

        using callfn = invoker_memberfn<Fn, C*, Args...>;

        {
            //lock to access allocator and semaphore
            comm_mutex_guard<comm_mutex> lock(_task_sync);

            granule* p = alloc_data(sizeof(callfn));
            if (wait_counter_ptr)
            {
                wait_counter_ptr->fetch_add(1, std::memory_order_relaxed);
            }
            auto task = new(p) callfn(wait_counter_ptr, fn, obj, std::forward<Args>(args)...);

            _ready_jobs[(int)priority].push_front(task);

            ++_qsize;
            if (priority != EPriority::LOW) ++_hqsize;
        }
        _cv.notify_one();
        if (priority != EPriority::LOW) {
            _hcv.notify_one();
        }
    }

    /// @brief Pushes a task (function and its arguments) into the queue for processing by worker threads.
    /// @param priority     Task priority. Higher-priority tasks are processed before lower-priority ones.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Member function to execute.
    /// @param obj          Reference to the object on which to invoke the member function. Can also be a smart pointer type 
    ///                     that resolves to the object via the * operator.
    /// @param args         Arguments to pass to the member function.
    template <typename Fn, typename C, typename ...Args>
    void push_memberfn(EPriority priority, wait_counter* wait_counter_ptr, Fn fn, const C& obj, Args&& ...args)
    {
        static_assert(std::is_member_function_pointer<Fn>::value, "fn must be a function that can be invoked as ((*obj).*fn)(args)");

        using callfn = invoker_memberfn<Fn, C, Args...>;

        {
            //lock to access allocator and semaphore
            comm_mutex_guard<comm_mutex> lock(_task_sync);

            granule* p = alloc_data(sizeof(callfn));
            if (wait_counter_ptr)
            {
                wait_counter_ptr->fetch_add(1, std::memory_order_relaxed);
            }
            auto task = new(p) callfn(wait_counter_ptr, fn, obj, std::forward<Args>(args)...);

            _ready_jobs[(int)priority].push_front(task);

            ++_qsize;
            if (priority != EPriority::LOW) ++_hqsize;
        }
        _cv.notify_one();
        if (priority != EPriority::LOW) {
            _hcv.notify_one();
        }
    }

    /// @brief Enters a critical section; ensures that no two threads are in the same critical section simultaneously.
    ///        While waiting to enter, the thread may continue process other enqueued tasks.
    /// @param spin_count Number of spins before attempting to process other tasks while waiting.
    /// @note Avoid calling enter(A), enter(B), exit(A), exit(B) in that order, as it can cause a deadlock
    ///       due to the Taskmaster's scheduling behavior.
    void enter_critical_section(critical_section& critical_section, int spin_count = 1024);

    /// @brief Leaves a critical section.
    /// @note Only the thread that entered the critical section may leave it.
    void leave_critical_section(critical_section& critical_section);

    /// @brief Waits until the specified wait_counter reaches the signaled state.
    /// @param wait_counter  The counter to wait for.
    /// @note Each time a task is pushed to the queue with an associated wait_counter,
    ///       the counter is incremented. When the task finishes, the counter is decremented.
    ///       Once the counter reaches zero, the wait_counter is considered signaled.
    ///       Multiple tasks can be associated with the same wait_counter.
    void wait(const wait_counter& wait_counter);

    /// @brief Terminates all task threads.
    /// @param empty_queue If true, waits until the task queue is empty before terminating;
    ///                    if false, only currently executing tasks are finished.
    void terminate(bool empty_queue);

protected:

    ///
    struct threadinfo
    {
        thread tid;
        taskmaster* master;

        int order;


        threadinfo() : master(0), order(-1)
        {}
    };

    ///Unit of allocation for tasks
    struct granule
    {
        uint8 dummy[8 * sizeof(void*)];
    };

    static int& get_order()
    {
        static thread_local int order = -1;
        return order;
    }

    granule* alloc_data(uints size)
    {
        uints n = align_to_chunks(size, sizeof(granule));
        granule* p = _taskdata.add_contiguous_range(n);

        //coidlog_devdbg("taskmaster", "pushed task id " << _taskdata.get_item_id(p));
        return p;
    }

    ///
    struct invoker_base {
        virtual void invoke() = 0;
        virtual size_t size() const = 0;

        invoker_base(wait_counter* wait_counter_ptr)
            : _wait_counter_ptr(wait_counter_ptr)
            , _tid(thread::self())
        {}

        std::atomic<uint>* get_wait_counter() const {
            return _wait_counter_ptr;
        }

    protected:
        std::atomic<uint32>* _wait_counter_ptr;
        thread_t _tid;
    };

    template <typename Fn, typename ...Args>
    struct invoker_common : invoker_base
    {
        invoker_common(wait_counter* wait_counter_ptr, const Fn& fn, Args&& ...args)
            : invoker_base(wait_counter_ptr)
            , _fn(fn)
            , _tuple(std::forward<Args>(args)...)
        {}

    protected:

        template <size_t ...I>
        void invoke_fn(index_sequence<I...>) {
            _fn(std::get<I>(_tuple)...);
        }

        template <class C, size_t ...I>
        void invoke_memberfn(C& obj, index_sequence<I...>) {
            (obj.*_fn)(std::get<I>(_tuple)...);
        }

    private:

        typedef std::tuple<std::remove_reference_t<Args>...> tuple_t;

        Fn _fn;
        tuple_t _tuple;
    };

    ///invoker for lambdas and functions
    template <typename Fn, typename ...Args>
    struct invoker : invoker_common<Fn, Args...>
    {
        invoker(wait_counter* wait_counter_ptr, const Fn& fn, Args&& ...args)
            : invoker_common<Fn, Args...>(wait_counter_ptr, fn, std::forward<Args>(args)...)
        {}

        void invoke() override final {
            this->invoke_fn(make_index_sequence<sizeof...(Args)>());
        }

        size_t size() const override final {
            return sizeof(*this);
        }
    };

    ///invoker for member functions (on copied objects)
    template <typename Fn, typename C, typename ...Args>
    struct invoker_memberfn : invoker_common<Fn, Args...>
    {
        invoker_memberfn(wait_counter* wait_counter_ptr, Fn fn, const C& obj, Args&&... args)
            : invoker_common<Fn, Args...>(wait_counter_ptr, fn, std::forward<Args>(args)...)
            , _obj(obj)
        {}

        invoker_memberfn(wait_counter* wait_counter_ptr, Fn fn, C&& obj, Args&&... args)
            : invoker_common<Fn, Args...>(wait_counter_ptr, fn, std::forward<Args>(args)...)
            , _obj(std::forward<C>(obj))
        {}

        void invoke() override final {
            this->invoke_memberfn(_obj, make_index_sequence<sizeof...(Args)>());
        }

        size_t size() const override final {
            return sizeof(*this);
        }

    private:

        C _obj;
    };

    ///invoker for member functions (on pointers)
    template <typename Fn, typename C, typename ...Args>
    struct invoker_memberfn<Fn, C*, Args...> : invoker_common<Fn, Args...>
    {
        invoker_memberfn(wait_counter* wait_counter_ptr, Fn fn, C* obj, Args&&... args)
            : invoker_common<Fn, Args...>(wait_counter_ptr, fn, std::forward<Args>(args)...)
            , _obj(obj)
        {}

        void invoke() override final {
            this->invoke_memberfn(*_obj, make_index_sequence<sizeof...(Args)>());
        }

        size_t size() const override final {
            return sizeof(*this);
        }

    private:

        C* _obj;
    };

    ///invoker for member functions (on irefs)
    template <typename Fn, typename C, typename ...Args>
    struct invoker_memberfn<Fn, iref<C>, Args...> : invoker_common<Fn, Args...>
    {
        invoker_memberfn(wait_counter* wait_counter_ptr, Fn fn, const iref<C>& obj, Args&&... args)
            : invoker_common<Fn, Args...>(wait_counter_ptr, fn, std::forward<Args>(args)...)
            , _obj(obj)
        {}

        void invoke() override final {
            this->invoke_memberfn(*_obj, make_index_sequence<sizeof...(Args)>());
        }

        size_t size() const override final {
            return sizeof(*this);
        }

    private:

        iref<C> _obj;
    };

private:

    taskmaster(const taskmaster&);

    static void* threadfunc(void* data) {
        threadinfo* ti = (threadinfo*)data;
        return ti->master->threadfunc(ti->order);
    }

    void* threadfunc(int order);
    void run_task(invoker_base* task, bool waiter);
    void notify_all();
    void wait_internal();

private:

    comm_mutex _task_sync;              //< mutex for queuing and dequeuing the tasks from queue
    comm_mutex _wait_sync;              //< mutex for waiting on condition variable
    condition_variable _cv;             //< for threads which can process low prio tasks
    condition_variable _hcv;            //< for threads which can not process low prio tasks
    std::atomic_int _qsize;             //< current queue size, used also as a semaphore
    std::atomic_int _hqsize;            //< current queue size without low prio tasks
    volatile bool _quitting;

    slotalloc_atomic<granule> _taskdata;

    dynarray<threadinfo> _threads;
    volatile int _nlowprio_threads;

    queue<invoker_base*> _ready_jobs[(int)EPriority::COUNT];
};

COID_NAMESPACE_END
