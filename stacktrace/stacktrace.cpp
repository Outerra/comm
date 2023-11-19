#include "stacktrace.h"
#include "../atomic/pool.h"

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::stacktrace::stacktrace(stacktrace&& rhs) noexcept
{
    _first_frame = rhs._first_frame;
    rhs._first_frame = nullptr;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::stacktrace& coid::stacktrace::operator=(stacktrace&& rhs) noexcept
{
    release_frames_internal();
    _first_frame = rhs._first_frame;
    rhs._first_frame = nullptr;

    return *this;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::stacktrace::~stacktrace()
{
    release_frames_internal();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void coid::stacktrace::release_frames_internal()
{
    stack_frame* current_frame = _first_frame;
    while (current_frame != nullptr)
    {
        stack_frame* next_frame = current_frame->_next_frame;
        pool<stack_frame>::global().release_item(current_frame);
        current_frame = next_frame;
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

const coid::stack_frame* coid::stacktrace::get_first_stack_frame() const
{
    return _first_frame;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

const void* coid::stack_frame::get_frame_address() const
{
    return _frame_address;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

const coid::stack_frame* coid::stack_frame::get_next_frame() const
{
    return _next_frame;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void coid::stack_frame::reset()
{
    _frame_address = nullptr;
    _next_frame = nullptr;
}
