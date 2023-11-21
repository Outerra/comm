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
 * Outerra s.r.o
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

#include "../commtypes.h"
#include "../atomic/pool.h"

#include <atomic>

namespace coid
{

/// @brief Base policy for reference counting.
/// @note ref_policy_base instances are non copiable, non movable
/// @note every derived ref_policy must contain static method for policy creation 
/// @note static this_type* create(void* object_ptr)
class ref_policy_base
{
    using this_type = ref_policy_base;
public: // methods only
    ref_policy_base(const ref_policy_base&) = delete;
    ref_policy_base(const ref_policy_base&&) = delete;
    const ref_policy_base& operator=(const ref_policy_base&) = delete;

    /// @brief Creates policy from the pool
    /// @param object_ptr - the pointer for the object the policy is counting the references for
    /// @return policy pointer
    static this_type* create(void* object_ptr)
    {
        return nullptr;
    }

    /// @brief Method called when refcount drops to 0 and the object and the policy are destoryed
    virtual void on_destroy() = 0;

    /// @brief Get pointer of counted object
    /// @return pointer of object
    virtual void* get_original_ptr() = 0;

    /// @brief Increase strong counter
    void increase_strong_counter()
    {
        _strong_counter.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Increases strong counter if possible (counter is not zero)
    /// @return true if counter was successfuly increased
    bool try_increase_strong_counter()
    {
        uint32 current_value = _strong_counter.load(std::memory_order_relaxed);
        while (true)
        {
            if (current_value == 0)
            {
                return false;
            }

            if (_strong_counter.compare_exchange_weak(current_value, current_value + 1, std::memory_order_relaxed))
            {
                return true;
            }
        }

        return false;
    }

    /// @brief Decreses strong counter
    /// @return true if counter decreased to zero
    bool decrease_strong_counter()
    {
        return _strong_counter.fetch_sub(1, std::memory_order_relaxed) == 1;
    }

    /// @brief Get current counter value
    /// @return counter value
    uint32 get_strong_counter_value() const 
    {
        return _strong_counter.load(std::memory_order_relaxed);
    }

    /// @brief Increases weak counter
    void increase_weak_counter()
    {
        _weak_counter.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Decreses weak counter
    /// @return true if counter decreased to zero
    bool decrease_weak_counter()
    {
        return _weak_counter.fetch_sub(1, std::memory_order_relaxed) == 1;
    }

    /// @brief Get current counter value
    /// @return counter value
    uint32 get_weak_counter_value() const
    {
        return _weak_counter.load(std::memory_order_relaxed);
    }

    /// @brief Default constructor
    ref_policy_base() = default;

protected: // methods only
protected: // members only
    std::atomic<uint32> _strong_counter = 0;
    std::atomic<uint32> _weak_counter = 0;
};

#ifdef COID_CONCEPTS
template<typename Derived> 
concept is_ref_policy = std::is_base_of<ref_policy_base, Derived>::value;
#endif

}; // end of namespace coid