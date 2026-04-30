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
* Outerra
* Portions created by the Initial Developer are Copyright (C) 2017
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

#include "commtypes.h"
#include "commassert.h"
#include "bitrange.h"
#include "range.h"

COID_NAMESPACE_BEGIN

namespace detail
{
    template<uint32 bit_size>
    using bits_to_uint_type =
        std::conditional_t<(bit_size <= 8), uint8,
        std::conditional_t<(bit_size <= 16), uint16,
        std::conditional_t<(bit_size <= 32), uint32, uint64>>>;

    template<uint32 max_value>
    using max_value_to_uint_type =
        std::conditional_t<(max_value <= 0xff), uint8,
        std::conditional_t<(max_value <= 0xffff), uint16,
        std::conditional_t<(max_value <= 0xffffffff), uint32, uint64>>>;



}; // end of namespace detail


template <uint32 bit_size, typename bit_index_type = detail::max_value_to_uint_type<bit_size>>
class bitfield
{
protected: // inner definitions only
    using word_type = detail::bits_to_uint_type<bit_size>;
    using bit_index_uint_type = detail::bits_to_uint_type<sizeof(bit_index_type) << 3>;
    static constexpr uint8 word_size = sizeof(word_type);
    static constexpr uint8 word_bit_size = sizeof(word_type) << 3;
    static constexpr bit_index_uint_type word_bit_index_mask = word_bit_size - 1;
    static constexpr uint8 bit_index_to_word_index_shift = word_size == 8 ? 6 : word_size == 4 ? 5 : word_size == 2 ? 4 : word_size == 1 ? 3 : -1;
    
    static_assert(bit_index_to_word_index_shift != -1);

    static constexpr uint32 word_count = coid::align_to_chunks(bit_size, word_bit_size);

    static constexpr uints byte_size = word_size * word_count;

public:

    static constexpr bit_index_type invalid_bit_index = bit_index_type(-1);

    /// @brief Get set bit count
    /// @return Count of the bits set
    uint32 get_set_bit_count() const 
    {
        uint32 result = 0;
        for (uint32 i = 0; i < word_count; ++i)
        {
            result += population_count(_bits[i]);
        }

        return result;
    }

    /// @brief Get unset bit count
    /// @return Count of the bits unset
    uint32 get_unset_bit_count() const
    {
        return bit_size - get_set_bit_count();
    }

    /// @brief Checks all bits are unset
    /// @return true if all bits are 0
    bool is_none_set() const
    {
        for (uint32 i = 0; i < word_count; ++i)
        {
            if (_bits[i] != 0)
            {
                return false;
            }
        }

        return true;
    }
    
    /// @brief Checks if any bit is set
    /// @return true if any of the bits is set
    bool is_any_set() const 
    {
        for (uint32 i = 0; i < word_count; ++i)
        {
            if (_bits[i] != 0)
            {
                return true;
            }
        }

        return false;
    }

    /// @brief Get index of the next set bit starting from given bit index
    /// @tparam noassert - When true the check and assert on bit index validity is disabled
    /// @param bit_index - starting bit index or invalid_bit_index for the start from the first index
    /// @return Index of the first set bit with higher index than bit_index
    template<bool noassert = false>
    bit_index_type get_next_set_bit_index(bit_index_type bit_index) const
    {
        const bit_index_uint_type next_bit_index = static_cast<bit_index_uint_type>(bit_index) + 1;
        if coid_constexpr_if(!noassert)
        {
            DASSERT_RETX(next_bit_index >= 0 && next_bit_index <= bit_size, "Invalid bit index", -1);
        }

        uint32 word_index = next_bit_index >> bit_index_to_word_index_shift;

        if (word_index < word_count)
        {
            const bit_index_uint_type word_bit_index = next_bit_index & word_bit_index_mask;
            word_type word = _bits[word_index] & (static_cast<word_type>(-1) << word_bit_index);

            do
            {
                if (word != 0)
                {
                    return bit_index_type((word_index << bit_index_to_word_index_shift) + lsb_bit_set(word));
                }

                word = _bits[++word_index];
            } while (word_index < word_count);
        }

        return invalid_bit_index;
    }

    /// @brief Get index of the previous set bit starting from given bit index
    /// @tparam noassert - When true the check and assert on bit index validity is disabled
    /// @param bit_index - starting bit index or bit_size for the start from the last index
    /// @return Index of the first set bit with lesser index than bit_index
    template<bool noassert = false>
    bit_index_type get_prev_set_bit_index(bit_index_type bit_index) const
    {
        const bit_index_uint_type next_bit_index = static_cast<bit_index_uint_type>(bit_index) - 1;
        if coid_constexpr_if(!noassert)
        {
            DASSERT_RETX(bit_index > 0 && next_bit_index <= bit_size, "Invalid bit index", -1);
        }

        uint32 word_index = next_bit_index >> bit_index_to_word_index_shift;

        if (word_index < word_count)
        {
            const uint32 word_bit_index = next_bit_index & word_bit_index_mask;
            word_type word = _bits[word_index] & (static_cast<word_type>(-1) >> (word_bit_size - word_bit_index - 1));
            
            do
            {
                if (word != 0)
                {
                    return bit_index_type((word_index << bit_index_to_word_index_shift) + msb_bit_set(word));
                }

                word = _bits[--word_index];
            } while (word_index < word_count);
        }

        return invalid_bit_index;
    }

    /// @brief Get the index of the first bit that is set
    /// @return Index of the frist bit set or -1 if none
    bit_index_type get_first_set_bit_index() const
    {
        return get_next_set_bit_index<true>(-1);
    }

    /// @brief Get the index of the last bit that is set
    /// @return Index of the last bit set or -1 if none
    bit_index_type get_last_set_bit_index() const
    {
        return get_prev_set_bit_index<true>(bit_size);
    }

    /// @brief Checks if bit on provided index is set
    /// @param bit_index - index of the bit to check
    /// @return true if the bit on provided index is set
    bool is_bit_set(bit_index_type bit_index) const
    {
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_size, "Invalid bit index", false);

        const word_type bit_mask = word_type(1) << (static_cast<uint32>(bit_index) & word_bit_index_mask);
        return _bits[static_cast<uint32>(bit_index) >> bit_index_to_word_index_shift] & bit_mask;
    }

    /// @brief Sets bit on provided index
    /// @param bit_index - index of the bit to set
    void set_bit(bit_index_type bit_index)
    {
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_size, "Invalid bit index");

        const word_type bit_mask = word_type(1) << (static_cast<uint32>(bit_index) & word_bit_index_mask);
        _bits[static_cast<uint32>(bit_index) >> bit_index_to_word_index_shift] |= bit_mask;
    }

    /// @brief Unsets bit on provided index
    /// @param bit_index - index of the bit to unset
    void unset_bit(bit_index_type bit_index)
    {
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_size, "Invalid bit index");

        const word_type bit_mask = word_type(1) << (static_cast<uint32>(bit_index) & word_bit_index_mask);
        _bits[static_cast<uint32>(bit_index) >> bit_index_to_word_index_shift] &= ~bit_mask;

    }

    /// @brief Sets all bits to 0
    void unset_all_bits() 
    {
        memset(_bits, 0, sizeof(_bits));
    }

    /// @brief Sets all bits to 1
    void set_all_bits()
    {
        memset(_bits, 0xff, sizeof(_bits));
    }

    /// @brief Flips the bit value of the bit on provided index
    /// @param bit_index - index of the bit to flip
    void flip_bit(bit_index_type bit_index)
    {
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_size, "Invalid bit index");

        const word_type bit_mask = word_type(1) << (static_cast<uint32>(bit_index) & word_bit_index_mask);
        _bits[static_cast<uint32>(bit_index) >> bit_index_to_word_index_shift] ^= bit_mask;
    }

    template<typename T>
    void insert_bits(const T& value, uint32 bit_count, uint32 write_bit_offset, uint32 read_bit_offset = 0)
    {
        DASSERT_RETX(bit_count > 0, "Must be > 0");
        DASSERT_RETX(write_bit_offset + bit_count < bit_size, "Bits out of bounds");
        DASSERT_RETX(read_bit_offset + bit_count < sizeof(T) << 3, "Bits out of bounds");

        do
        {
            const uint32 source_word_index = read_bit_offset >> bit_index_to_word_index_shift;
            const uint32 destination_word_index = write_bit_offset >> bit_index_to_word_index_shift;
            const word_type* source_word_ptr = reinterpret_cast<const word_type*>(&value) + source_word_index;
            word_type* destination_word_ptr = reinterpret_cast<word_type*>(&_bits) + destination_word_index;

            const bit_index_uint_type source_word_bit_offset = read_bit_offset & word_bit_index_mask;
            const bit_index_uint_type destination_word_bit_offset = write_bit_offset & word_bit_index_mask;
            bit_index_uint_type bits_to_process_count = source_word_bit_offset > destination_word_bit_offset ? word_bit_size - source_word_bit_offset : word_bit_size - destination_word_bit_offset;
            if (bits_to_process_count > bit_count)
            {
                bits_to_process_count = bit_count;
            }

            const word_type mask_base = bits_to_process_count == word_bit_size ? word_type(-1) : ((word_type(1) << bits_to_process_count) - 1);
            const word_type read_mask = mask_base << source_word_bit_offset;
            const word_type write_mask = mask_base << destination_word_bit_offset;

            // clear the bit section in the destination word
            *destination_word_ptr &= ~(write_mask);
            const word_type word_to_write = (*source_word_ptr) & read_mask;

            if (source_word_bit_offset < destination_word_bit_offset)
            {
                *destination_word_ptr |= word_to_write << (destination_word_bit_offset - source_word_bit_offset);
            }
            else
            {
                *destination_word_ptr |= word_to_write >> (source_word_bit_offset - destination_word_bit_offset);
            }

            write_bit_offset += bits_to_process_count;
            read_bit_offset += bits_to_process_count;
            bit_count -= bits_to_process_count;
        } 
        while (bit_count > 0);
    }

    /// @brief Resets all the bits to 0
    void reset() 
    {
        for (uint32 i = 0; i < word_count; ++i)
        {
            _bits[i] = word_type(0);
        }
    }

    /// @brief Iterate over all the set bits and call the function
    /// @tparam FN - void fn(bit_index_type bit_index)
    /// @param fn - Function to call for every bit set
    template<typename FN>
    void for_each_bit_set(const FN& fn) const
    {
        for (uint32 word_index = 0; word_index < word_count; ++word_index)
        {
            word_type word = _bits[word_index];

            while (word != 0)
            {
                uint8 bit_index = lsb_bit_set(word);
                fn(bit_index_type((word_index << bit_index_to_word_index_shift) | bit_index));

                word ^= (static_cast<word_type>(1) << bit_index);
            }
        }
    }

    /// @brief Bitwise "AND" operator with bitfield operand
    /// @param rhs - bitfield value to perform operation with
    /// @return Bitfield resulting from "AND" operation between operands
    bitfield operator&(const bitfield& rhs) const
    {
        bitfield result;
        for (uint32 i = 0; i < word_count; ++i)
        {
            result._bits[i] = _bits[i] & rhs._bits[i];
        }

        return result;
    }

    /// @brief 	Unary complement operator
    /// @return Bit inverted bitfield
    bitfield operator~() const
    {
        bitfield result;
        for (uint32 i = 0; i < word_count; ++i)
        {
            result._bits[i] = ~_bits[i];
        }

        return result;
    }

    /// @brief Check if if the bit on the bit index is set
    /// @param bit_index - index to check
    /// @return True when the bit is set
    bool operator&(bit_index_type bit_index) const
    {
        return is_bit_set(bit_index);
    }

    bitfield(const coid::range<uint8>& bytes)
    {
        DASSERT_RETX(bytes.byte_size() == byte_size, "Invalid byte size");

        memcpy(_bits, bytes.ptr(), byte_size);
    }

    bitfield(std::initializer_list<bit_index_type> bits_to_set)
    {
        for (bit_index_type i : bits_to_set)
        {
            set_bit(i);
        }
    }

    bitfield() = default;

protected: // members only
    word_type _bits[word_count] = {word_type(0), };
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<typename T, T count_value>
COID_REQUIRES((std::is_enum_v<T>))
using enum_bitfield = bitfield<static_cast<uint32>(count_value), T>;


COID_NAMESPACE_END