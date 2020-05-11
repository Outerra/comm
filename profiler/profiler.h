#pragma once

#include "../commtypes.h"
#include "../sync/mutex.h"


namespace profiler
{

struct gpu_event {
    char name[32];
    uint64 timestamp;
    bool is_end;
};

enum class record_type : uint8 {
    BEGIN,
    END,
    STRING,
    LINK,
    FRAME
};

struct record_header {
    uint8 size;
    record_type type;
    uint64 time;
};

struct begin_record {
    const char* name;
    uint color_rgba;
};

struct thread_context final {
    thread_context() : mutex(400, false) {}

    static constexpr size_t SIZE = 2*1024*1024;
    uint8 buffer[SIZE];
    size_t begin = 0;
    size_t end = 0;
    coid::comm_mutex mutex;
    char name[64];
};


void frame();
uint64 get_token(const char* name, uint8 r = 0xa0, uint8 g = 0xa0, uint8 b = 0xa0);
void begin(uint64 token);
void begin(const char* name, uint8 r = 0xa0, uint8 g = 0xa0, uint8 b = 0xa0);
void push_string(const char* string);
void push_number(const char* label, uint value);
void push_link(uint64 link);
void end();

void begin_gpu(const coid::token& name, uint64 timestamp);
void end_gpu(const coid::token& name, uint64 timestamp);

// returns unique value in 0-0xffFFffFF range
// use this for shortlived stuff,
uint64 create_transient_link();
uint64 create_fixed_link(); // returns unique value in 0-0xffFFffFF range + 33rd bit set

uint64 now();
void pause(bool pause);
bool is_paused();
void setThreadName(const char* name);

uint lock_contexts();
thread_context* lock_context(uint idx);
void unlock_context(uint idx);
void unlock_contexts();
uint get_gpu_events_count();
gpu_event& get_gpu_event(uint idx);

struct scope final
{
    scope(const char* name, uint8 r, uint8 g, uint8 b) { begin(name, r, g, b); }
    scope(const char* name) { begin(name); }
    scope(uint64 token) { begin(token); }
    ~scope() { end(); }
};
}

#define CPU_PROFILE_CONCAT2(a, b) a ## b
#define CPU_PROFILE_CONCAT(a, b) CPU_PROFILE_CONCAT2(a, b)

#ifdef _TITAN
    #define USE_MICROPROFILE
#endif

#ifdef USE_MICROPROFILE
#define CPU_PROFILE_BEGIN(name) static uint64 CPU_PROFILE_CONCAT(g_mp, __LINE__) = profiler::get_token(name); profiler::begin(CPU_PROFILE_CONCAT(g_mp, __LINE__));
#define CPU_PROFILE_END() profiler::end();
#define CPU_PROFILE_SCOPE(name) static uint64 CPU_PROFILE_CONCAT(g_mp, __LINE__) = profiler::get_token(name); profiler::scope CPU_PROFILE_CONCAT(foo, __LINE__)(CPU_PROFILE_CONCAT(g_mp, __LINE__));
#define CPU_PROFILE_SCOPE_COLOR(name, r, g, b) static uint64 CPU_PROFILE_CONCAT(g_mp, __LINE__) = profiler::get_token(name, r, g, b); profiler::scope CPU_PROFILE_CONCAT(foo, __LINE__)(CPU_PROFILE_CONCAT(g_mp, __LINE__));
#define CPU_PROFILE_FUNCTION() static uint64 CPU_PROFILE_CONCAT(g_mp, __LINE__) = profiler::get_token(__FUNCTION__); profiler::scope CPU_PROFILE_CONCAT(foo, __LINE__)(CPU_PROFILE_CONCAT(g_mp, __LINE__));
#else
#define CPU_PROFILE_BEGIN(name) profiler::begin(name);
#define CPU_PROFILE_END() profiler::end();
#define CPU_PROFILE_SCOPE(name) profiler::scope CPU_PROFILE_CONCAT(profile_scope, __LINE__)(name);
#define CPU_PROFILE_SCOPE_COLOR(name, r, g, b) profiler::scope CPU_PROFILE_CONCAT(profile_scope, __LINE__)(name, r, g, b);
#define CPU_PROFILE_FUNCTION() profiler::scope profile_scope(__FUNCTION__);
#endif


