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

#include "../commtypes.h"
#include "../dynarray.h"
#include "../str.h"

/// @file
/// @brief The system's information (coid::system_info): its displays, its CPU, its operating system

namespace coid {

/// @brief A display's mode - its resolution and refresh rate (display_info::_available_modes)
struct display_mode
{
    uint32 _width = 0;                                                          ///< Pixels
    uint32 _height = 0;                                                         ///< Pixels
    uint32 _refresh_rate = 0;                                                   ///< Hz in the modes' terms (Windows: a 59.94 Hz mode's 59)

    bool operator == (const display_mode& other) const
    {
        return _width == other._width && _height == other._height && _refresh_rate == other._refresh_rate;
    }
};

/// @brief An active display of the system - its desktop part and its monitor (system_info::get_display_info)
struct display_info
{
    coid::charstr _display_name;                                                ///< The monitor's name ("DELL S2721DGF") - an application's setting can select
                                                                                ///< the display by it; empty when the monitor has none
    coid::charstr _device_name;                                                 ///< The system's name of the display's desktop part (Windows: "\\.\DISPLAY2")
    uint32 _width = 0;                                                          ///< The current mode's width in pixels
    uint32 _height = 0;                                                         ///< The current mode's height in pixels
    int32 _desktop_x = 0;                                                       ///< Its left edge on the desktop in pixels - the primary display's 0, a display
                                                                                ///< left of it negative
    int32 _desktop_y = 0;                                                       ///< Its top edge on the desktop in pixels - the primary display's 0, a display
                                                                                ///< above it negative
    uint32 _refresh_rate = 0;                                                   ///< The current mode's refresh rate in Hz in its modes' terms (Windows: a 59.94 Hz
                                                                                ///< mode's 59) - supports_mode compares it with them
    float _exact_refresh_rate = 0.f;                                            ///< The current mode's exact refresh rate in Hz (59.94); 0 when unknown
    float _dpi_scale = 0.f;                                                     ///< Its DPI divided by 96 (1.5 at 150%); 0 when unknown
    coid::dynarray32<uint32> _available_refresh_rates;                          ///< The modes' refresh rates in Hz, sorted
    coid::dynarray32<display_mode> _available_modes;                            ///< The modes, each once (Windows: a mode's color depths and scalings are one)

    /// @brief Checks whether the display is the primary one - at the desktop's origin
    bool is_primary() const
    {
        return _desktop_x == 0 && _desktop_y == 0;
    }

    /// @brief Checks whether the display has a mode
    /// @param refresh_rate - Hz in the modes' terms (_refresh_rate)
    /// @return true when the mode is one of the display's modes
    bool supports_mode(uint32 width, uint32 height, uint32 refresh_rate) const
    {
        display_mode mode;
        mode._width = width;
        mode._height = height;
        mode._refresh_rate = refresh_rate;
        return _available_modes.contains(mode) != nullptr;
    }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief The CPU's instruction set extensions above x64's baseline (SSE2) - each with the operating system's support of its
/// registers
enum class cpu_feature_enum_flags : uint32
{
    none = 0,
    sse4_2 = 1 << 0,                                                            ///< SSE4.2
    avx = 1 << 1,                                                               ///< AVX - the operating system saves the YMM registers
    avx2 = 1 << 2,                                                              ///< AVX2 - with AVX
    fma = 1 << 3,                                                               ///< FMA3 - with AVX
    f16c = 1 << 4,                                                              ///< F16C, the half floats' conversions - with AVX
    avx512f = 1 << 5,                                                           ///< AVX-512 Foundation - with AVX, the operating system saves the ZMM and the mask
                                                                                ///< registers too
};

/// @brief The features of cpu_feature_enum_flags - a bit each from the lowest
constexpr uint32 cpu_feature_count = 6;

constexpr cpu_feature_enum_flags operator & (const cpu_feature_enum_flags& lhs, const cpu_feature_enum_flags& rhs) { return cpu_feature_enum_flags(uint32(lhs) & uint32(rhs)); }
constexpr cpu_feature_enum_flags operator | (const cpu_feature_enum_flags& lhs, const cpu_feature_enum_flags& rhs) { return cpu_feature_enum_flags(uint32(lhs) | uint32(rhs)); }
constexpr cpu_feature_enum_flags& operator |= (cpu_feature_enum_flags& lhs, const cpu_feature_enum_flags& rhs) { lhs = lhs | rhs; return lhs; }

/// @brief Checks whether features contain a feature
/// @return true if the features contain any of the given ones
constexpr bool has_cpu_feature(cpu_feature_enum_flags features, cpu_feature_enum_flags feature) { return (features & feature) != cpu_feature_enum_flags::none; }

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief Logical processors of the CPU: a processor group (Windows' - at most 64 logical processors each) and a mask of
/// them in it - a thread's affinity
struct cpu_affinity
{
    uint16 _group = 0;                                                          ///< The processor group
    uint64 _mask = 0;                                                           ///< The logical processors in the group, a bit each
};

/// @brief A physical core of the CPU (cpu_info::_cores)
struct cpu_core
{
    cpu_affinity _logical_processors;                                           ///< Its logical processors - its SMT threads
    uint32 _thread_count = 0;                                                   ///< Its logical processors' count
    uint8 _efficiency_class = 0;                                                ///< Higher - faster: a hybrid CPU's performance cores; 0 on a CPU of one class
};

/// @brief What a cache of the CPU holds
enum class cpu_cache_type_enum : uint8
{
    unified,                                                                    ///< Instructions and data
    instruction,
    data,
    trace,                                                                      ///< Decoded instructions (Pentium 4's)
    unknown,
};

/// @brief A cache of the CPU (cpu_info::_caches) - one per core, per cluster of cores or per die
struct cpu_cache
{
    cpu_affinity _logical_processors;                                           ///< The logical processors sharing it - a cache the system reports in more than one
                                                                                ///< processor group by the first one
    uint8 _level = 0;                                                           ///< 1, 2, 3
    cpu_cache_type_enum _type = cpu_cache_type_enum::unknown;
    uint8 _associativity = 0;                                                   ///< Its ways; 0xff fully associative
    uint16 _line_size = 0;                                                      ///< Bytes
    uint32 _size = 0;                                                           ///< Bytes
};

/// @brief The cache levels cpu_info summarizes - L1, L2, L3
constexpr uint32 cpu_cache_level_count = 3;

/// @brief A cache level's data and unified caches together (cpu_info::_cache_levels, summarize_cpu_topology) - all 0
/// without the level
struct cpu_cache_level
{
    uint32 _count = 0;                                                          ///< Its caches - one per core, per cluster of cores or per die
    uint32 _smallest_size = 0;                                                  ///< Bytes - the smallest cache's: what a thread on any core can count on
    uint64 _total_size = 0;                                                     ///< Bytes - every cache of the level together
    uint16 _line_size = 0;                                                      ///< Bytes - the smallest line
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief The system's CPU (system_info::get_cpu_info) - identified by CPUID (decode_cpuid), its cores and caches by the
/// operating system (summarize_cpu_topology)
struct cpu_info
{
    coid::charstr _vendor;                                                      ///< CPUID's vendor ("GenuineIntel", "AuthenticAMD")
    coid::charstr _brand;                                                       ///< CPUID's brand string without its padding ("12th Gen Intel(R) Core(TM) i9-12900K")
    uint32 _family = 0;                                                         ///< With the extended family (25 - a Zen 4's; Windows' "Family 25")
    uint32 _model = 0;                                                          ///< With the extended model (151 - an Alder Lake's; Windows' "Model 151")
    uint32 _stepping = 0;
    cpu_feature_enum_flags _features = cpu_feature_enum_flags::none;            ///< The features the CPU and the operating system support
    uint32 _core_count = 0;                                                     ///< The physical cores
    uint32 _performance_core_count = 0;                                         ///< The cores of the highest efficiency class - a hybrid CPU's performance cores,
                                                                                ///< every core of a CPU of one class
    uint32 _thread_count = 0;                                                   ///< The logical processors
    cpu_cache_level _cache_levels[cpu_cache_level_count];                       ///< L1, L2, L3 ([level - 1]): their data and unified caches
    coid::dynarray32<cpu_core> _cores;                                          ///< Every physical core - their logical processors a task system's threads'
                                                                                ///< affinities
    coid::dynarray32<cpu_cache> _caches;                                        ///< Every cache: data, instruction, unified
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

/// @brief The operating system (system_info::get_os_info)
struct os_info
{
    coid::charstr _name;                                                        ///< "Windows 11 Home" (get_windows_name)
    coid::charstr _display_version;                                             ///< "26H2" - empty when the system has none
    uint32 _major = 0;                                                          ///< The version major.minor.build.revision (10.0.26300.9711)
    uint32 _minor = 0;
    uint32 _build = 0;
    uint32 _revision = 0;                                                       ///< Windows' update build revision (UBR)
};

} // end of namespace coid
