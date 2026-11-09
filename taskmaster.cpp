#include "net_ul.h"
#include "profiler/profiler.h"
#include "taskmaster.h"
#include "commexception.h"
#include "system/system_info.h"

#include <algorithm>
#include <exception>
#include <thread>

COIDLOG_MODULE(taskmaster);

COID_NAMESPACE_BEGIN


namespace {

/// The calling thread's taskmaster and its index among the workers - a worker's
thread_local const taskmaster* t_master = nullptr;
thread_local int t_order = -1;

/// The number of a mask's n-th set bit (0 - its lowest), 64 for none
uint8 nth_set_bit(uint64 mask, uint8 n)
{
    for (uint8 bit = 0; bit < 64; ++bit) {
        if (mask & (uint64(1) << bit)) {
            if (n == 0)
                return bit;
            --n;
        }
    }

    return 64;
}

/// Appends the slots of cores' logical processors - a core without the logical processor left out
void append_slots(const cpu_info& cpu, const dynarray<uint32>& cores, uint8 thread, dynarray<taskmaster::placement_slot>& slots)
{
    for (uints i = 0; i < cores.size(); ++i) {
        const cpu_affinity& processors = cpu._cores[cores[i]]._logical_processors;
        const uint8 number = nth_set_bit(processors._mask, thread);
        if (number >= 64)
            continue;

        taskmaster::placement_slot* slot = slots.add();
        *slot = taskmaster::placement_slot();
        slot->_core = cores[i];
        slot->_thread = thread;
        slot->_number = number;
        slot->_group = processors._group;
    }
}

/// Appends the workers of a kind and their places: "7 frame workers on cores 1-7", "16 background workers on cores 8-15
/// and the second threads of cores 8-15"
void append_workers(charstr& out, const dynarray<taskmaster::placement_slot>& slots, const char* kind)
{
    out << slots.size() << ' ' << kind << (slots.size() == 1 ? " worker" : " workers");
    for (uints i = 0; i < slots.size();) {
        // a run of one logical processor of the cores
        const bool is_set = slots[i].is_set();
        const uint8 thread = slots[i]._thread;
        uints end = i + 1;
        while (end < slots.size() && slots[end].is_set() == is_set && slots[end]._thread == thread)
            ++end;

        if (!is_set) {
            out << (i == 0 ? " anywhere" : " and anywhere");
            i = end;
            continue;
        }

        out << (i == 0 ? " on " : " and ");
        const bool is_single = end - i == 1;
        if (thread == 0)
            out << (is_single ? "core " : "cores ");
        else
            out << (is_single ? "the second thread of core " : "the second threads of cores ");

        // the cores - consecutive ones as a range
        for (uints j = i; j < end;) {
            uints k = j + 1;
            while (k < end && slots[k]._core == slots[k - 1]._core + 1)
                ++k;

            if (j > i)
                out << ", ";

            out << slots[j]._core;
            if (k - j > 1)
                out << '-' << slots[k - 1]._core;

            j = k;
        }

        i = end;
    }
}

} // end of anonymous namespace


taskmaster::placement taskmaster::plan_placement(const cpu_info& cpu, uint32 frame_workers, uint32 background_workers)
{
    placement plan;
    const uint32 core_count = cpu._cores.size();
    if (core_count == 0) {
        // no topology - the counts by the logical processors (the main thread's core left out), no places
        const uint32 logical = stdmax(uint32(std::thread::hardware_concurrency()), 2u);
        const uint32 frames = frame_workers != default_count ? frame_workers : stdmax(logical / 2, 2u) - 1;
        const uint32 backgrounds = background_workers != default_count ? background_workers : 1;
        for (uint32 i = 0; i < frames; ++i)
            *plan._frame_workers.add() = placement_slot();
        for (uint32 i = 0; i < backgrounds; ++i)
            *plan._background_workers.add() = placement_slot();
        return plan;
    }

    // the frame domain - the largest L3 (the first of equal ones); each core's L3 - the one its logical processors share
    uint32 frame_domain = UINT32_MAX;
    uint32 largest = 0;
    for (uint32 k = 0; k < cpu._caches.size(); ++k) {
        if (cpu._caches[k]._level == 3 && cpu._caches[k]._size > largest) {
            largest = cpu._caches[k]._size;
            frame_domain = k;
        }
    }

    dynarray<uint32> domains;
    for (uint32 c = 0; c < core_count; ++c) {
        const cpu_affinity& core = cpu._cores[c]._logical_processors;
        uint32 domain = UINT32_MAX;
        for (uint32 k = 0; k < cpu._caches.size() && domain == UINT32_MAX; ++k) {
            const cpu_affinity& shared = cpu._caches[k]._logical_processors;
            if (cpu._caches[k]._level == 3 && shared._group == core._group && (shared._mask & core._mask) != 0)
                domain = k;
        }

        *domains.add() = domain;
    }

    // the main thread - the frame domain's first core of its fastest class (the first core when no core shares the L3)
    uint8 fastest = 0;
    for (uint32 c = 0; c < core_count; ++c) {
        if (domains[c] == frame_domain && cpu._cores[c]._efficiency_class > fastest)
            fastest = cpu._cores[c]._efficiency_class;
    }

    uint32 main_core = UINT32_MAX;
    for (uint32 c = 0; c < core_count && main_core == UINT32_MAX; ++c) {
        if (domains[c] == frame_domain && cpu._cores[c]._efficiency_class == fastest)
            main_core = c;
    }

    if (main_core == UINT32_MAX) {
        main_core = 0;
        frame_domain = domains[0];
        fastest = cpu._cores[0]._efficiency_class;
    }

    dynarray<uint32> main_cores;
    *main_cores.add() = main_core;
    dynarray<placement_slot> main_slot;
    append_slots(cpu, main_cores, 0, main_slot);
    if (main_slot.size() > 0)
        plan._main = main_slot[0];

    // the frame domain's other cores of its fastest class; the other cores - the fastest first, the system's order among
    // equal ones
    dynarray<uint32> frame_cores;
    dynarray<uint32> other_cores;
    for (uint32 c = 0; c < core_count; ++c) {
        if (c == main_core)
            continue;

        if (domains[c] == frame_domain && cpu._cores[c]._efficiency_class == fastest)
            *frame_cores.add() = c;
        else
            *other_cores.add() = c;
    }

    std::stable_sort(other_cores.ptr(), other_cores.ptr() + other_cores.size(), [&cpu](uint32 a, uint32 b) {
        return cpu._cores[a]._efficiency_class > cpu._cores[b]._efficiency_class;
        });

    // the frame workers: the frame cores, then the other cores, then the SMT siblings of both
    dynarray<placement_slot> frame_candidates;
    append_slots(cpu, frame_cores, 0, frame_candidates);
    append_slots(cpu, other_cores, 0, frame_candidates);
    append_slots(cpu, frame_cores, 1, frame_candidates);
    append_slots(cpu, other_cores, 1, frame_candidates);

    const uint32 frames = frame_workers != default_count ? frame_workers : stdmax(uint32(frame_cores.size()), 1u);
    dynarray<bool> is_frame_core;
    for (uint32 c = 0; c < core_count; ++c)
        *is_frame_core.add() = false;

    dynarray<uint32> frame_workers_cores;
    for (uint32 i = 0; i < frames; ++i) {
        const placement_slot slot = frame_candidates.size() > 0 ? frame_candidates[i % frame_candidates.size()] : placement_slot();
        *plan._frame_workers.add() = slot;
        if (slot.is_set() && slot._thread == 0 && !is_frame_core[slot._core]) {
            is_frame_core[slot._core] = true;
            *frame_workers_cores.add() = slot._core;
        }
    }

    // the background workers: the other cores the frame workers don't take, then their SMT siblings, then the frame workers'
    // cores' siblings, then sharing the frame workers' cores (the last first)
    dynarray<uint32> free_cores;
    for (uints i = 0; i < other_cores.size(); ++i) {
        if (!is_frame_core[other_cores[i]])
            *free_cores.add() = other_cores[i];
    }

    dynarray<placement_slot> background_candidates;
    append_slots(cpu, free_cores, 0, background_candidates);
    append_slots(cpu, free_cores, 1, background_candidates);
    const uint32 own_count = uint32(background_candidates.size());
    append_slots(cpu, frame_workers_cores, 1, background_candidates);
    const uint32 sibling_count = uint32(background_candidates.size()) - own_count;

    dynarray<uint32> shared_cores;
    for (uints i = frame_workers_cores.size(); i-- > 0;)
        *shared_cores.add() = frame_workers_cores[i];
    append_slots(cpu, shared_cores, 0, background_candidates);

    uint32 backgrounds = background_workers;
    if (backgrounds == default_count)
        backgrounds = stdmax(stdmin(own_count > 0 ? own_count : sibling_count, max_default_background_workers), 1u);

    for (uint32 i = 0; i < backgrounds; ++i) {
        *plan._background_workers.add() = background_candidates.size() > 0 ? background_candidates[i % background_candidates.size()]
            : placement_slot();
    }

    return plan;
}

void taskmaster::append_placement(charstr& out, const placement& plan)
{
    if (plan._main.is_set())
        out << "the main thread on core " << plan._main._core << ", ";

    append_workers(out, plan._frame_workers, "frame");
    out << ", ";
    append_workers(out, plan._background_workers, "background");
}

taskmaster::taskmaster(const placement& plan)
    : _sync(2500, false)
    , _quitting(false)
{
    _taskdata.reserve_virtual(8192 * 16);

    const uints frames = plan._frame_workers.size();
    _frame_workers_count = frames;
    _threads.alloc(frames + plan._background_workers.size());
    _threads.for_each([&](threadinfo& ti, uints id) {
        ti.order = int(id);
        ti.master = this;
        ti.cv = new condition_variable;
        ti.is_background = id >= frames;
        ti.slot = id < frames ? plan._frame_workers[id] : plan._background_workers[id - frames];
        });

    _threads.for_each([&](threadinfo& ti) {
        ti.tid.create(threadfunc, &ti, 0, "taskmaster");
        });
}

taskmaster::taskmaster(uint nthreads, uint nbackground_threads)
    : taskmaster(plan_placement(system_info::get_instance().get_cpu_info(), nthreads - stdmin(nbackground_threads, nthreads),
        stdmin(nbackground_threads, nthreads)))
{
}

taskmaster::~taskmaster() {
    terminate(false);

    _threads.for_each([](threadinfo& ti) {
        delete ti.cv;
        ti.cv = 0;
        });
}

int taskmaster::get_worker_index() const
{
    return t_master == this ? t_order : -1;
}

bool taskmaster::is_background_worker() const
{
    const int order = get_worker_index();
    return order >= 0 && uints(order) >= _frame_workers_count;
}

taskmaster::thread_lane taskmaster::register_thread()
{
    const thread_t id = thread::self();
    comm_mutex_guard<comm_mutex> lock(_sync);
    const thread_lane lane = find_lane_locked(id);
    if (lane != invalid_lane)
        return lane;

    lane_info* info = _lanes.add();
    info->_thread_id = id;
    return thread_lane(_lanes.size() - 1);
}

void* taskmaster::alloc_task_memory(uints size, uints& granules_out)
{
    const uints n = align_to_chunks(size, sizeof(granule));
    {
        comm_mutex_guard<comm_mutex> lock(_sync);
        range<granule> data = _taskdata.add_contiguous_range(n);
        if (!data.is_empty()) {
            granules_out = n;
            return data.ptr();
        }
    }

    // larger than a page of granules
    granules_out = 0;
    return ::operator new(size);
}

void taskmaster::free_task_memory(void* memory, uints granules)
{
    comm_mutex_guard<comm_mutex> lock(_sync);
    free_task_memory_locked(memory, granules);
}

void taskmaster::free_task_memory_locked(void* memory, uints granules)
{
    if (granules == 0) {
        ::operator delete(memory);
        return;
    }

    const uints id = _taskdata.get_item_id(static_cast<granule*>(memory));
    DASSERT_RET(_taskdata.is_valid_id(id));
    _taskdata.del_range(id, granules);
}

void taskmaster::enqueue(invoker_base* task)
{
    // counted from its push - its waiter can't see it done before it runs
    if (task->_wait_counter_ptr)
        task->_wait_counter_ptr->fetch_add(1, std::memory_order_relaxed);

    comm_mutex_guard<comm_mutex> lock(_sync);
    if (_quitting.load(std::memory_order_relaxed)) {
        coidlog_warning("task '" << task->_name << "' pushed after the termination - dropped");
        drop_locked(task, false);
        return;
    }

    if (task->_lane != invalid_lane) {
        if (task->_lane >= _lanes.size()) {
            coidlog_error("task '" << task->_name << "' pushed for an unknown lane " << task->_lane << " - dropped");
            drop_locked(task, false);
            return;
        }

        _lanes[task->_lane]._tasks.push_back(task);
    }
    else {
        _ready_jobs[int(task->_priority)].push_back(task);
        ++_active_count;
    }

    wake_locked(task);
}

taskmaster::invoker_base* taskmaster::pop_locked(thread_lane lane, bool is_background)
{
    if (lane != invalid_lane) {
        if (invoker_base* task = _lanes[lane]._tasks.pop_front()) {
            ++_active_count;
            return task;
        }
    }

    // a background worker's normal and background tasks, another thread's critical and normal ones
    const int first = is_background ? int(task_priority_enum::normal) : int(task_priority_enum::critical);
    const int last = is_background ? int(task_priority_enum::background) : int(task_priority_enum::normal);
    for (int prio = first; prio <= last; ++prio) {
        if (invoker_base* task = _ready_jobs[prio].pop_front())
            return task;
    }

    return nullptr;
}

void taskmaster::wake_locked(const invoker_base* task)
{
    if (task->_lane == invalid_lane) {
        // an idle worker that runs it: a critical task a frame worker's, a background one a background worker's, a normal one
        // either - the frame workers first
        threadinfo** idle = &_idle_background;
        if (task->_priority == task_priority_enum::critical || (task->_priority == task_priority_enum::normal && _idle_frame))
            idle = &_idle_frame;

        if (threadinfo* worker = *idle) {
            *idle = worker->next_idle;
            worker->next_idle = 0;
            worker->is_idle = false;
            worker->cv->notify_one();
            return;
        }
    }

    // a lane's task or no idle worker for it - the waiting threads (its thread, a thread that may run it)
    if (_idle_waiters)
        _wait_cv.notify_all();
}

void taskmaster::remove_idle_locked(threadinfo& ti)
{
    for (threadinfo** link = ti.is_background ? &_idle_background : &_idle_frame; *link; link = &(*link)->next_idle) {
        if (*link == &ti) {
            *link = ti.next_idle;
            break;
        }
    }

    ti.next_idle = 0;
    ti.is_idle = false;
}

bool taskmaster::release_task(invoker_base* task, uints& granules_out)
{
    wait_counter* const counter_ptr = task->_wait_counter_ptr;
    granules_out = task->_granules;
    task->~invoker_base();
    return counter_ptr && counter_ptr->fetch_sub(1, std::memory_order_release) == 1;
}

void taskmaster::execute_locked(invoker_base* task)
{
    _sync.unlock();

    {
        profiler::scope scope(task->_name);
        try {
            task->invoke();
        }
        catch (const exception& e) {
            coidlog_error("task '" << task->_name << "' threw: " << e.text());
        }
        catch (const std::exception& e) {
            coidlog_error("task '" << task->_name << "' threw: " << e.what());
        }
        catch (...) {
            coidlog_error("task '" << task->_name << "' threw an unknown exception");
        }
    }

    uints granules = 0;
    const bool is_done = release_task(task, granules);

    _sync.lock();
    free_task_memory_locked(task, granules);
    --_active_count;

    // its waiters - the counts changed before the lock, so a waiter saw them or sleeps now
    if (_idle_waiters && (is_done || _active_count == 0))
        _wait_cv.notify_all();
}

void taskmaster::drop_locked(invoker_base* task, bool is_counted)
{
    _sync.unlock();

    uints granules = 0;
    const bool is_done = release_task(task, granules);

    _sync.lock();
    free_task_memory_locked(task, granules);
    if (is_counted)
        --_active_count;

    if (_idle_waiters && (is_done || (is_counted && _active_count == 0)))
        _wait_cv.notify_all();
}

taskmaster::thread_lane taskmaster::find_lane_locked(thread_t id) const
{
    for (uints i = 0; i < _lanes.size(); ++i) {
        if (_lanes[i]._thread_id == id)
            return thread_lane(i);
    }

    return invalid_lane;
}

template <typename Done>
void taskmaster::help_until_locked(comm_mutex_guard<comm_mutex>& lock, const Done& is_done)
{
    const bool is_background = is_background_worker();
    const thread_lane lane = find_lane_locked(thread::self());
    while (!is_done()) {
        invoker_base* task = pop_locked(lane, is_background);
        if (task) {
            execute_locked(task);
            continue;
        }

        // asleep until a task it may run or its condition - both signaled under the lock, no wakeup lost
        ++_idle_waiters;
        _wait_cv.wait(lock);
        --_idle_waiters;
    }
}

void* taskmaster::threadfunc( int order )
{
    t_master = this;
    t_order = order;

    threadinfo& self = _threads[order];
    if (self.slot.is_set())
        thread::set_ideal_processor(self.slot._group, self.slot._number);

    char tmp[64];
    if (self.is_background) {
        thread::set_background_priority(true);
        sprintf_s(tmp, "background worker %d", order - int(_frame_workers_count));
    }
    else {
        sprintf_s(tmp, "frame worker %d", order);
    }

    coidlog_debug(tmp << " running");
    profiler::set_thread_name(tmp);

    // the debugger's name too - the thread manager's (SetThreadDescription)
    thread::set_name(tmp);

    threadinfo*& idle = self.is_background ? _idle_background : _idle_frame;

    comm_mutex_guard<comm_mutex> lock(_sync);
    while (!_quitting.load(std::memory_order_relaxed)) {
        invoker_base* task = pop_locked(invalid_lane, self.is_background);
        if (task) {
            execute_locked(task);
            continue;
        }

        // asleep in its idle list until a push takes it out or the quit - both under the lock, no wakeup lost
        self.is_idle = true;
        self.next_idle = idle;
        idle = &self;
        while (self.is_idle && !_quitting.load(std::memory_order_relaxed))
            self.cv->wait(lock);

        if (self.is_idle)
            remove_idle_locked(self);
    }

    coidlog_debug(tmp << " exiting");

    return 0;
}

void taskmaster::enter_critical_section(critical_section& critical_section, int spin_count)
{
    for (;;) {
        for (int i = 0; i < spin_count; ++i) {
            if (critical_section.value == 0 && atomic::cas(&critical_section.value, 1, 0) == 0) {
                return;
            }
        }

        // another task the thread may run meanwhile, none - a yield
        bool has_run = false;
        {
            comm_mutex_guard<comm_mutex> lock(_sync);
            invoker_base* task = pop_locked(find_lane_locked(thread::self()), is_background_worker());
            if (task) {
                execute_locked(task);
                has_run = true;
            }
        }

        if (!has_run)
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
    comm_mutex_guard<comm_mutex> lock(_sync);
    help_until_locked(lock, [&wait_counter]() {
        return wait_counter.load(std::memory_order_acquire) == 0;
        });
}

void taskmaster::terminate(bool empty_queue)
{
    {
        comm_mutex_guard<comm_mutex> lock(_sync);
        if (_terminated || _quitting.load(std::memory_order_relaxed))
            return;

        // the queued tasks done first - the calling thread helps (its lane's too)
        if (empty_queue) {
            const thread_lane lane = find_lane_locked(thread::self());
            help_until_locked(lock, [this, lane]() {
                return _active_count == 0 && (lane == invalid_lane || _lanes[lane]._tasks.is_empty());
                });
        }

        _quitting.store(true, std::memory_order_relaxed);
        _threads.for_each([](threadinfo& ti) {
            if (ti.is_idle)
                ti.cv->notify_one();
            });
    }

    //wait for cancellation
    _threads.for_each([](threadinfo& ti) {
        thread::join(ti.tid);
        });

    // the tasks that didn't run - destroyed, their counters signaled
    comm_mutex_guard<comm_mutex> lock(_sync);
    for (task_queue& queue : _ready_jobs) {
        while (invoker_base* task = queue.pop_front())
            drop_locked(task, true);
    }

    for (uints i = 0; i < _lanes.size(); ++i) {
        while (invoker_base* task = _lanes[i]._tasks.pop_front())
            drop_locked(task, false);
    }

    _terminated = true;
}

COID_NAMESPACE_END
