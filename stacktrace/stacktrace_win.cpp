#include "stacktrace.h"
#include "../atomic/pool.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>


coid::stacktrace coid::stacktrace::get_current_stack_trace()
{
    constexpr DWORD max_frames_to_capture = 128;
    void* backtrace[max_frames_to_capture];

    WORD frame_count = RtlCaptureStackBackTrace(0, max_frames_to_capture, backtrace, NULL);
    stacktrace result;

    stack_frame* previous_frame = nullptr;
    for (WORD i = 0; i < frame_count; i++)
    {
        stack_frame* new_frame = pool<stack_frame>::global().create_item();
        new_frame->reset();

        new_frame->_frame_address = backtrace[i];

        if (i == 0) 
        {
            result._first_frame = new_frame;
        }
        else 
        {
            previous_frame->_next_frame = new_frame;
        }
    
        previous_frame = new_frame;
    }

    return result;
}