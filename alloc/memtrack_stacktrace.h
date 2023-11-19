#pragma once
#include "../token.h"
#include "../stacktrace/stacktrace.h"
namespace coid
{
/// @brief Stack trace of single memory operation alloc/free
struct memtrack_stacktrace
{
    coid::token _name = 0;               //< class identifier
    uints _size = 0;                    //< size of memory operation        
    stacktrace _stack_trace;             //< stack trace of memory operation call

    memtrack_stacktrace() = default;
};

void enable_record_memtrack_stacktrace(bool enable);
uint memtrack_stacktrace_list(memtrack_stacktrace* destionaion, uint max_count);
uint memtrack_stacktrace_count();


}; // end of namespace coid