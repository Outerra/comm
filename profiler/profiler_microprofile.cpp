#include "profiler.h"

#ifdef USE_MICROPROFILE
#include "microprofile/microprofile.h"
#include "../atomic//atomic.h"
#include "../sync/mutex.h"
#include "../timer.h"
struct IUnknown;
#include <Windows.h>

namespace profiler
{

static bool paused = false;
struct microprofile_initializer
{
    microprofile_initializer()
    {
        MicroProfileOnThreadCreate("Main");
        MicroProfileSetEnableAllGroups(true);
        MicroProfileSetForceMetaCounters(true);
    }
} _microprofile_initializer;

uint64 now() {
    return coid::nsec_timer::current_time_ns();
}

bool is_paused() { return paused; }
void pause(bool pause) { paused = pause; }


void setThreadName(const char* name) {
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
}

void push_number(const char* label, uint value) {
}

void push_string(const char* string) {
}

void frame() {
    MicroProfileFlip(0);
}

void begin_gpu(const coid::token& name, uint64 timestamp)
{
    if (paused) return;
    begin(name.ptr());
}

uint get_gpu_events_count() { return 0; }

gpu_event& get_gpu_event(uint idx) {
    static gpu_event null_event;
    return null_event;
}

void end_gpu(const coid::token& name, uint64 timestamp)
{
    if (paused) return;
    end();
}
uint64 get_token(const char* name, uint8 r, uint8 g, uint8 b) {
    return MicroProfileGetToken("outerra", name, MP_AUTO, MicroProfileTokenTypeCpu);
}
void begin(uint64 token) {
    if (paused) return;

    MicroProfileEnter(token);
}
void begin(const char* name, uint8 r, uint8 g, uint8 b) {
    if (paused) return;

    static std::unordered_map<const char*, MicroProfileToken> s_tokens;
    auto pos = s_tokens.find(name);
    if (pos == s_tokens.end())
    {
        const char* shortName = strrchr(name, ':');
        pos = s_tokens.insert(std::make_pair(name, MicroProfileGetToken("outerra", shortName ? shortName + 1 : name, MP_AUTO, MicroProfileTokenTypeCpu))).first;
    }
    MicroProfileEnter(pos->second);
}

void end() {
    if (paused) return;

    MicroProfileLeave();
}


uint lock_contexts() {
    return 0;
}


void unlock_contexts() {
}


thread_context* lock_context(uint idx) {
    return nullptr;
}


void unlock_context(uint idx) {
}

} // namespace profiler
#endif