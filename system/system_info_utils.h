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

#include "../commtypes.h"
#include "../dynarray.h"
#include "../str.h"
#include "../token.h"

/// @file
/// @brief The system information's pure part: system_info's platform part reads the system's values, these functions
/// decode, summarize, order and format them - without the system, so any CPU's or display's values can be given to them

namespace coid {

/// @brief A CPUID leaf's registers
struct cpuid_registers
{
    uint32 _eax = 0;
    uint32 _ebx = 0;
    uint32 _ecx = 0;
    uint32 _edx = 0;
};

/// @brief The CPUID leaves the CPU's description is decoded from (decode_cpuid) - zero where the CPU has none
struct cpuid_leaves
{
    cpuid_registers _leaf_0;                                                    ///< The highest leaf (EAX), the vendor (EBX, EDX, ECX)
    cpuid_registers _leaf_1;                                                    ///< The signature (EAX), the features (ECX)
    cpuid_registers _leaf_7;                                                    ///< Subleaf 0: the extended features (EBX) - when the highest leaf is 7 or more
    cpuid_registers _brand[3];                                                  ///< Leaves 0x80000002 - 0x80000004: the brand string
    uint64 _xcr0 = 0;                                                           ///< The registers the operating system saves (XGETBV 0); 0 without OSXSAVE
};

/// @brief The first build of Windows 11 - its registry's product name says Windows 10 (get_windows_name)
constexpr uint32 windows_11_first_build = 22000;

/// @brief Decodes the CPU's description from its CPUID leaves: the vendor, the brand without its padding (older Intel brands
/// start with spaces), the family and the model with their extended fields (the extended family when the family is 15, the
/// extended model when it's 6 or 15), the stepping, the features with the operating system's support of their registers
/// (XCR0's SSE and AVX state for AVX, AVX2, FMA and F16C; AVX-512's opmask and ZMM state too for AVX-512F; leaf 7 only when
/// the highest leaf has it)
/// @param info_out - its description's fields set, its cores and caches untouched
void decode_cpuid(const cpuid_leaves& leaves, cpu_info& info_out);

/// @brief Summarizes the CPU's cores and caches (cpu_info::_cores, _caches - the operating system's: Windows'
/// GetLogicalProcessorInformationEx): the cores, the performance cores (the highest efficiency class's), the threads; per
/// level L1 - L3 its data and unified caches - their count, the smallest one's size, their total, the smallest line (the
/// instruction and the trace caches and the other levels left out)
/// @param info - its counts and cache levels set from its lists
void summarize_cpu_topology(cpu_info& info);

/// @brief Gets Windows' name: the registry's product name, its "Windows 10" "Windows 11" from windows_11_first_build on
/// @param product_name - the registry's ProductName ("Windows 10 Home")
/// @return "Windows 11 Home"; "Windows" without a product name
coid::charstr get_windows_name(const coid::token& product_name, uint32 build);

/// @brief Gets the displays' order: the primary display first (at the desktop's origin), the others from left to right, then
/// from top to bottom - displays at one position in their order
/// @param order_out - the displays' indices in the order
void get_display_order(const display_info* displays, uint32 display_count, coid::dynarray32<uint32>& order_out);

/// @brief Gets a CPU feature's name ("SSE4.2", "AVX-512F")
/// @param feature - one feature
/// @return its name; "unknown" for another value
coid::token get_cpu_feature_name(cpu_feature_enum_flags feature);

/// @brief Gets a cache type's name ("data", "instruction")
coid::token get_cpu_cache_type_name(cpu_cache_type_enum type);

/// @brief Appends a memory size: GiB with a decimal place from 1 GiB, MiB with up to two (no trailing zeros) from 1 MiB, KiB
/// below - rounded
/// @param size - bytes
void append_memory_size(uint64 size, coid::charstr& text_out);

/// @brief Formats the system's information into the log's lines (system_info::log_info): the operating system, the CPU, the
/// memory, a line per display
/// @param physical_memory_size - bytes; 0 - unknown, no line
/// @param lines_out - the lines, the previous ones removed
void format_system_info(const os_info& os, const cpu_info& cpu, uint64 physical_memory_size, const display_info* displays,
    uint32 display_count, coid::dynarray32<coid::charstr>& lines_out);

} // end of namespace coid
