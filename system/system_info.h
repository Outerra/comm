#pragma once

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
 * Outerra s.r.o.
 * Portions created by the Initial Developer are Copyright (C) 2026
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

#include "system_info_types.h"

#include "../dynarray.h"
#include "../str.h"

namespace coid {

/// @brief The system's information - its operating system, CPU, memory and displays - gathered once per process and logged
/// once a log is open (log_info). get_instance gives the process's instance (a process-wide singleton - the executable's
/// and its modules' one), its first call gathers it: a failure recorded (get_errors), never thrown - a logger may not exist
/// yet. Thread safe: get_instance's concurrent first calls wait for the one gathering, the information doesn't change once
/// gathered. The platform's part reads the system (Windows: system_info_win.cpp), the pure functions of system_info_utils.h
/// decode, summarize, order and format its values
class system_info
{
public: // methods only
    /// @brief Gets the process's information - the first call gathers it (a few milliseconds), the calls meanwhile wait for
    /// it
    static const system_info& get_instance();

    /// @brief Gathers the system's information - get_instance's instance; another one gathers it again
    system_info();
    ~system_info();

    /// @brief Gets the active displays: the primary display first (at the desktop's origin), the others from left to right,
    /// then from top to bottom (get_display_order)
    const coid::dynarray32<const display_info>& get_display_info() const;

    /// @brief Gets the CPU: its identification and features by CPUID, its cores and caches by the system
    const cpu_info& get_cpu_info() const;

    /// @brief Gets the operating system's name and version
    const os_info& get_os_info() const;

    /// @brief Gets the physical memory the system uses
    /// @return bytes; 0 when it can't be read
    uint64 get_physical_memory_size() const;

    /// @brief Gets the failures of the gathering - log_info logs them (a logger may not exist at the gathering)
    const coid::dynarray32<coid::charstr>& get_errors() const;

    /// @brief Logs the information at info level - a line each of the operating system, the CPU, the memory and every
    /// display (format_system_info) - and the gathering's failures as errors
    void log_info() const;

protected: // methods only
    system_info(const system_info&) = delete;
    system_info& operator=(const system_info&) = delete;

    // the platform's part - a failure added to _errors
    void load_os_info_internal();
    void load_cpu_info_internal();
    void load_memory_info_internal();
    void load_display_info_internal();

protected: // members only
    coid::dynarray32<const display_info> _display_info;
    cpu_info _cpu_info;
    os_info _os_info;
    uint64 _physical_memory_size = 0;
    coid::dynarray32<coid::charstr> _errors;
};

} // end of namespace coid
