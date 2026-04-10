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

template <uint32 bit_count, typename word_type = uints, typename bit_index_type = uint8>
class bitfield
{
protected: // inner definitions only
    static constexpr uint8 word_size = sizeof(word_type);
    static constexpr uint8 word_bit_count = sizeof(word_type) << 3;
    static constexpr uint8 word_bit_index_mask = word_bit_count - 1;
    static constexpr uint8 bit_index_to_word_index_shift = word_size == 8 ? 6 : word_size == 4 ? 5 : word_size == 2 ? 4 : word_size == 1 ? 3 : -1;
    
    static_assert(bit_index_to_word_index_shift != -1);

    static constexpr uint32 word_count = coid::align_to_chunks(bit_count, word_bit_count);

    static constexpr uints byte_size = word_size * word_count;

public:

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
        return bit_count - get_set_bit_count();
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
    /// @param bit_index - starting bit index or -1 for the start from the first index
    /// @return Index of the first set bit with higher index than bit_index
    template<bool noassert = false>
    bit_index_type get_next_set_bit_index(bit_index_type bit_index) const
    {
        const uint32 next_bit_index = static_cast<uint32>(bit_index) + 1;
        if coid_constexpr_if(!noassert)
        {
            DASSERT_RETX(next_bit_index >= 0 && next_bit_index <= bit_count, "Invalid bit index", -1);
        }

        uint32 word_index = next_bit_index >> bit_index_to_word_index_shift;

        if (word_index < word_count)
        {
            const uint32 word_bit_index = next_bit_index & word_bit_index_mask;
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

        return bit_index_type(-1);
    }

    /// @brief Get index of the previous set bit starting from given bit index
    /// @tparam noassert - When true the check and assert on bit index validity is disabled
    /// @param bit_index - starting bit index or bit_count for the start from the last index
    /// @return Index of the first set bit with lesser index than bit_index
    template<bool noassert = false>
    bit_index_type get_prev_set_bit_index(bit_index_type bit_index) const
    {
        const uint32 next_bit_index = static_cast<uint32>(bit_index) - 1;
        if coid_constexpr_if(!noassert)
        {
            DASSERT_RETX(bit_index > 0 && next_bit_index <= bit_count, "Invalid bit index", -1);
        }

        uint32 word_index = next_bit_index >> bit_index_to_word_index_shift;

        if (word_index < word_count)
        {
            const uint32 word_bit_index = next_bit_index & word_bit_index_mask;
            word_type word = _bits[word_index] & (static_cast<word_type>(-1) >> (word_bit_count - word_bit_index - 1));
            
            do
            {
                if (word != 0)
                {
                    return bit_index_type((word_index << bit_index_to_word_index_shift) + msb_bit_set(word));
                }

                word = _bits[--word_index];
            } while (word_index < word_count);
        }

        return bit_index_type(-1);
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
        return get_prev_set_bit_index<true>(bit_count);
    }

    /// @brief Checks if bit on provided index is set
    /// @param bit_index - index of the bit to check
    /// @return true if the bit on provided index is set
    bool is_bit_set(bit_index_type bit_index) const
    {
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_count, "Invalid bit index", false);

        const word_type bit_mask = word_type(1) << (static_cast<uint32>(bit_index) & word_bit_index_mask);
        return _bits[static_cast<uint32>(bit_index) >> bit_index_to_word_index_shift] & bit_mask;
    }

    /// @brief Sets bit on provided index
    /// @param bit_index - index of the bit to set
    void set_bit(bit_index_type bit_index)
    {
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_count, "Invalid bit index");

        const word_type bit_mask = word_type(1) << (static_cast<uint32>(bit_index) & word_bit_index_mask);
        _bits[static_cast<uint32>(bit_index) >> bit_index_to_word_index_shift] |= bit_mask;
    }

    /// @brief Unsets bit on provided index
    /// @param bit_index - index of the bit to unset
    void unset_bit(bit_index_type bit_index)
    {
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_count, "Invalid bit index");

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
        DASSERT_RETX(static_cast<uint32>(bit_index) < bit_count, "Invalid bit index");

        const word_type bit_mask = word_type(1) << (static_cast<uint32>(bit_index) & word_bit_index_mask);
        _bits[static_cast<uint32>(bit_index) >> bit_index_to_word_index_shift] ^= bit_mask;
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

    bitfield operator&(const bitfield& rhs) const
    {
        bitfield result;
        for (uint32 i = 0; i < word_count; ++i)
        {
            result._bits[i] = _bits[i] & rhs._bits[i];
        }

        return result;
    }

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

namespace detail
{
    template<uint32 bit_count>
    using bits_to_uint_type =
        std::conditional_t<(bit_count <= 8), uint8,
        std::conditional_t<(bit_count <= 16), uint16,
        std::conditional_t<(bit_count <= 32), uint32,
        std::conditional_t<(bit_count <= 64), uint64, void>>>>;

}; // end of namespace detail


template<typename T, T count_value>
COID_REQUIRES((std::is_enum_v<T>))
using enum_bitfield = bitfield<static_cast<uint32>(count_value), detail::bits_to_uint_type<static_cast<uint32>(count_value)>, T>;


COID_NAMESPACE_END