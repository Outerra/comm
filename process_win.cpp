#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "process.h"
#include "net_ul.h"
#include "commexception.h"

constexpr uint wait_ms = 10;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::process::process(
    void* std_out_handle,
    void* std_err_handle,
    void* std_in_write_handle,
    void* std_in_read_handle,
    void* process_handle,
    void* process_therad_handle,
    unsigned long process_id,
    unsigned long process_thead_id)
    : _std_out_handle(std_out_handle)
    , _std_err_handle(std_err_handle)
    , _std_in_write_handle(std_in_write_handle)
    , _std_in_read_handle(std_in_read_handle)
    , _process_handle(process_handle)
    , _process_therad_handle(process_therad_handle)
    , _process_id(process_id)
    , _process_thead_id(process_thead_id)
    
    , _std_out_buf_mutex(512, false)
    , _std_err_buf_mutex(512, false)

{}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//void * coid::process::process_std_out_thread_func(void* context)
//{
//    bool success = true;
//    process* proc_ptr = static_cast<process*>(context);
//    DASSERT(proc_ptr != nullptr);
//
//    char buf[pipe_buffer_size] = { 0, };
//    DWORD bytes_read = 0;
//
//    while (!coid::thread::self_should_cancel() && success)
//    {
//
//        success = PeekNamedPipe(proc_ptr->_std_out_handle, buf, pipe_buffer_size, &bytes_read, NULL, NULL);
//
//        bytes_read -= proc_ptr->_std_out_peek_pos;
//
//        if (success && bytes_read > 0)
//        {
//            GUARDTHIS(proc_ptr->_std_out_buf_mutex);
//            uints size = bytes_read;
//            if (proc_ptr->_std_out_buf.write_raw(buf + proc_ptr->_std_out_peek_pos, size) != NOERR)
//            {
//                DASSERT(0);
//            }
//
//            proc_ptr->_std_out_peek_pos += bytes_read;
//        }        
//        
//        if (proc_ptr->_std_out_peek_pos == pipe_buffer_size)
//        {
//            success = ReadFile(proc_ptr->_std_out_handle, buf, pipe_buffer_size, &bytes_read, NULL);
//            proc_ptr->_std_out_peek_pos = 0;
//        }
//
//        coid::sysMilliSecondSleep(wait_ms);
//    }
//
//    return nullptr;
//}

void* coid::process::process_std_out_thread_func(void* context)
{
    bool success = true;
    process* proc_ptr = static_cast<process*>(context);
    DASSERT(proc_ptr != nullptr);

    char buf[pipe_buffer_size] = { 0, };
    DWORD bytes_read = 0;

    while (!coid::thread::self_should_cancel() && success)
    {
        success = ReadFile(proc_ptr->_std_out_handle, buf, pipe_buffer_size, &bytes_read, NULL);

        if (success && bytes_read > 0)
        {
            GUARDTHIS(proc_ptr->_std_out_buf_mutex);
            uints size = bytes_read;
            if (proc_ptr->_std_out_buf.write_raw(buf, size) != NOERR)
            {
                DASSERT(0);
            }
        }

        coid::sysMilliSecondSleep(wait_ms);
    }

    return nullptr;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void* coid::process::process_std_err_thread_func(void* context)
{
    bool success = true;
    process* proc_ptr = static_cast<process*>(context);
    DASSERT(proc_ptr != nullptr);

    char buf[pipe_buffer_size] = { 0, };
    DWORD bytes_read = 0;

    while (!coid::thread::self_should_cancel() && success)
    {
        success = ReadFile(proc_ptr->_std_err_handle, buf, pipe_buffer_size, &bytes_read, NULL);

        if (success && bytes_read > 0)
        {
            GUARDTHIS(proc_ptr->_std_err_buf_mutex);
            uints size = bytes_read;
            if (proc_ptr->_std_err_buf.write_raw(buf, size) != NOERR)
            {
                DASSERT(0);
            };
        }

        coid::sysMilliSecondSleep(wait_ms);
    }

    return nullptr;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::process::~process()
{
    if (_std_out_handle != NULL)
    {
        CloseHandle(_std_out_handle);
    }

    if (_std_err_handle != NULL)
    {
        CloseHandle(_std_err_handle);
    }

    if (_std_in_write_handle != NULL)
    {
        CloseHandle(_std_in_write_handle);
    }

    if (_std_in_read_handle != NULL)
    {
        CloseHandle(_std_in_read_handle);
    }

    if (_process_handle != NULL)
    {
        CloseHandle(_process_handle);
    }

    if (_process_therad_handle != NULL)
    {
        CloseHandle(_process_therad_handle);
    }

    if (_std_out_thread.is_invalid())
    {
        _std_out_thread.cancel();
    }

    if (_std_err_thread.is_invalid())
    {
        _std_err_thread.cancel();
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bool coid::process::is_running() const
{
    if (_process_handle != NULL)
    {
        DWORD result = WaitForSingleObject(_process_handle, 0);
        return result == WAIT_TIMEOUT;
    }

    return false;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bool coid::process::get_return_code(int& return_code_out)
{
    DWORD exit_code;
    if (GetExitCodeProcess(_process_handle, &exit_code))
    {
        if (exit_code != STILL_ACTIVE)
        {
            return_code_out = exit_code;
            return true;
        }
    }
    return false;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void coid::process::terminate()
{
    if (_process_handle)
    {
        TerminateProcess(_process_handle, -1);
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bool coid::process::is_awaiting_input()
{
    DWORD res = WaitForSingleObject(_std_in_read_handle, 0);
    return res == WAIT_TIMEOUT;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

uint coid::process::read_std_out(coid::charstr& message_out)
{
    message_out.reset();

    if (!_std_out_buf.is_empty())
    {
        GUARDTHIS(_std_out_buf_mutex);
        coid::token buf_tok(_std_out_buf);
        message_out = buf_tok;
        _std_out_buf.reset_all();
        return message_out.len();
    }

    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bool coid::process::is_std_out_empty() const
{
    GUARDTHIS(_std_out_buf_mutex);
    return _std_out_buf.is_empty();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

uint coid::process::read_std_err(coid::charstr& message_out)
{
    message_out.reset();

    if (!_std_err_buf.is_empty())
    {
        GUARDTHIS(_std_err_buf_mutex);
        coid::token buf_tok(_std_err_buf);
        message_out = buf_tok;
        _std_err_buf.reset_all();
        return message_out.len();
    }

    return 0;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

bool coid::process::is_std_err_empty() const
{
    GUARDTHIS(_std_err_buf_mutex);
    return _std_err_buf.is_empty();
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


uint coid::process::write_std_in(const coid::token& message_in)
{
    if (message_in.is_empty())
    {
        return 0;
    }
    
    uint result = 0;
    
    const char* message_start = message_in.ptr();
    const char* message_end = message_in.ptre();

    while (message_start < message_end)
    {
        DWORD len = static_cast<DWORD>(message_end - message_start);
        DWORD to_write = len < pipe_buffer_size ? len : pipe_buffer_size;
        DWORD written = 0;
        if (WriteFile(_std_in_write_handle, message_start, to_write, &written, NULL))
        {
            message_start += written;
            result += written;
        }
    }

    return result;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

int coid::process::get_process_thread_priority() const
{
    if (is_running())
    {
        return GetThreadPriority(_process_therad_handle);
    }

    return THREAD_PRIORITY_ERROR_RETURN;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

ref<coid::process> coid::process::create_process(const coid::charstr& process_executable_path, const coid::charstr& params, bool show_window, bool redirect_std_streams)
{
    HANDLE std_out_write_handle = NULL;
    HANDLE std_out_read_handle = NULL;
    HANDLE std_err_write_handle = NULL;
    HANDLE std_err_read_handle = NULL;
    HANDLE std_in_write_handle = NULL;
    HANDLE std_in_read_handle = NULL;
    PROCESS_INFORMATION proc_info;

    try
    {
        char command_line_buf[MAX_PATH] = { 0, };
        uints len = process_executable_path.lens();
        process_executable_path.copy_to(command_line_buf, MAX_PATH);
        command_line_buf[len++] = ' ';
        params.copy_to(&command_line_buf[len], MAX_PATH - len);

        if (redirect_std_streams)
        {
            SECURITY_ATTRIBUTES pipe_security_atttibs;
            ZeroMemory(&pipe_security_atttibs, sizeof(pipe_security_atttibs));
            pipe_security_atttibs.nLength = sizeof(SECURITY_ATTRIBUTES);
            pipe_security_atttibs.bInheritHandle = TRUE;
            pipe_security_atttibs.lpSecurityDescriptor = NULL;

            if (!CreatePipe(&std_out_read_handle, &std_out_write_handle, &pipe_security_atttibs, pipe_buffer_size))
            {
                throw coid::exception() << "Can't create pipe!" << "(Error: " << GetLastError() << ")";
            }

            if (!CreatePipe(&std_err_read_handle, &std_err_write_handle, &pipe_security_atttibs, pipe_buffer_size))
            {
                throw coid::exception() << "Can't create pipe!" << "(Error: " << GetLastError() << ")";
            }

            if (!CreatePipe(&std_in_read_handle, &std_in_write_handle, &pipe_security_atttibs, pipe_buffer_size))
            {
                throw coid::exception() << "Can't create pipe!" << "(Error: " << GetLastError() << ")";
            }
        }

        STARTUPINFO proc_startup_info;
        SECURITY_ATTRIBUTES proc_security_atttibs;

        ZeroMemory(&proc_startup_info, sizeof(proc_startup_info));
        proc_startup_info.cb = sizeof(proc_startup_info);

        if (redirect_std_streams)
        {
            proc_startup_info.hStdOutput = std_out_write_handle;
            proc_startup_info.hStdError = std_err_write_handle;
            proc_startup_info.hStdInput = std_in_read_handle;
            proc_startup_info.dwFlags |= STARTF_USESTDHANDLES;
        }

        if (!show_window)
        {
            proc_startup_info.dwFlags |= STARTF_USESHOWWINDOW;
            proc_startup_info.wShowWindow = SW_HIDE;
        }
        else 
        {
            proc_startup_info.dwFlags |= STARTF_USESHOWWINDOW;
            proc_startup_info.wShowWindow = SW_SHOWNOACTIVATE;
        }


        ZeroMemory(&proc_info, sizeof(proc_info));

        ZeroMemory(&proc_security_atttibs, sizeof(proc_security_atttibs));
        proc_security_atttibs.nLength = sizeof(SECURITY_ATTRIBUTES);
        proc_security_atttibs.bInheritHandle = redirect_std_streams ? TRUE : FALSE;
        proc_security_atttibs.lpSecurityDescriptor = NULL;

        if (!CreateProcess(
            process_executable_path.c_str(), 
            command_line_buf, 
            &proc_security_atttibs, 
            NULL, 
            proc_security_atttibs.bInheritHandle, 
            show_window ? CREATE_NEW_CONSOLE : DETACHED_PROCESS,
            NULL, 
            NULL, 
            &proc_startup_info, 
            &proc_info))
        {

            throw coid::exception() << "Can't create process \"" << process_executable_path << " \"!" << "(Error: " << GetLastError() << ")";
        }
    }
    catch (const coid::exception& e)
    {
        if (std_out_write_handle) CloseHandle(std_out_write_handle);
        if (std_out_read_handle) CloseHandle(std_out_read_handle);
        if (std_err_write_handle)CloseHandle(std_err_write_handle);
        if (std_err_read_handle) CloseHandle(std_err_read_handle);
        if (std_in_write_handle)CloseHandle(std_in_write_handle);
        if (std_in_read_handle)CloseHandle(std_in_read_handle);
        if (proc_info.hThread)CloseHandle(proc_info.hThread);
        if (proc_info.hProcess)CloseHandle(proc_info.hProcess);
        throw e;
    }

    ref<process> result(new coid::process(std_out_read_handle, std_err_read_handle, std_in_write_handle, std_in_read_handle, proc_info.hProcess, proc_info.hThread, proc_info.dwProcessId, proc_info.dwThreadId));

    if (redirect_std_streams)
    {
        CloseHandle(std_out_write_handle);
        CloseHandle(std_err_write_handle);
    
        coid::charstr thread_name = process_executable_path;
        thread_name << ": std out";
        result->_std_out_thread.create(&coid::process::process_std_out_thread_func, result.get(), nullptr, thread_name);

        thread_name = process_executable_path;
        thread_name << ": std err";
        result->_std_err_thread.create(&coid::process::process_std_err_thread_func, result.get(), nullptr, thread_name);
    }

    return result;
}