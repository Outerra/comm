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
#include "pthreadx.h"

COID_NAMESPACE_BEGIN

class taskmaster
{
public:

    taskmaster(uint nthreads) {
        _threads.alloc(nthreads);
        _threads.for_each([&](thread& tid) {
            uints id = &tid - _threads.ptr();
            tid.create(threadfunc, this, (void*)id, "taskmaster");
        });
    }

    //taskmaster(taskmaster&& other) {
    //    //_taskdata
    //}

    ///Push task into queue
    //@param fn function to run
    //@param args arguments needed to invoke the function
    template <typename Fn, typename ...Args>
    void push( const Fn& fn, Args&& ...args )
    {
        using callfn = invoker<Fn, Args...>;

        void* p = _taskdata.add_range_uninit(align_to_chunks(sizeof(callfn), sizeof(uint64)));

        auto task = new(p) callfn(fn, std::forward<Args>(args)...);
        _queue.push(task);
    }

    ///Push task into queue
    //@param fn member function to run
    //@param args arguments needed to invoke the function
    template <typename Fn, typename C, typename ...Args>
    void push_memberfn(Fn fn, C* obj, Args&& ...args)
    {
        using callfn = invoker_memberfn<Fn, C, Args...>;

        void* p = _taskdata.add_range_uninit(align_to_chunks(sizeof(callfn), sizeof(uint64)));

        auto task = new(p) callfn(fn, obj, std::forward<Args>(args)...);
        _queue.push(task);
    }

    ///Terminate task threads
    //@param empty_queue if true, wait until the task queue empties
    void terminate( bool empty_queue )
    {
        if (empty_queue) {
            while (!_queue.is_empty())
                thread::wait(0);
        }

        //signal cancellation
        _threads.for_each([](thread& tid) {
            tid.cancel();
        });

        //wait for cancellation
        _threads.for_each([](thread& tid) {
            tid.join(tid);
        });
    }

    //void invoke() {
    //    reinterpret_cast<invoker_root*>(_taskdata.get_item(0))->invoke();
    //}

protected:

    struct invoker_base {
        virtual void invoke() = 0;
    };

    template <typename Fn, typename ...Args>
    struct invoker_common : invoker_base
    {
        invoker_common( const Fn& fn, Args&& ...args )
            : _fn(fn)
            , _tuple(std::forward<Args>(args)...)
        {}

    protected:

        template <size_t ...I>
        void invoke_fn(index_sequence<I...>) {
            _fn(std::get<I>(_tuple)...);
        }

        template <class C, size_t ...I>
        void invoke_memberfn(C* obj, index_sequence<I...>) {
            (obj->*_fn)(std::get<I>(_tuple)...);
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
        invoker( const Fn& fn, Args&& ...args )
            : invoker_common(fn, std::forward<Args>(args)...)
        {}

        void invoke() override final {
            invoke_fn(make_index_sequence<sizeof...(Args)>());
        }
    };

    ///invoker for member functions
    template <typename Fn, typename C, typename ...Args>
    struct invoker_memberfn : invoker_common<Fn, Args...>
    {
        invoker_memberfn(Fn fn, C* obj, Args&&... args)
            : invoker_common(fn, std::forward<Args>(args)...)
            , _obj(obj)
        {}

        void invoke() override final {
            invoke_memberfn(_obj, make_index_sequence<sizeof...(Args)>());
        }

    private:

        C* _obj;
    };


private:

    static void* threadfunc(void* data) {
        return static_cast<taskmaster*>(data)->threadfunc();
    }

    void* threadfunc() {
        //thread order number
        uint id = (uint)thread::context();

        while (!thread::self_should_cancel())
        {
            invoker_base* task = 0;
            if (_queue.pop(task)) {
                task->invoke();
            }
            else
                thread::wait(1);
        }

        return 0;
    }

private:

    slotalloc<uint64> _taskdata;

    queue<invoker_base*> _queue;

    dynarray<thread> _threads;
};

COID_NAMESPACE_END
