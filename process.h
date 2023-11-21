/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is COID/comm module.
 *
 * The Initial Developer of the Original Code is
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2023 
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Cyril Gramblicka
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#pragma once

#include "ref_s.h"
#include "sync/mutex.h"
#include "binstream/binstreambuf.h"

COID_NAMESPACE_BEGIN

class process
{
public: // methods only
    process(const process&) = delete;
    process& operator=(const process&) = delete;
    ~process();

    static ref<process> create_process(const coid::charstr& process_executable_path, const coid::charstr& params, bool show_window, bool redirect_std_streams);

    bool is_running() const;
    bool get_return_code(int& return_code_out);
    void terminate();
    bool is_awaiting_input();

    uint read_std_out(coid::charstr& message_out);
    bool is_std_out_empty() const;

    uint read_std_err(coid::charstr& message_out);
    bool is_std_err_empty() const;

    uint write_std_in(const coid::token& message_in);

    int get_process_thread_priority() const;

protected: // methods only
#if defined(SYSTYPE_MSVC)
    process(
        void* std_out_handle,
        void* std_err_handle,
        void* std_in_write_handle,
        void* std_in_read_handle,
        void* process_handle,
        void* process_therad_handle,
        unsigned long process_id,
        unsigned long process_thead_id
    );
#endif

    static void* process_std_out_thread_func(void* context);
    static void* process_std_err_thread_func(void* context);

protected: // members only
    inline static const int pipe_buffer_size = 4096;

#if defined(SYSTYPE_MSVC)
    //unsigned long _std_out_peek_pos = 0;

    void* _std_out_handle = nullptr;
    void* _std_err_handle = nullptr;
    void* _std_in_write_handle = nullptr;
    void* _std_in_read_handle = nullptr;

    void* _process_handle;
    void* _process_therad_handle;
    unsigned long _process_id;
    unsigned long _process_thead_id;
#endif

    coid::thread _std_out_thread;
    coid::thread _std_err_thread;

    coid::comm_mutex _std_out_buf_mutex;
    coid::comm_mutex _std_err_buf_mutex;

    coid::binstreambuf _std_out_buf;
    coid::binstreambuf _std_err_buf;
};

COID_NAMESPACE_END
