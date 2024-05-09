#include "net_ul.h"
#include "profiler/profiler.h"
#include "taskmaster.h"

COID_NAMESPACE_BEGIN


taskmaster::taskmaster(uint nthreads, uint nlowprio_threads)
    : _task_sync(2500, false)
    , _wait_sync(500, false)
    , _qsize(0)
    , _hqsize(0)
    , _quitting(false)
    , _nlowprio_threads(nlowprio_threads)
{
    _taskdata.reserve_virtual(8192 * 16);

    _threads.alloc(nthreads);
    _threads.for_each([&](threadinfo& ti, uints id) {
        ti.order = uint(id);
        ti.master = this;
        ti.tid.create(threadfunc, &ti, 0, "taskmaster");
        });
}

taskmaster::~taskmaster() {
    terminate(false);
}

void taskmaster::wait_internal() {
    CPU_PROFILE_SCOPE_COLOR(taskmaster_wait, 0x80, 0, 0);
    comm_mutex_guard<comm_mutex> lock(_wait_sync);
    if (get_order() < _nlowprio_threads) {
        while (!_qsize) // handle spurious wake-ups
            _cv.wait(lock);
    }
    else {
        while (!_hqsize) // handle spurious wake-ups
            _hcv.wait(lock);
    }
}

void taskmaster::run_task(invoker_base* task, bool waiter)
{
    CPU_PROFILE_FUNCTION();
    uints id = _taskdata.get_item_id((granule*)task);
    //coidlog_devdbg("taskmaster", "thread " << order << " processing task id " << id);

#ifdef _DEBUG
    if (!waiter)
        thread::set_name("<unknown task>"_T);
#endif

    DASSERT_RET(_taskdata.is_valid_id(id));
    task->invoke();

#ifdef _DEBUG
    if (!waiter)
        thread::set_name("<no task>"_T);
#endif

    wait_counter* wait_counter_ptr = task->get_wait_counter();
    if (wait_counter_ptr)
    {
        wait_counter_ptr->fetch_sub(1, std::memory_order_relaxed);
    }

    _taskdata.del_range(id, align_to_chunks(task->size(), sizeof(granule)));
}

void* taskmaster::threadfunc( int order )
{
    get_order() = order;

    thread::set_affinity_mask((uint64)1 << order);
    coidlog_info("taskmaster", "thread " << order << " running");
    char tmp[64];
    sprintf_s(tmp, "taskmaster %d", order);
    profiler::set_thread_name(tmp);

    wait_internal();
    while (!_quitting) {
        // TODO there are 3 locks here: wait, _ready_jobs.pop and lock(_sync), they can be merged into one
        invoker_base* task = 0;
        for (int prio = 0; prio < (int)EPriority::COUNT; ++prio) {
            const bool can_run = prio != (int)EPriority::LOW || (order < _nlowprio_threads && order != -1);
            if (can_run && _ready_jobs[prio].pop(task)) {
                {
                    //comm_mutex_guard<comm_mutex> lock(_sync);
                    --_qsize;
                    if (prio != (int)EPriority::LOW) --_hqsize;
                }
                run_task(task, false);
                break;
            }
        }
        wait_internal();
    }

    coidlog_info("taskmaster", "thread " << order << " exiting");

    return 0;
}

void taskmaster::notify_all() {
    _qsize.store(static_cast<int>(_threads.size()), std::memory_order_relaxed);
    _hqsize.store(static_cast<int>(_threads.size()), std::memory_order_relaxed);
    _cv.notify_all();
    _hcv.notify_all();
}

void taskmaster::enter_critical_section(critical_section& critical_section, int spin_count)
{
    for (;;) {
        for (int i = 0; i < spin_count; ++i) {
            if (critical_section.value == 0 && atomic::cas(&critical_section.value, 1, 0) == 0) {
                return;
            }
        }

        invoker_base* task = 0;
        const int order = get_order();
        for (int prio = 0; prio < (int)EPriority::COUNT; ++prio) {
            const bool can_run = prio != (int)EPriority::LOW || (order < _nlowprio_threads && order != -1);
            if (can_run && _ready_jobs[prio].pop(task)) {
                {
                    --_qsize;
                    if (prio != (int)EPriority::LOW) --_hqsize;
                }
                run_task(task, true);
                break;
            }
        }
        if (!task)
            thread::wait(0);
    }
}

void taskmaster::leave_critical_section(critical_section& critical_section)
{
    const int32 prev = atomic::cas(&critical_section.value, 0, 1);
    DASSERTN(prev == 1);
}


void taskmaster::wait(const wait_counter& wait_counter)
{
    const int order = get_order();
    while (wait_counter.load() != 0) {
        invoker_base* task = 0;
        for (int prio = 0; prio < (int)EPriority::COUNT; ++prio) {
            const bool can_run = prio != (int)EPriority::LOW || (order < _nlowprio_threads && order != -1);
            if (can_run && _ready_jobs[prio].pop(task)) {
                {
                    --_qsize;
                    if (prio != (int)EPriority::LOW) --_hqsize;
                }
                run_task(task, true);
                break;
            }
        }
        if (!task)
            thread::wait(0);
    }
}

void taskmaster::terminate(bool empty_queue)
{
    if (empty_queue) {
        while (_qsize > 0)
            thread::wait(0);
    }

    _quitting = true;

    notify_all();

    //wait for cancellation
    _threads.for_each([](threadinfo& ti) {
        thread::join(ti.tid);
        });
}

COID_NAMESPACE_END
