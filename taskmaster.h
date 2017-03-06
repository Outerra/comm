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
* Portions created by the Initial Developer are Copyright (C) 2017
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
#include "sync/guard.h"
#include "pthreadx.h"
#include "log/logger.h"

COID_NAMESPACE_BEGIN

/**
    Taskmaster runs a set of job threads and a queue of jobs that are processed by the job threads.
    It's intended for short tasks that can be parallelized across the preallocated number of working
    threads, but it also supports long-duration jobs that can run on a limited number of worker threads
    and have a lower priority.

    Additionally, jobs can be synchronized or unsynchronized.

    - long duration threads will prioritize short jobs if available
    - thread waiting for completion of given set of jobs will also partake in processing of the same
      type of jobs
    - there can be multiple threads that wait for completion of jobs
    - if there's a thread waiting for job completion, all worker threads prioritize given job type
    - if there are multiple completion requests at different synchronization levels, the one with
      the highest priority (lowest sync level number) is prioritized over the other

**/
class taskmaster
{
public:

    //@param nthreads total number of job threads to spawn
    //@param nlong_threads number of long-duration job threads (<= nthreads)
    taskmaster( uint nthreads, uint nlong_threads )
        : _write_sync(500, false, "taskmaster")
        , _nlong_threads(nlong_threads)
    {
        _taskdata.reserve(8192);
        _synclevels.reserve(16, false);

        _threads.alloc(nthreads);
        _threads.for_each([&](threadinfo& ti) {
            ti.order = uint(&ti - _threads.ptr());
            ti.master = this;
            ti.tid.create(threadfunc, &ti, 0, "taskmaster");
        });
    }

    ///Set number of threads that can process long-duration jobs
    //@param nlong_threads number of long-duration job threads
    void set_long_duration_threads( uint nlong_threads ) {
        _nlong_threads = nlong_threads;
    }

    ///Add synchronization level
    //@param level sync level number (0 highest priority for completion)
    //@param name sync level name
    //@return level
    uint add_sync_level( uint level, const token& name ) {
        sync_level& sl = _synclevels.get_or_add(level);
        sl.name = name;
        sl.njobs = 0;

        return level;
    }

    ///Push task into queue
    //@param sync synchronization stage to push into (<0 unsynced long duration job, >=0 sync level
    //@param fn function to run
    //@param args arguments needed to invoke the function
    template <typename Fn, typename ...Args>
    void push( int sync, const Fn& fn, Args&& ...args )
    {
        using callfn = invoker<Fn, Args...>;

        granule* p = alloc_data(sizeof(callfn));
        auto task = new(p) callfn(sync, fn, std::forward<Args>(args)...);

        push_to_queue(task);
    }

    ///Push task into queue
    //@param sync synchronization stage to push into (<0 unsynced long duration job, >=0 sync level
    //@param fn member function to run
    //@param args arguments needed to invoke the function
    template <typename Fn, typename C, typename ...Args>
    void push_memberfn( int sync, Fn fn, const C& obj, Args&& ...args )
    {
        using callfn = invoker_memberfn<Fn, C, Args...>;

        granule* p = alloc_data(sizeof(callfn));
        auto task = new(p) callfn(sync, fn, obj, std::forward<Args>(args)...);

        push_to_queue(task);
    }

    ///Terminate task threads
    //@param empty_queue if true, wait until the task queue empties
    void terminate(bool empty_queue)
    {
        if (empty_queue) {
            while (!_queue.is_empty())
                thread::wait(0);
        }

        //signal cancellation
        _threads.for_each([](threadinfo& ti) {
            ti.tid.cancel();
        });

        //wait for cancellation
        _threads.for_each([](threadinfo& ti) {
            thread::join(ti.tid);
        });
    }

protected:

    ///
    struct threadinfo
    {
        thread tid;
        taskmaster* master;

        int order;


        threadinfo() : master(0), order(-1)
        {}

        bool is_long_duration_thread() const {
            return order < master->_nlong_threads;
        }
    };

    ///
    struct sync_level
    {
        std::atomic_int njobs;          //< number of jobs that need completing on this sync level

        charstr name;
    };

    ///Unit of allocation for tasks
    struct granule
    {
        uint64 dummy[2 * sizeof(uints) / 4];
    };

    granule* alloc_data(uints size)
    {
        auto base = _taskdata.get_array().ptr();

        uints n = align_to_chunks(size, sizeof(granule));
        granule* p;

        {
            GUARDTHIS(_write_sync);
            p = _taskdata.add_range_uninit(n);
        }

        //no rebase
        DASSERT(base == _taskdata.get_array().ptr());

        coidlog_devdbg("taskmaster", "pushed task id " << _taskdata.get_item_id(p));
        return p;
    }

    ///
    struct invoker_base {
        virtual void invoke() = 0;
        virtual size_t size() const = 0;

        invoker_base(int sync)
            : _sync(sync)
        {}

        int sync_level() const {
            return _sync;
        }

    protected:
        int _sync;
    };

    template <typename Fn, typename ...Args>
    struct invoker_common : invoker_base
    {
        invoker_common(int sync, const Fn& fn, Args&& ...args)
            : invoker_base(sync)
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
            ((*obj).*_fn)(std::get<I>(_tuple)...);
        }

    private:

        typedef std::tuple<Args...> tuple_t;

        Fn _fn;
        tuple_t _tuple;
    };

    ///invoker for lambdas and functions
    template <typename Fn, typename ...Args>
    struct invoker : invoker_common<Fn, Args...>
    {
        invoker(int sync, const Fn& fn, Args&& ...args)
            : invoker_common(sync, fn, std::forward<Args>(args)...)
        {}

        void invoke() override final {
            invoke_fn(make_index_sequence<sizeof...(Args)>());
        }

        size_t size() const override final {
            return sizeof(*this);
        }
    };

    ///invoker for member functions
    template <typename Fn, typename C, typename ...Args>
    struct invoker_memberfn : invoker_common<Fn, Args...>
    {
        invoker_memberfn(int sync, Fn fn, const C& obj, Args&&... args)
            : invoker_common(sync, fn, std::forward<Args>(args)...)
            , _obj(obj)
        {}

        void invoke() override final {
            invoke_memberfn(_obj, make_index_sequence<sizeof...(Args)>());
        }

        size_t size() const override final {
            return sizeof(*this);
        }

    private:

        C _obj;
    };


    void push_to_queue(invoker_base* task)
    {
        int sync = task->sync_level();

        if (sync >= 0) {
            DASSERT(sync < (int)_synclevels.size());

            if (sync < (int)_synclevels.size())
                ++_synclevels[sync].njobs;
        }

        _queue.push(task);
    }

private:

    taskmaster(const taskmaster&);

    static void* threadfunc(void* data) {
        threadinfo* ti = (threadinfo*)data;
        return ti->master->threadfunc(ti->order);
    }

    void* threadfunc( uint order )
    {
        while (!thread::self_should_cancel())
        {
            invoker_base* task = 0;
            if (_queue.pop(task)) {
                uints id = _taskdata.get_item_id((granule*)task);
                coidlog_devdbg("taskmaster", "thread " << order << " popped task id " << id);

                DASSERT(_taskdata.is_valid_id(id));
                task->invoke();

                _taskdata.del_range((granule*)task, align_to_chunks(task->size(), sizeof(granule)));
            }
            else
                thread::wait(1);
        }

        return 0;
    }

private:

    comm_mutex _write_sync;

    slotalloc<granule> _taskdata;

    queue<invoker_base*> _queue;

    dynarray<threadinfo> _threads;
    volatile int _nlong_threads;

    dynarray<sync_level> _synclevels;
};

COID_NAMESPACE_END
