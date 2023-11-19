#pragma once

namespace coid
{
class stack_frame
{
    friend class stacktrace;
public: // methods only
    const void* get_frame_address() const;
    const stack_frame* get_next_frame() const;
    stack_frame() = default;
protected: // methods only
    void reset();
    stack_frame(const stack_frame&) = delete;
    stack_frame& operator=(const stack_frame&) = delete;
protected: // members only
    void* _frame_address = nullptr;
    stack_frame* _next_frame = nullptr;

};


/// @brief Class representing the stack trace
/// @note it is non-copyable, only movables
class stacktrace
{
public: // methods only
    stacktrace(stacktrace&& rhs) noexcept;
    stacktrace& operator=(stacktrace&& rhs) noexcept;
    ~stacktrace();
    stacktrace() = default;

    static stacktrace get_current_stack_trace();

    const stack_frame* get_first_stack_frame() const;
protected: // methods only
    stacktrace(const stacktrace&) = delete;
    stacktrace& operator=(const stacktrace&) = delete;
    void release_frames_internal();

protected: // members only
    stack_frame* _first_frame = nullptr;
};


}; // end of namespace coid