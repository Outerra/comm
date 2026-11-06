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

#include "system_info.h"
#include "system_info_types.h"
#include "system_info_utils.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ShellScalingApi.h>

#include <immintrin.h>
#include <intrin.h>

#include <utility>

// GetDpiForMonitor
#pragma comment(lib, "shcore.lib")

/// @file
/// @brief The Windows part of system_info: the CPU (CPUID, GetLogicalProcessorInformationEx), Windows' version
/// (RtlGetVersion, the registry), the memory (GlobalMemoryStatusEx), the displays (QueryDisplayConfig, EnumDisplaySettingsW,
/// GetDpiForMonitor) - a failure added to the errors, never thrown (a logger may not exist yet)

namespace coid {

namespace
{
    /// @brief Windows' version key in the registry
    const wchar_t* const windows_version_key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

    /// @brief Appends a terminated UTF-16 text as UTF-8 (comm's append_wchar_buf takes UCS-2)
    void append_utf8_text(const wchar_t* text_ptr, coid::charstr& text_out)
    {
        const int wide_length = static_cast<int>(wcslen(text_ptr));
        const int length = wide_length > 0 ? WideCharToMultiByte(CP_UTF8, 0, text_ptr, wide_length, nullptr, 0, nullptr, nullptr) : 0;
        if (length > 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, text_ptr, wide_length, text_out.get_buf(static_cast<uints>(length)), length, nullptr, nullptr);
        }
    }

    /// @brief Reads a text of Windows' version key in the registry
    /// @return false when the value isn't there
    bool read_version_text(const wchar_t* value_name, coid::charstr& text_out)
    {
        wchar_t text[256];
        DWORD size = sizeof(text);
        if (RegGetValueW(HKEY_LOCAL_MACHINE, windows_version_key, value_name, RRF_RT_REG_SZ, nullptr, text, &size) != ERROR_SUCCESS)
        {
            return false;
        }

        append_utf8_text(text, text_out);
        return true;
    }

    /// @brief Reads a CPUID leaf
    cpuid_registers read_cpuid(uint32 leaf, uint32 subleaf)
    {
        int registers[4] = {};
        __cpuidex(registers, static_cast<int>(leaf), static_cast<int>(subleaf));

        cpuid_registers result;
        result._eax = static_cast<uint32>(registers[0]);
        result._ebx = static_cast<uint32>(registers[1]);
        result._ecx = static_cast<uint32>(registers[2]);
        result._edx = static_cast<uint32>(registers[3]);
        return result;
    }

    /// @brief Gets the logical processors of a group affinity
    cpu_affinity get_affinity(const GROUP_AFFINITY& group_affinity)
    {
        cpu_affinity affinity;
        affinity._group = group_affinity.Group;
        affinity._mask = group_affinity.Mask;
        return affinity;
    }

    /// @brief Gets the type of a cache's system type
    cpu_cache_type_enum get_cache_type(PROCESSOR_CACHE_TYPE type)
    {
        switch (type)
        {
            case CacheUnified: return cpu_cache_type_enum::unified;
            case CacheInstruction: return cpu_cache_type_enum::instruction;
            case CacheData: return cpu_cache_type_enum::data;
            case CacheTrace: return cpu_cache_type_enum::trace;
            default: return cpu_cache_type_enum::unknown;
        }
    }

    /// @brief A monitor searched by its device's name (EnumDisplayMonitors)
    struct monitor_search
    {
        const wchar_t* _device_name = nullptr;
        HMONITOR _monitor = nullptr;
    };

    BOOL CALLBACK find_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        monitor_search& search = *reinterpret_cast<monitor_search*>(data);
        MONITORINFOEXW info = {};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info) && wcscmp(info.szDevice, search._device_name) == 0)
        {
            search._monitor = monitor;
            return FALSE;
        }

        return TRUE;
    }

    /// @brief Gets a display's DPI scale - its monitor's effective DPI divided by 96 (each monitor's own when the process is
    /// DPI aware per monitor, otherwise the system's)
    /// @param device_name - the display's GDI name ("\\.\DISPLAY2")
    /// @return 0 when it can't be read
    float get_dpi_scale(const wchar_t* device_name)
    {
        monitor_search search;
        search._device_name = device_name;
        EnumDisplayMonitors(nullptr, nullptr, find_monitor, reinterpret_cast<LPARAM>(&search));

        UINT dpi_x = 0;
        UINT dpi_y = 0;
        if (search._monitor == nullptr || FAILED(GetDpiForMonitor(search._monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y)) || dpi_x == 0)
        {
            return 0.f;
        }

        return static_cast<float>(dpi_x) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
    }
} // end of anonymous namespace

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void system_info::load_os_info_internal()
{
    // RtlGetVersion - the system's version (GetVersionEx gives a process without a compatibility manifest Windows 8's)
    using rtl_get_version_fn = LONG(WINAPI*)(RTL_OSVERSIONINFOW*);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const rtl_get_version_fn rtl_get_version = ntdll != nullptr
        ? reinterpret_cast<rtl_get_version_fn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;

    RTL_OSVERSIONINFOW version = {};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtl_get_version == nullptr || rtl_get_version(&version) != 0)
    {
        *_errors.add() << "Windows' version can't be read: RtlGetVersion failed";
        return;
    }

    _os_info._major = version.dwMajorVersion;
    _os_info._minor = version.dwMinorVersion;
    _os_info._build = version.dwBuildNumber;

    // the registry's: the update build revision, the display version (none before 20H2), the product name
    DWORD revision = 0;
    DWORD size = sizeof(revision);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, windows_version_key, L"UBR", RRF_RT_REG_DWORD, nullptr, &revision, &size) == ERROR_SUCCESS)
    {
        _os_info._revision = revision;
    }

    read_version_text(L"DisplayVersion", _os_info._display_version);

    coid::charstr product_name;
    read_version_text(L"ProductName", product_name);
    _os_info._name = get_windows_name(product_name, _os_info._build);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void system_info::load_cpu_info_internal()
{
    // the CPU's description by CPUID - only the leaves the CPU has; XCR0 only with OSXSAVE (XGETBV faults without it)
    constexpr uint32 leaf_1_ecx_osxsave = 1u << 27;
    cpuid_leaves leaves;
    leaves._leaf_0 = read_cpuid(0, 0);
    if (leaves._leaf_0._eax >= 1)
    {
        leaves._leaf_1 = read_cpuid(1, 0);
    }
    if (leaves._leaf_0._eax >= 7)
    {
        leaves._leaf_7 = read_cpuid(7, 0);
    }
    if (read_cpuid(0x80000000, 0)._eax >= 0x80000004)
    {
        for (uint32 i = 0; i < 3; ++i)
        {
            leaves._brand[i] = read_cpuid(0x80000002 + i, 0);
        }
    }
    if ((leaves._leaf_1._ecx & leaf_1_ecx_osxsave) != 0)
    {
        leaves._xcr0 = _xgetbv(0);
    }

    decode_cpuid(leaves, _cpu_info);

    // the cores and the caches by the system - a record per physical core and per cache
    DWORD size = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &size);
    coid::dynarray32<uint8> records;
    records.resize(size);
    if (size == 0 || !GetLogicalProcessorInformationEx(RelationAll, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(records.ptr()), &size))
    {
        *_errors.add() << "The CPU's cores can't be read: GetLogicalProcessorInformationEx failed (" << GetLastError() << ')';
        return;
    }

    // the cores and the caches kept - their logical processors a task system's affinities
    _cpu_info._cores.reset();
    _cpu_info._caches.reset();
    for (DWORD offset = 0; offset < size;)
    {
        const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX& record = *reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(records.ptr() + offset);
        if (record.Size == 0)
        {
            break;
        }

        if (record.Relationship == RelationProcessorCore)
        {
            // its logical processors - a core is in one processor group -, its threads its masks' bits
            const PROCESSOR_RELATIONSHIP& processor = record.Processor;
            uint32 thread_count = 0;
            for (WORD group = 0; group < processor.GroupCount; ++group)
            {
                for (KAFFINITY mask = processor.GroupMask[group].Mask; mask != 0; mask &= mask - 1)
                {
                    ++thread_count;
                }
            }

            cpu_core& core = *_cpu_info._cores.add();
            core._logical_processors = get_affinity(processor.GroupMask[0]);
            core._thread_count = thread_count;
            core._efficiency_class = processor.EfficiencyClass;
        }
        else if (record.Relationship == RelationCache)
        {
            // its logical processors - its first processor group's when the system reports more than one (GroupCount, 0
            // before Windows 11: one group mask)
            const CACHE_RELATIONSHIP& cache_record = record.Cache;
            cpu_cache& cache = *_cpu_info._caches.add();
            cache._logical_processors = get_affinity(cache_record.GroupMask);
            cache._level = cache_record.Level;
            cache._type = get_cache_type(cache_record.Type);
            cache._associativity = cache_record.Associativity;
            cache._line_size = cache_record.LineSize;
            cache._size = cache_record.CacheSize;
        }

        offset += record.Size;
    }

    summarize_cpu_topology(_cpu_info);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void system_info::load_memory_info_internal()
{
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status))
    {
        *_errors.add() << "The memory's size can't be read: GlobalMemoryStatusEx failed (" << GetLastError() << ')';
        return;
    }

    _physical_memory_size = status.ullTotalPhys;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void system_info::load_display_info_internal()
{
    // the active displays: a path each - its source (the desktop's part) and its target (the monitor)
    coid::dynarray32<DISPLAYCONFIG_PATH_INFO> paths;
    coid::dynarray32<DISPLAYCONFIG_MODE_INFO> modes;
    LONG result = ERROR_SUCCESS;
    do
    {
        UINT32 path_count = 0;
        UINT32 mode_count = 0;
        result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count);
        if (result != ERROR_SUCCESS)
        {
            break;
        }

        paths.resize(path_count);
        modes.resize(mode_count);
        result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.ptr(), &mode_count, modes.ptr(), nullptr);
        paths.resize(path_count);
        modes.resize(mode_count);
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    if (result != ERROR_SUCCESS)
    {
        // e.g. ERROR_ACCESS_DENIED in a remote session - no displays
        *_errors.add() << "The displays can't be read: the display configuration's query failed (" << result << ')';
        return;
    }

    coid::dynarray32<display_info> displays;
    for (const DISPLAYCONFIG_PATH_INFO& path : paths)
    {
        // the path's source and target modes by their indices (no QDC_VIRTUAL_MODE_AWARE)
        const UINT32 source_index = path.sourceInfo.modeInfoIdx;
        const UINT32 target_index = path.targetInfo.modeInfoIdx;
        if (source_index >= modes.size() || target_index >= modes.size()
            || modes[source_index].infoType != DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE
            || modes[target_index].infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET)
        {
            *_errors.add() << "A display left out: its path has no source or target mode";
            continue;
        }

        const DISPLAYCONFIG_SOURCE_MODE& source_mode = modes[source_index].sourceMode;
        const DISPLAYCONFIG_TARGET_MODE& target_mode = modes[target_index].targetMode;

        // the source's GDI name - the legacy API's display ("\\.\DISPLAY2")
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name = {};
        source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source_name.header.size = sizeof(source_name);
        source_name.header.adapterId = path.sourceInfo.adapterId;
        source_name.header.id = path.sourceInfo.id;
        result = DisplayConfigGetDeviceInfo(&source_name.header);
        if (result != ERROR_SUCCESS)
        {
            *_errors.add() << "A display left out: its source's name can't be read (" << result << ')';
            continue;
        }

        // the monitor's name - none when it has none
        DISPLAYCONFIG_TARGET_DEVICE_NAME target_name = {};
        target_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target_name.header.size = sizeof(target_name);
        target_name.header.adapterId = path.targetInfo.adapterId;
        target_name.header.id = path.targetInfo.id;
        const bool has_target_name = DisplayConfigGetDeviceInfo(&target_name.header) == ERROR_SUCCESS;

        display_info& info = *displays.add();
        if (has_target_name)
        {
            append_utf8_text(target_name.monitorFriendlyDeviceName, info._display_name);
        }
        append_utf8_text(source_name.viewGdiDeviceName, info._device_name);
        info._width = source_mode.width;
        info._height = source_mode.height;
        info._desktop_x = source_mode.position.x;
        info._desktop_y = source_mode.position.y;

        const DISPLAYCONFIG_RATIONAL& vsync = target_mode.targetVideoSignalInfo.vSyncFreq;
        info._exact_refresh_rate = vsync.Denominator != 0 ? static_cast<float>(static_cast<double>(vsync.Numerator) / vsync.Denominator) : 0.f;
        info._dpi_scale = get_dpi_scale(source_name.viewGdiDeviceName);

        // the current mode's refresh rate in the modes' terms - the legacy API lists a 59.94 Hz mode as 59, the rounded exact
        // rate would be 60
        DEVMODEW mode = {};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsW(source_name.viewGdiDeviceName, ENUM_CURRENT_SETTINGS, &mode))
        {
            info._refresh_rate = mode.dmDisplayFrequency;
        }
        else
        {
            *_errors.add() << "The current mode of the display " << info._device_name << " can't be read - its refresh rate rounded";
            info._refresh_rate = static_cast<uint32>(info._exact_refresh_rate + 0.5f);
        }

        // the modes: (width, height, refresh rate) once each - a mode's color depths and scalings are one
        for (DWORD mode_index = 0; EnumDisplaySettingsW(source_name.viewGdiDeviceName, mode_index, &mode); ++mode_index)
        {
            display_mode available_mode;
            available_mode._width = mode.dmPelsWidth;
            available_mode._height = mode.dmPelsHeight;
            available_mode._refresh_rate = mode.dmDisplayFrequency;
            if (!info._available_refresh_rates.contains_sorted(available_mode._refresh_rate))
            {
                info._available_refresh_rates.push_sort(available_mode._refresh_rate);
            }

            info._available_modes.push_unique(available_mode);
        }
    }

    // the primary display first, the others by their places on the desktop
    coid::dynarray32<uint32> order;
    get_display_order(displays.ptr(), displays.size(), order);
    for (uint32 index : order)
    {
        _display_info.push(std::move(displays[index]));
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

} // end of namespace coid
