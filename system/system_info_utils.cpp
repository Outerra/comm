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

#include "system_info_utils.h"

namespace coid {

namespace
{
    // CPUID's feature bits: leaf 1's ECX, leaf 7's EBX
    constexpr uint32 leaf_1_ecx_fma = 1u << 12;
    constexpr uint32 leaf_1_ecx_sse4_2 = 1u << 20;
    constexpr uint32 leaf_1_ecx_osxsave = 1u << 27;
    constexpr uint32 leaf_1_ecx_avx = 1u << 28;
    constexpr uint32 leaf_1_ecx_f16c = 1u << 29;
    constexpr uint32 leaf_7_ebx_avx2 = 1u << 5;
    constexpr uint32 leaf_7_ebx_avx512f = 1u << 16;

    // XCR0's state the operating system saves: SSE's XMM and AVX's upper YMM registers; AVX-512's opmask, ZMM_Hi256 and
    // Hi16_ZMM too
    constexpr uint64 xcr0_avx_state = 0x6;
    constexpr uint64 xcr0_avx512_state = 0xe6;

    /// @brief Appends a register's four characters - its lowest byte first - up to a NUL
    /// @return false when a NUL ended the text
    bool append_register_text(uint32 value, coid::charstr& text_out)
    {
        for (uint32 i = 0; i < 4; ++i)
        {
            const char c = static_cast<char>((value >> (8 * i)) & 0xff);
            if (c == 0)
            {
                return false;
            }
            text_out.append(c);
        }

        return true;
    }
} // end of anonymous namespace

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::token get_cpu_feature_name(cpu_feature_enum_flags feature)
{
    switch (feature)
    {
        case cpu_feature_enum_flags::sse4_2: return "SSE4.2"_T;
        case cpu_feature_enum_flags::avx: return "AVX"_T;
        case cpu_feature_enum_flags::avx2: return "AVX2"_T;
        case cpu_feature_enum_flags::fma: return "FMA"_T;
        case cpu_feature_enum_flags::f16c: return "F16C"_T;
        case cpu_feature_enum_flags::avx512f: return "AVX-512F"_T;
        default: return "unknown"_T;
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::token get_cpu_cache_type_name(cpu_cache_type_enum type)
{
    switch (type)
    {
        case cpu_cache_type_enum::unified: return "unified"_T;
        case cpu_cache_type_enum::instruction: return "instruction"_T;
        case cpu_cache_type_enum::data: return "data"_T;
        case cpu_cache_type_enum::trace: return "trace"_T;
        default: return "unknown"_T;
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void append_memory_size(uint64 size, coid::charstr& text_out)
{
    constexpr uint64 kib = 1024;
    constexpr uint64 mib = kib * 1024;
    constexpr uint64 gib = mib * 1024;

    if (size >= gib)
    {
        const uint64 tenths = (size * 10 + gib / 2) / gib;
        text_out << tenths / 10 << '.' << tenths % 10 << " GiB";
    }
    else if (size >= mib)
    {
        const uint64 hundredths = (size * 100 + mib / 2) / mib;
        text_out << hundredths / 100;
        if (hundredths % 100 != 0)
        {
            text_out << '.' << (hundredths % 100) / 10;
            if (hundredths % 10 != 0)
            {
                text_out << hundredths % 10;
            }
        }
        text_out << " MiB";
    }
    else
    {
        text_out << (size + kib / 2) / kib << " KiB";
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void decode_cpuid(const cpuid_leaves& leaves, cpu_info& info_out)
{
    // the vendor: EBX, EDX, ECX
    info_out._vendor.reset();
    const uint32 vendor_registers[3] = { leaves._leaf_0._ebx, leaves._leaf_0._edx, leaves._leaf_0._ecx };
    for (uint32 value : vendor_registers)
    {
        if (!append_register_text(value, info_out._vendor))
        {
            break;
        }
    }

    // the brand: 48 characters in EAX, EBX, ECX and EDX of three leaves, a shorter one ended by a NUL - its padding spaces
    // left out (older Intel brands are right-aligned)
    coid::charstr brand;
    bool is_brand_ended = false;
    for (const cpuid_registers& leaf : leaves._brand)
    {
        const uint32 brand_registers[4] = { leaf._eax, leaf._ebx, leaf._ecx, leaf._edx };
        for (uint32 value : brand_registers)
        {
            is_brand_ended = is_brand_ended || !append_register_text(value, brand);
        }
    }

    coid::token brand_text = brand;
    brand_text.skip_whitespace();
    brand_text.trim_whitespace();
    info_out._brand = brand_text;

    // the signature: the stepping, the model and the family - the extended family added to family 15, the extended model to
    // families 6 and 15
    const uint32 signature = leaves._leaf_1._eax;
    const uint32 base_family = (signature >> 8) & 0xf;
    const uint32 base_model = (signature >> 4) & 0xf;
    info_out._stepping = signature & 0xf;
    info_out._family = base_family == 0xf ? base_family + ((signature >> 20) & 0xff) : base_family;
    info_out._model = base_family == 0x6 || base_family == 0xf ? base_model + (((signature >> 16) & 0xf) << 4) : base_model;

    // the features: the AVX family's with the operating system's AVX state, AVX-512's with its state too; leaf 7 only when the
    // CPU has it (a higher leaf returns another leaf's values)
    const uint32 features = leaves._leaf_1._ecx;
    const uint32 extended_features = leaves._leaf_0._eax >= 7 ? leaves._leaf_7._ebx : 0;
    const uint64 xcr0 = (features & leaf_1_ecx_osxsave) != 0 ? leaves._xcr0 : 0;
    const bool has_avx = (features & leaf_1_ecx_avx) != 0 && (xcr0 & xcr0_avx_state) == xcr0_avx_state;
    const bool has_avx512_state = (xcr0 & xcr0_avx512_state) == xcr0_avx512_state;

    info_out._features = cpu_feature_enum_flags::none;
    if ((features & leaf_1_ecx_sse4_2) != 0)
    {
        info_out._features |= cpu_feature_enum_flags::sse4_2;
    }

    if (has_avx)
    {
        info_out._features |= cpu_feature_enum_flags::avx;
        if ((extended_features & leaf_7_ebx_avx2) != 0)
        {
            info_out._features |= cpu_feature_enum_flags::avx2;
        }
        if ((features & leaf_1_ecx_fma) != 0)
        {
            info_out._features |= cpu_feature_enum_flags::fma;
        }
        if ((features & leaf_1_ecx_f16c) != 0)
        {
            info_out._features |= cpu_feature_enum_flags::f16c;
        }
        if (has_avx512_state && (extended_features & leaf_7_ebx_avx512f) != 0)
        {
            info_out._features |= cpu_feature_enum_flags::avx512f;
        }
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void summarize_cpu_topology(cpu_info& info)
{
    // the performance cores: the highest efficiency class's - every core when the CPU has one class
    uint8 highest_efficiency_class = 0;
    for (const cpu_core& core : info._cores)
    {
        if (core._efficiency_class > highest_efficiency_class)
        {
            highest_efficiency_class = core._efficiency_class;
        }
    }

    info._core_count = info._cores.size();
    info._performance_core_count = 0;
    info._thread_count = 0;
    for (const cpu_core& core : info._cores)
    {
        info._thread_count += core._thread_count;
        if (core._efficiency_class == highest_efficiency_class)
        {
            ++info._performance_core_count;
        }
    }

    // the levels' data and unified caches - an L1 and an L2 per core or per cluster of cores, an L3 per die
    for (cpu_cache_level& level : info._cache_levels)
    {
        level = cpu_cache_level();
    }

    for (const cpu_cache& cache : info._caches)
    {
        const bool holds_data = cache._type == cpu_cache_type_enum::data || cache._type == cpu_cache_type_enum::unified;
        if (!holds_data || cache._level < 1 || cache._level > cpu_cache_level_count)
        {
            continue;
        }

        cpu_cache_level& level = info._cache_levels[cache._level - 1];
        if (level._count == 0 || cache._size < level._smallest_size)
        {
            level._smallest_size = cache._size;
        }
        if (level._count == 0 || cache._line_size < level._line_size)
        {
            level._line_size = cache._line_size;
        }
        level._total_size += cache._size;
        ++level._count;
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

coid::charstr get_windows_name(const coid::token& product_name, uint32 build)
{
    if (product_name.is_empty())
    {
        return coid::charstr("Windows");
    }

    // Windows 11's registry keeps Windows 10's product name
    const coid::token windows_10 = "Windows 10";
    coid::charstr name;
    if (build >= windows_11_first_build && product_name.begins_with(windows_10))
    {
        name << "Windows 11" << coid::token(product_name.ptr() + windows_10.len(), product_name.ptre());
    }
    else
    {
        name = product_name;
    }

    return name;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void get_display_order(const display_info* displays, uint32 display_count, coid::dynarray32<uint32>& order_out)
{
    order_out.reset();

    // the primary display before another, then the one more left, then the one higher
    auto is_before = [displays](uint32 a, uint32 b)
    {
        const display_info& display_a = displays[a];
        const display_info& display_b = displays[b];
        const bool is_primary_a = display_a.is_primary();
        const bool is_primary_b = display_b.is_primary();
        if (is_primary_a != is_primary_b)
        {
            return is_primary_a;
        }

        return display_a._desktop_x != display_b._desktop_x ? display_a._desktop_x < display_b._desktop_x : display_a._desktop_y < display_b._desktop_y;
    };

    // an insertion sort - stable: displays at one position (duplicated) keep their order
    for (uint32 i = 0; i < display_count; ++i)
    {
        uint32 place = order_out.size();
        while (place > 0 && is_before(i, order_out[place - 1]))
        {
            --place;
        }
        *order_out.ins(place) = i;
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

void format_system_info(const os_info& os, const cpu_info& cpu, uint64 physical_memory_size, const display_info* displays,
    uint32 display_count, coid::dynarray32<coid::charstr>& lines_out)
{
    lines_out.reset();

    // the operating system: Windows 11 Home 26H2 (10.0.26300.9711) - each line finished before the next one is added (an
    // addition can move the lines)
    {
        coid::charstr& line = *lines_out.add();
        if (os._name.is_set())
        {
            line << os._name;
        }
        else
        {
            line << "Unknown operating system";
        }

        if (os._display_version.is_set())
        {
            line << ' ' << os._display_version;
        }

        if (os._major != 0)
        {
            line << " (" << os._major << '.' << os._minor << '.' << os._build;
            if (os._revision != 0)
            {
                line << '.' << os._revision;
            }
            line << ')';
        }
    }

    // the CPU: its brand and identification, its cores and threads, its caches, its features
    {
        coid::charstr& line = *lines_out.add();
        line << "CPU ";
        if (cpu._brand.is_set())
        {
            line << cpu._brand;
        }
        else
        {
            line << "of an unknown brand";
        }

        line << " (";
        if (cpu._vendor.is_set())
        {
            line << cpu._vendor;
        }
        else
        {
            line << "unknown vendor";
        }
        line << ", family " << cpu._family << ", model " << cpu._model << ", stepping " << cpu._stepping << ')';

        if (cpu._core_count > 0)
        {
            line << ": " << cpu._core_count << " cores";
            if (cpu._performance_core_count < cpu._core_count)
            {
                line << " - " << cpu._performance_core_count << " performance";
            }
            line << ", " << cpu._thread_count << " threads";
        }

        // the cache levels' totals - L1's data caches -, their smallest line
        constexpr const char* level_names[cpu_cache_level_count] = { "L1d ", "L2 ", "L3 " };
        uint16 line_size = 0;
        bool is_first_level = true;
        for (uint32 i = 0; i < cpu_cache_level_count; ++i)
        {
            const cpu_cache_level& level = cpu._cache_levels[i];
            if (level._count == 0)
            {
                continue;
            }

            line << (is_first_level ? "; " : ", ") << level_names[i];
            append_memory_size(level._total_size, line);
            is_first_level = false;
            if (level._line_size > 0 && (line_size == 0 || level._line_size < line_size))
            {
                line_size = level._line_size;
            }
        }

        if (line_size > 0)
        {
            line << ", " << line_size << "-byte lines";
        }

        bool is_first_feature = true;
        for (uint32 bit = 0; bit < cpu_feature_count; ++bit)
        {
            const cpu_feature_enum_flags feature = static_cast<cpu_feature_enum_flags>(1u << bit);
            if (has_cpu_feature(cpu._features, feature))
            {
                line << (is_first_feature ? "; " : ", ") << get_cpu_feature_name(feature);
                is_first_feature = false;
            }
        }
    }

    // the memory
    if (physical_memory_size > 0)
    {
        coid::charstr& line = *lines_out.add();
        line << "Memory ";
        append_memory_size(physical_memory_size, line);
    }

    // the displays: Display 0: \\.\DISPLAY2 "DELL S2721DGF" 2560x1440 at (0, 0), 143.97 Hz, 100%
    if (display_count == 0)
    {
        *lines_out.add() << "Displays: none";
    }

    for (uint32 i = 0; i < display_count; ++i)
    {
        const display_info& display = displays[i];
        coid::charstr& line = *lines_out.add();
        line << "Display " << i << ": ";
        if (display._device_name.is_set())
        {
            line << display._device_name << ' ';
        }
        line << '"' << display._display_name << "\" " << display._width << 'x' << display._height
            << " at (" << display._desktop_x << ", " << display._desktop_y << "), ";

        // the exact refresh rate with precisely two decimal places, otherwise the mode's
        if (display._exact_refresh_rate > 0.f)
        {
            line << coid::float_nfrac(display._exact_refresh_rate, -2);
        }
        else
        {
            line << display._refresh_rate;
        }
        line << " Hz";

        if (display._dpi_scale > 0.f)
        {
            line << ", " << static_cast<uint32>(display._dpi_scale * 100.f + 0.5f) << '%';
        }
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

} // end of namespace coid
