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
#include "ref_i.h"
#include "log/logger.h"
#include "str.h"
#include "token.h"

#include <atomic>
#include <new>

COID_NAMESPACE_BEGIN

struct cpu_info;

/**
    Taskmaster manages a pool of worker threads and the queues of tasks they execute.

    The workers are of two kinds: the frame workers - the work a frame waits for - and the background
    workers - long jobs. Each task has a priority (task_priority_enum) that says which threads run it;
    a thread takes the critical tasks before the normal ones before the background ones, the tasks of
    one priority in the order they were pushed.

    Each task can be associated with a wait counter, which allows the user to wait until all tasks
    linked to that counter have completed. Multiple tasks may share the same wait counter, but a
    single task cannot be associated with more than one wait counter.

    While a thread is waiting on a wait counter, it processes other tasks: first the tasks pushed for
    its own lane (register_thread), then the shared ones it may run; when there's none, it sleeps until
    one comes or its counter is done.

    A task is destroyed right after it runs, before its wait counter is signaled. A task that throws
    is logged with its name, its wait counter signaled all the same. The tasks left at terminate()
    are destroyed without running, their wait counters signaled.

    The threads prefer the places plan_placement chooses for them on the CPU (an ideal processor - a
    scheduling hint, not an affinity): the frame workers the cores sharing the main thread's L3, the
    background workers the other cores, then the SMT siblings.

    Basic usage example:
        coid::taskmaster::wait_counter wait_counter;
        for (int i = 0; i < 10; ++i) {
            taskmaster->push(coid::taskmaster::task_priority_enum::normal, &wait_counter, [i]() { foo(i); });
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

    /// A thread's own queue of tasks (register_thread): the tasks pushed for it run only on that thread, while it waits
    using thread_lane = uint32;
    static constexpr thread_lane invalid_lane = UINT32_MAX;

    /// A task's priority (task_options::_priority) - what it's for and which threads run it. A thread takes the critical
    /// tasks before the normal ones before the background ones, the tasks of one priority in the order they were pushed.
    enum class task_priority_enum : uint8
    {
        /// Something waits for it right now - the current frame or a waiting thread: the frame's own work, the chunks of
        /// its parallel_for. Runs on the frame workers and the waiting threads, never on a background worker.
        /// Keep each task short - its waiter waits for the slowest one.
        critical,

        /// Needed soon, nothing waits for it yet: work for the next frames (a streaming's preparation), an asynchronous
        /// computation polled later. Runs on any worker and the waiting threads, after the critical tasks. The default.
        normal,

        /// Long and latency-tolerant: file loading, decompression, shader compiles. Runs only on the background workers -
        /// their cores off the frame's (another L3, a hybrid CPU's efficiency cores, SMT siblings), their OS priority below
        /// normal -, never on a frame worker or a waiting thread (but a background worker's own wait), so it can't delay a
        /// frame. Needs a background worker (a lane's task excepted).
        background,

        count,
    };

    /// How a task runs (push)
    struct task_options
    {
        task_priority_enum _priority = task_priority_enum::normal;
        wait_counter* _wait_counter_ptr = nullptr;      //< Signaled when the task is done, nullptr - none
        token_literal _name;                            //< Its profiler scope (a literal), empty - "task"
        thread_lane _lane = invalid_lane;               //< A thread's lane (register_thread) - the task runs only on that
                                                        //< thread, while it waits; invalid_lane - a shared task
    };

    /// A thread's place on the CPU - a physical core's logical processor, the thread's ideal processor
    /// (thread::set_ideal_processor)
    struct placement_slot
    {
        uint32 _core = UINT32_MAX;                      //< Its core - an index of cpu_info::_cores; UINT32_MAX - none
        uint8 _thread = 0;                              //< The core's logical processor: 0 its first, 1 its SMT sibling
        uint8 _number = 0;                              //< The logical processor's number in its group
        uint16 _group = 0;                              //< The logical processor's group

        bool is_set() const { return _core != UINT32_MAX; }
    };

    /// Where the threads prefer to run (plan_placement)
    struct placement
    {
        placement_slot _main;                           //< The thread that drives the frame - the main thread
        dynarray<placement_slot> _frame_workers;        //< A slot per frame worker
        dynarray<placement_slot> _background_workers;   //< A slot per background worker
    };

    /// plan_placement's count chosen by the CPU
    static constexpr uint32 default_count = UINT32_MAX;

    /// The background workers plan_placement chooses at most by the CPU
    static constexpr uint32 max_default_background_workers = 16;

    /// @brief Plans where the threads prefer to run on a CPU (system_info's topology) - pure:
    ///        - the frame domain: the cores sharing the largest L3 (the first of equal ones; all of them without an L3)
    ///        - the main thread: the frame domain's first core of its fastest class
    ///        - the frame workers: the frame domain's other cores of that class, one per core; more of them on the other
    ///          cores (the fastest first), then on the SMT siblings
    ///        - the background workers: the other cores (the other domains', the slower classes') one per core, then their
    ///          SMT siblings, then the frame workers' cores' siblings, then sharing the frame workers' cores (the last
    ///          first)
    ///        Without the topology (no cores) the slots aren't set.
    /// @param frame_workers default_count - the frame domain's cores of its fastest class but the main thread's, at least 1
    /// @param background_workers default_count - the other cores and their siblings - without them the frame workers' cores'
    ///        siblings -, at most max_default_background_workers, at least 1
    static placement plan_placement(const cpu_info& cpu, uint32 frame_workers = default_count, uint32 background_workers = default_count);

    /// @brief Appends a placement's description: "the main thread on core 0, 7 frame workers on cores 1-7, 16 background
    ///        workers on cores 8-15 and the second threads of cores 8-15"
    static void append_placement(charstr& out, const placement& plan);

    /// @brief Workers placed by a plan (plan_placement): the frame workers - the indices 0 - the frame workers' count - 1 -,
    ///        then the background workers, their OS priority below normal
    explicit taskmaster(const placement& plan);

    /// @param nthreads             Total number of job threads to spawn - with none the tasks run in the waits only.
    /// @param nbackground_threads  The background workers among them (must be <= nthreads); placed by plan_placement on
    ///                             the system's CPU.
    taskmaster(uint nthreads, uint nbackground_threads);

    ~taskmaster();

    uints get_workers_count() const { return _threads.size(); }
    uints get_frame_workers_count() const { return _frame_workers_count; }
    uints get_background_workers_count() const { return _threads.size() - _frame_workers_count; }

    /// @brief Gets the calling thread's index among the workers - the frame workers' first
    /// @return 0 - get_workers_count() - 1, -1 for a thread that isn't this taskmaster's worker
    int get_worker_index() const;

    /// @brief Registers the calling thread's lane - the tasks pushed for it (task_options::_lane) run only on it, while
    ///        it waits (wait, terminate)
    /// @return its lane - the same one for a thread registered already
    thread_lane register_thread();

    /// @brief Run fn(index) in parallel - a critical task per index
    /// @param first begin index value
    /// @param last end index value
    /// @param fn function(index) to run
    template <typename Index, typename Fn>
    void parallel_for(Index first, Index last, const Fn& fn) {
        wait_counter counter;
        for (; first != last; ++first) {
            push(task_priority_enum::critical, &counter, fn, first);
        }

        wait(counter);
    }

    /// @brief Runs fn(begin, end, slot) over [first, last) in chunks of grain items in parallel: as many critical tasks as
    ///        the frame workers and the calling thread, each taking the next chunk until none is left
    /// @param first begin index value (an integer)
    /// @param last end index value
    /// @param grain items in a chunk - at least 1
    /// @param fn function(Index begin, Index end, uint slot) - slot is its task's index, 0 - get_frame_workers_count(),
    ///        for per-task outputs (0 the calling thread's)
    template <typename Index, typename Fn>
    void parallel_for(Index first, Index last, Index grain, const Fn& fn) {
        if (!(first < last))
            return;

        const uints total = uints(last - first);
        const uints step = grain > 0 ? uints(grain) : 1;
        const uints chunk_count = (total + step - 1) / step;
        const uints slot_count = stdmin(chunk_count, get_frame_workers_count() + 1);
        std::atomic<uints> next_chunk = 0;
        const auto run_chunks = [&next_chunk, &fn, first, total, step, chunk_count](uint slot) {
            for (;;) {
                const uints chunk = next_chunk.fetch_add(1, std::memory_order_relaxed);
                if (chunk >= chunk_count)
                    return;

                const uints begin = chunk * step;
                const uints end = stdmin(begin + step, total);
                fn(Index(first + Index(begin)), Index(first + Index(end)), slot);
            }
        };

        wait_counter counter;
        const task_options options{ task_priority_enum::critical, &counter, "parallel_for" };
        for (uint slot = 1; slot < slot_count; ++slot) {
            push(options, run_chunks, slot);
        }

        run_chunks(0);
        wait(counter);
    }

    /// @brief Pushes a task (functor, e.g., lambda) into the queue for processing by worker threads.
    /// @param priority     Task priority (task_priority_enum) - what it's for and which threads run it.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Functor to execute - moved into the task when it's a temporary (its captures may be move-only),
    ///                     copied otherwise.
    template <typename Fn>
    void push_functor(task_priority_enum priority, wait_counter* wait_counter_ptr, Fn&& fn)
    {
        push_functor(task_options{ priority, wait_counter_ptr }, std::forward<Fn>(fn));
    }

    /// @brief Pushes a task (functor, e.g., lambda) - its priority, wait counter, name and lane in the options.
    template <typename Fn>
    void push_functor(const task_options& options, Fn&& fn)
    {
        emplace_task<invoker<std::decay_t<Fn>>>(options, std::forward<Fn>(fn));
    }

    /// @brief Pushes a task (function and its arguments) into the queue for processing by worker threads.
    /// @param priority     Task priority (task_priority_enum) - what it's for and which threads run it.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Function to execute - copied into the task.
    /// @param args         Arguments to pass to the function - copied into the task at the push (temporaries moved).
    template <typename Fn, typename ...Args>
    void push(task_priority_enum priority, wait_counter* wait_counter_ptr, const Fn& fn, Args&& ...args)
    {
        push(task_options{ priority, wait_counter_ptr }, fn, std::forward<Args>(args)...);
    }

    /// @brief Pushes a task (function and its arguments) - its priority, wait counter, name and lane in the options.
    template <typename Fn, typename ...Args>
    void push(const task_options& options, const Fn& fn, Args&& ...args)
    {
        push_functor(options, [fn = fn, ...args = std::forward<Args>(args)]() mutable {
            fn(args...);
        });
    }

    /// @brief Pushes a task (member function and its arguments) into the queue for processing by worker threads.
    /// @param priority     Task priority (task_priority_enum) - what it's for and which threads run it.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Member function to execute.
    /// @param obj          Pointer to the object on which to invoke the member function - the object itself, not a copy.
    /// @param args         Arguments to pass to the function - copied into the task at the push (temporaries moved).
    template <typename Fn, typename C, typename ...Args>
    void push_memberfn(task_priority_enum priority, wait_counter* wait_counter_ptr, Fn fn, C* obj, Args&& ...args)
    {
        static_assert(std::is_member_function_pointer<Fn>::value, "fn must be a function that can be invoked as ((*obj).*fn)(args)");

        push_functor(task_options{ priority, wait_counter_ptr }, [fn, obj, ...args = std::forward<Args>(args)]() mutable {
            (obj->*fn)(args...);
        });
    }

    /// @brief Pushes a task (member function and its arguments) into the queue for processing by worker threads.
    /// @param priority     Task priority (task_priority_enum) - what it's for and which threads run it.
    /// @param wait_counter Optional wait counter associated with this task. Can be nullptr if no synchronization is required.
    /// @param fn           Member function to execute.
    /// @param obj          The object on which to invoke the member function - copied into the task at the push. Can also
    ///                     be a smart pointer type (iref) that resolves to the object via the * operator - the task's copy
    ///                     keeps the object alive until it's done.
    /// @param args         Arguments to pass to the member function - copied into the task at the push (temporaries moved).
    template <typename Fn, typename C, typename ...Args>
    void push_memberfn(task_priority_enum priority, wait_counter* wait_counter_ptr, Fn fn, const C& obj, Args&& ...args)
    {
        static_assert(std::is_member_function_pointer<Fn>::value, "fn must be a function that can be invoked as ((*obj).*fn)(args)");

        push_functor(task_options{ priority, wait_counter_ptr }, [fn, obj = obj, ...args = std::forward<Args>(args)]() mutable {
            invoke_member(fn, obj, args...);
        });
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

    /// @brief Waits until the specified wait_counter reaches the signaled state - the thread runs the tasks it may
    ///        meanwhile (its lane's first), sleeps while there's none.
    /// @param wait_counter  The counter to wait for.
    /// @note Each time a task is pushed to the queue with an associated wait_counter,
    ///       the counter is incremented. When the task finishes, the counter is decremented.
    ///       Once the counter reaches zero, the wait_counter is considered signaled.
    ///       Multiple tasks can be associated with the same wait_counter.
    void wait(const wait_counter& wait_counter);

    /// @brief Terminates all task threads.
    /// @param empty_queue If true, waits until the task queue is empty before terminating (the calling thread helps);
    ///                    if false, only currently executing tasks are finished. The tasks left are destroyed without
    ///                    running, their wait counters signaled.
    void terminate(bool empty_queue);

protected:

    ///
    struct threadinfo
    {
        thread tid;
        taskmaster* master;
        threadinfo* next_idle;          //< The next idle worker of its list
        condition_variable* cv;         //< Its sleep while it's idle
        placement_slot slot;            //< Its ideal processor
        int order;
        bool is_background;             //< A background worker - the frame workers' first
        bool is_idle;                   //< In its idle list - asleep until a push takes it out

        threadinfo() : master(0), next_idle(0), cv(0), order(-1), is_background(false), is_idle(false)
        {}
    };

    ///Unit of allocation for tasks
    struct granule
    {
        uint8 dummy[8 * sizeof(void*)];
    };

    ///A task: its function behind the links of its queue
    struct invoker_base {
        virtual ~invoker_base() {}
        virtual void invoke() = 0;

        explicit invoker_base(const task_options& options)
            : _wait_counter_ptr(options._wait_counter_ptr)
            , _name(options._name.is_empty() ? token_literal("task") : options._name)
            , _lane(options._lane)
            , _priority(options._priority)
        {}

        invoker_base* _next = nullptr;                  //< The next task of its queue
        wait_counter* _wait_counter_ptr;                //< Signaled when it's done
        token_literal _name;                            //< Its profiler scope
        thread_lane _lane;                              //< Its thread's lane, invalid_lane - a shared task
        task_priority_enum _priority;
        uints _granules = 0;                            //< Its granules in _taskdata, 0 - on the heap
    };

    ///The tasks in the order they were queued - linked through themselves
    struct task_queue
    {
        invoker_base* _head = nullptr;
        invoker_base* _tail = nullptr;

        bool is_empty() const { return _head == nullptr; }

        void push_back(invoker_base* task) {
            task->_next = nullptr;
            if (_tail)
                _tail->_next = task;
            else
                _head = task;
            _tail = task;
        }

        invoker_base* pop_front() {
            invoker_base* task = _head;
            if (task) {
                _head = task->_next;
                if (!_head)
                    _tail = nullptr;
                task->_next = nullptr;
            }
            return task;
        }
    };

    ///A thread's lane (register_thread)
    struct lane_info
    {
        thread_t _thread_id = 0;
        task_queue _tasks;
    };

    ///A task's functor and its call - a lambda of push or push_memberfn (their arguments its captures) or any functor of
    ///push_functor
    template <typename Fn>
    struct invoker : invoker_base
    {
        template <typename F>
        invoker(const task_options& options, F&& fn)
            : invoker_base(options)
            , _fn(std::forward<F>(fn))
        {}

        void invoke() override final {
            _fn();
        }

    private:

        Fn _fn;
    };

    ///Calls a member function on an object, or on the object a smart pointer (iref) resolves to by its * operator
    template <typename Fn, typename C, typename ...Args>
    static void invoke_member(Fn fn, C& obj, Args& ...args) {
        if constexpr (requires(C& o, Fn f, Args& ...a) { (o.*f)(a...); })
            (obj.*fn)(args...);
        else
            ((*obj).*fn)(args...);
    }

private:

    taskmaster(const taskmaster&);

    static void* threadfunc(void* data) {
        threadinfo* ti = (threadinfo*)data;
        return ti->master->threadfunc(ti->order);
    }

    void* threadfunc(int order);

    /// @brief Constructs a task in its memory and queues it - constructed outside the lock: copying its arguments may push
    ///        tasks
    template <typename CallFn, typename ...CtorArgs>
    void emplace_task(const task_options& options, CtorArgs&& ...ctor_args)
    {
        DASSERT(options._priority != task_priority_enum::background || options._lane != invalid_lane || get_background_workers_count() > 0);

        uints granules = 0;
        void* memory = alloc_task_memory(sizeof(CallFn), granules);
        invoker_base* task = nullptr;
        try {
            task = new(memory) CallFn(options, std::forward<CtorArgs>(ctor_args)...);
        }
        catch (...) {
            free_task_memory(memory, granules);
            throw;
        }

        task->_granules = granules;
        enqueue(task);
    }

    /// @brief Checks whether the calling thread is this taskmaster's background worker
    bool is_background_worker() const;

    /// @brief Allocates a task's memory - granules of _taskdata, a task larger than their page on the heap
    /// @param granules_out its granules, 0 - on the heap
    void* alloc_task_memory(uints size, uints& granules_out);

    /// @brief Frees a task's memory (alloc_task_memory)
    void free_task_memory(void* memory, uints granules);

    /// @brief Frees a task's memory - _sync locked
    void free_task_memory_locked(void* memory, uints granules);

    /// @brief Queues a pushed task and wakes a thread that runs it - counted by its wait counter first
    void enqueue(invoker_base* task);

    /// @brief Takes the next task for a thread: its lane's first, then the shared ones it runs by priority - a background
    ///        worker's normal and background ones, another thread's critical and normal ones - _sync locked
    invoker_base* pop_locked(thread_lane lane, bool is_background);

    /// @brief Wakes a thread for a queued task - an idle worker that runs it, otherwise the waiting threads - _sync locked
    void wake_locked(const invoker_base* task);

    /// @brief Takes a worker out of its idle list - _sync locked
    void remove_idle_locked(threadinfo& ti);

    /// @brief Runs a task taken from its queue, then releases it (release_task) - _sync locked on entry and on return,
    ///        unlocked while the task runs
    void execute_locked(invoker_base* task);

    /// @brief Releases a task that doesn't run (release_task) - _sync locked on entry and on return
    /// @param is_counted a shared task (counted by _active_count)
    void drop_locked(invoker_base* task, bool is_counted);

    /// @brief Destroys a task, then signals its wait counter - its captures go before its waiter returns
    /// @param granules_out its memory's granules (free_task_memory_locked)
    /// @return true when its wait counter reached zero
    static bool release_task(invoker_base* task, uints& granules_out);

    /// @brief Waits until a condition holds - the calling thread runs the tasks it may meanwhile, sleeps while there's
    ///        none - _sync locked on entry and on return
    template <typename Done>
    void help_until_locked(comm_mutex_guard<comm_mutex>& lock, const Done& is_done);

    /// @brief Finds a thread's lane - _sync locked
    /// @return its lane, invalid_lane when it has none
    thread_lane find_lane_locked(thread_t id) const;

private:

    comm_mutex _sync;                   //< Guards the queues, the lanes, the idle lists, the tasks' memory and the counts
    condition_variable _wait_cv;        //< The threads asleep in a wait (wait, terminate)
    task_queue _ready_jobs[(int)task_priority_enum::count];    //< The shared tasks by priority
    dynarray<lane_info> _lanes;         //< The threads' lanes (register_thread)
    threadinfo* _idle_frame = nullptr;  //< The idle frame workers
    threadinfo* _idle_background = nullptr; //< The idle background workers
    uint _idle_waiters = 0;             //< The threads asleep in a wait
    uints _active_count = 0;            //< The shared tasks queued and every task running (terminate)
    std::atomic<bool> _quitting;
    bool _terminated = false;

    slotalloc_atomic<granule> _taskdata;

    dynarray<threadinfo> _threads;      //< The frame workers, then the background workers
    uints _frame_workers_count = 0;
};

COID_NAMESPACE_END
