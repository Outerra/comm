#include "profiler.h"

#ifndef USE_MICROPROFILE
#include "../atomic/atomic.h"
#include "../dynarray.h"
#include "../sync/mutex.h"
#include "../timer.h"
struct IUnknown;
#define NOMINMAX 
#include <Windows.h>

namespace profiler
{

template <typename T, uint COUNT>
struct ring_buffer {
    alignas(T) uint8 mem[sizeof(T) * COUNT];
    uint beg = 0;
    uint end = 0;

    T* push() {
        if (end - beg == COUNT) {
            ++beg;
        }
        ++end;
        return &((T*)mem)[(end - 1) % COUNT];
    }

    uint size() const { return end - beg; }
    T& operator[](uint idx) { return ((T*)mem)[(beg + idx) % COUNT]; }
};

struct profiler {

    profiler() : mutex(400, false) {}
    coid::comm_mutex mutex;
    coid::dynarray<thread_context*> contexts;
    ring_buffer<gpu_event, 8*1024> gpu_events;
    bool paused = true;
};

auto& g_profiler = PROCWIDE_SINGLETON(profiler);

thread_context* get_thread_context() {
    thread_local thread_context* ctx = [](){
        thread_context* ctx = new thread_context;
        GUARDTHIS(g_profiler.mutex);
        sprintf_s(ctx->name, "thread %X", GetCurrentThreadId());
        g_profiler.contexts.push(ctx);
        return ctx;
    }();

    return ctx;
}


uint64 now() {
    return coid::nsec_timer::current_time_ns();
}


bool is_paused() { return g_profiler.paused; }
void pause(bool pause) { g_profiler.paused = pause; }


void setThreadName(const char* name) {
    thread_context* ctx = get_thread_context();
    GUARDTHIS(ctx->mutex);
    strcpy_s(ctx->name, name);
}


void write(thread_context* ctx, const uint8* value, size_t vsize) {
    while (ctx->end - ctx->begin + vsize > thread_context::SIZE) {
        const uint8 size = *(uint8*)&ctx->buffer[ctx->begin % thread_context::SIZE];
        ctx->begin += sizeof(record_header) + size;
    }

    const size_t e = ctx->end % thread_context::SIZE;
    if (e + vsize > thread_context::SIZE) {
        const size_t s = thread_context::SIZE - e;
        memcpy(&ctx->buffer[e], value, s);
        memcpy(&ctx->buffer[0], value + s, vsize - s);
    }
    else {
        memcpy(&ctx->buffer[e], value, vsize);
    }
    ctx->end += vsize;
}

template <typename T>
void write(thread_context* ctx, const T& val) {
    while (ctx->end - ctx->begin + sizeof(val) > thread_context::SIZE) {
        const uint8 size = *(uint8*)&ctx->buffer[ctx->begin % thread_context::SIZE];
        ctx->begin += sizeof(record_header) + size;
    }

    const size_t e = ctx->end % thread_context::SIZE;
    if (e + sizeof(val) > thread_context::SIZE) {
        const size_t s = thread_context::SIZE - e;
        memcpy(&ctx->buffer[e], &val, s);
        memcpy(&ctx->buffer[0], (uint8*)&val + s, sizeof(val) - s);
    }
    else {
        memcpy(&ctx->buffer[e], &val, sizeof(val));
    }
    ctx->end += sizeof(val);
}


void write(thread_context* ctx, record_type type, uint64 time) {
    if (g_profiler.paused) return;
    record_header header;
    header.time = time;
    header.type = type;
    header.size = 0;
    GUARDTHIS(ctx->mutex);
    write(ctx, header);
}


template <typename T>
void write(thread_context* ctx, record_type type, uint64 time, const T& val)
{
    if (g_profiler.paused) return;
    record_header header;
    header.time = time;
    header.type = type;
    header.size = sizeof(val);
    GUARDTHIS(ctx->mutex);
    write(ctx, header);
    write(ctx, val);
}

uint64 create_transient_link() {
    static volatile uint32 link;
    return atomic::inc(&link);
}

uint64 create_fixed_link() {
    static volatile uint32 link;
    return atomic::inc(&link) | ((uint64)1 << 32);
}

void push_link(uint64 link) {
    thread_context* ctx = get_thread_context();
    write(ctx, record_type::LINK, now(), link);
}

void push_number(const char* label, uint value) {
    char tmp[256];
    sprintf_s(tmp, "%s: %d", label, value);
    push_string(tmp);
}

void push_string(const char* string) {
    thread_context* ctx = get_thread_context();

    if (g_profiler.paused) return;
    record_header header;
    header.time = now();
    header.type = record_type::STRING;
    size_t len = strlen(string);
    if (len > 0xfe) len = 0xfe;
    header.size = uint8(len) + 1;
    GUARDTHIS(ctx->mutex);
    write(ctx, header);
    write(ctx, (const uint8*)string, header.size - 1);
    write(ctx, (char)0);
}

void frame() {
    thread_context* ctx = get_thread_context();
    static uint64 time = now();
    const uint64 n = now();
    const uint64 delta = now() - time;
    time = n;
    write(ctx, record_type::FRAME, now(), delta);
}

void begin_gpu(const coid::token& name, uint64 timestamp)
{
    if (g_profiler.paused) return;
    gpu_event* e = g_profiler.gpu_events.push();
    e->timestamp = timestamp;
    name.copy_to(e->name, sizeof(e->name));
    e->is_end = false;
}

uint get_gpu_events_count() { return (uint)g_profiler.gpu_events.size(); }

gpu_event& get_gpu_event(uint idx) { return g_profiler.gpu_events[idx]; }

void end_gpu(const coid::token& name, uint64 timestamp)
{
    if (g_profiler.paused) return;
    gpu_event* e = g_profiler.gpu_events.push();
    e->timestamp = timestamp;
    name.copy_to(e->name, sizeof(e->name));
    e->is_end = true;
}
uint64 get_token(const char* name, uint8 r, uint8 g, uint8 b) {
    return 0;
}
void begin(uint64 token) {
}
void begin(const char* name, uint8 r, uint8 g, uint8 b) {
    thread_context* ctx = get_thread_context();
    begin_record val;
    val.name = name;
    val.color_rgba = ((uint)r << 24) | ((uint)g << 16) | ((uint)b << 8) | 0xff;
    write(ctx, record_type::BEGIN, now(), val);
}

void end() {
    thread_context* ctx = get_thread_context();
    write(ctx, record_type::END, now());
}


uint lock_contexts() {
    g_profiler.mutex.lock();
    return (uint)g_profiler.contexts.size();
}


void unlock_contexts() {
    g_profiler.mutex.unlock();
}


thread_context* lock_context(uint idx) {
    g_profiler.contexts[idx]->mutex.lock();
    return g_profiler.contexts[idx];
}


void unlock_context(uint idx) {
    g_profiler.contexts[idx]->mutex.unlock();
}

} // namespace profiler
#endif