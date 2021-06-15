#include "net_ul.h"
#include "profiler/profiler.h"
#include "taskmaster.h"

COID_NAMESPACE_BEGIN

const taskmaster::signal_handle taskmaster::invalid_signal = taskmaster::signal_handle(taskmaster::signal_handle::invalid); 

taskmaster::~taskmaster() {
    terminate(false);
}

void taskmaster::wait() {
    CPU_PROFILE_SCOPE_COLOR("taskmaster::wait", 0x80, 0, 0);
    std::unique_lock<std::mutex> lock(_sync);
    while (!_qsize) // handle spurious wake-ups
        _cv.wait(lock);
    --_qsize;
}

void taskmaster::run_task(invoker_base* task, int order)
{
    CPU_PROFILE_FUNCTION();
    uints id = _taskdata.get_item_id((granule*)task);
    //coidlog_devdbg("taskmaster", "thread " << order << " processing task id " << id);

#ifdef _DEBUG
    thread::set_name("<unknown task>"_T);
#endif

    DASSERT_RET(_taskdata.is_valid_id(id));
    task->invoke();

#ifdef _DEBUG
    thread::set_name("<no task>"_T);
#endif

    const signal_handle handle = task->signal();
    if (handle.is_valid()) {
        std::unique_lock<std::mutex> lock(_signal_sync);
        signal& s = _signal_pool[handle.index()];
        --s.ref;
        if (s.ref == 0) {
            s.version = (s.version + 1) % 0xffFF;
            signal_handle free_handle = signal_handle::make(s.version, handle.index());
            _free_signals.push(free_handle);
        }
    }

    _taskdata.del_range((granule*)task, align_to_chunks(task->size(), sizeof(granule)));
}

void* taskmaster::threadfunc( int order )
{
    uint notify_counter = 0;

    get_order() = order;

    thread::set_affinity_mask((uint64)1 << order);
    coidlog_info("taskmaster", "thread " << order << " running");
    char tmp[64];
    sprintf_s(tmp, "taskmaster %d", order);
    profiler::set_thread_name(tmp);

    do
    {
        wait();
        if (_quitting) break;

        invoker_base* task = 0;
        for(int prio = 0; prio < (int)EPriority::COUNT; ++prio) {
            const bool can_run = prio != (int)EPriority::LOW || order < _nlowprio_threads || order == -1;
            if (can_run && _ready_jobs[prio].pop(task)) {
                notify_counter = 0;
                run_task(task, order);
                break;
            }
        }
        
        if (!task) {
            // we did not pop any task, this might happen if there's a low prio task 
            // and thread can not process it, so let's wake other thread, hopefully one which 
            // can process low prio tasks
            notify();
            ++notify_counter;
            if (notify_counter > _threads.size() * 4) {
                // all low prio threads are most likely busy so we give up our timeslice since there might be 
                // other sleeping threads ready to work (e.g. jobmaster)
                thread::wait(0);
                notify_counter = 0;
            }
        }

    }
    while (!_quitting);

    coidlog_info("taskmaster", "thread " << order << " exiting");

    return 0;
}


bool taskmaster::is_signaled(signal_handle handle, bool lock)
{
    DASSERT_RET(handle.is_valid(), false);

    signal& s = _signal_pool[handle.index()];

    if (lock) _signal_sync.lock();
    const uint version = s.version;
    const uint ref = s.ref;
    if (lock) _signal_sync.unlock();

    return version != handle.version() || ref == 0;
}

taskmaster::signal_handle taskmaster::alloc_signal()
{
    signal_handle handle;
    if (!_free_signals.pop(handle)) return invalid_signal;

    signal& s = _signal_pool[handle.index()];
    s.ref = 1;

    return handle;
}

void taskmaster::increment(signal_handle* handle)
{
    std::unique_lock<std::mutex> lock(_signal_sync);
    if (!handle) return;

    if (handle->is_valid()) {
        signal& s = _signal_pool[handle->index()];
        if (is_signaled(*handle, false)) {
            *handle = alloc_signal();
        }
        else {
            ++s.ref;
        }
    }
    else {
        *handle = alloc_signal();
    }
}

void taskmaster::notify() {
    {
        std::unique_lock<std::mutex> lock(_sync);
        ++_qsize;
    }
    _cv.notify_one();
}

void taskmaster::notify(int n) {
    {
        std::unique_lock<std::mutex> lock(_sync);
        _qsize += n;
    }
    _cv.notify_all();
}

bool taskmaster::try_wait() {
    std::unique_lock<std::mutex> lock(_sync);
    if (_qsize) {
        --_qsize;
        return true;
    }
    return false;
}

COID_NAMESPACE_END
