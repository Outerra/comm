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

#include <atomic>

namespace coid
{

/// @brief The strong and weak reference counts shared by every policy
/// @note Held by value inside ref_policy_base, so this layout reaches every counted object.
/// @note For the ordering the increments and decrements use, and why, see doc/refs.html,
///  "Thread safety".
class ref_counter
{
public: // methods only
    /// @brief Starts with no references
    ref_counter() = default;

    ///// @brief Creates counter from the global pool
    ///// @param instance - reference to instance pointer
    //static void create(ref_counter*& instance)
    //{
    //    if (instance == nullptr)
    //    {
    //        instance = SINGLETON(coid::pool<ref_counter>).get_item();
    //    }
    //    else 
    //    {
    //        DASSERT(0); // this shouldn't happen
    //    }
    //}
    ///// @brief Returns the counter back to global the pool and clear the counter variable
    ///// @param instance - reference to instance pointer
    //static void destroy(ref_counter*& instance)
    //{
    //    if (instance != nullptr)
    //    {
    //        SINGLETON(coid::pool<ref_counter>).release_item(instance);
    //    }
    //}

    /// @brief Adds one strong reference
    /// @note Relaxed is enough: the caller already holds a reference, so the object cannot be
    ///  destroyed underneath it and no other memory needs to be ordered against this.
    void increase_strong_counter()
    {
        _strong_counter.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Adds one strong reference unless the count is already zero
    /// @return true if the reference was added, false if the count was zero
    /// @note Acquire on success: unlike the plain increase this may be called without already
    ///  holding a reference, so it has to synchronise with whatever the previous owners wrote
    ///  before the object can be touched.
    /// @note Exists for the eventual ref_weak::lock(). Its one caller today is ref_shared's
    ///  downcast helper, where the count provably cannot be zero. It is not a way to make a racing
    ///  copy safe - see doc/refs.html, "Thread safety".
    bool try_increase_strong_counter()
    {
        uint32 current_value = _strong_counter.load(std::memory_order_relaxed);
        while (true)
        {
            if (current_value == 0)
            {
                return false;
            }

            if (_strong_counter.compare_exchange_weak(current_value, current_value + 1,
                std::memory_order_acquire, std::memory_order_relaxed))
            {
                return true;
            }
        }

        return false;
    }

    /// @brief Drops one strong reference
    /// @return true when this was the last one, so the caller must destroy the object
    /// @note Release on the decrement, acquire before reporting the last one. Every thread that
    ///  drops a reference publishes what it did to the object; the one thread that sees the count
    ///  reach zero then acquires all of it, so the destructor cannot race with writes made through
    ///  the references that were just given up. Relaxed here is invisible on x86, whose stores are
    ///  already ordered, but lets the destructor run against stale data on a weakly ordered CPU.
    bool decrease_strong_counter()
    {
        if (_strong_counter.fetch_sub(1, std::memory_order_release) == 1)
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            return true;
        }

        return false;
    }

    /// @brief Current strong reference count
    /// @return the count; a snapshot, since other threads may change it
    uint32 get_strong_counter_value() const
    {
        return _strong_counter.load(std::memory_order_relaxed);
    }

    // ------------------------------------------------------------------------------------------
    // Weak counter. Nothing increments, decrements or reads it - there is no ref_weak - so the
    // methods below are commented out. The *member* stays live, because it is layout: ref_counter
    // sits inside every ref_policy_base and every intrusively counted object is compiled against
    // it, so removing it would be an ABI change now and adding it back a second one later. It
    // costs nothing to keep - measured, no policy changes size either way.
    //
    // For what implementing weak references would take, see doc/refs.html, "Weak references".
    // ------------------------------------------------------------------------------------------

    ///// @brief Adds one weak reference
    //void increase_weak_counter()
    //{
    //    _weak_counter.fetch_add(1, std::memory_order_relaxed);
    //}

    ///// @brief Drops one weak reference
    ///// @return true when this was the last one
    //bool decrease_weak_counter()
    //{
    //    return _weak_counter.fetch_sub(1, std::memory_order_relaxed) == 1;
    //}

    ///// @brief Current weak reference count
    ///// @return the count
    //uint32 get_weak_counter_value() const
    //{
    //    return _weak_counter.load(std::memory_order_relaxed);
    //}

protected: // methods only
    ref_counter(const ref_counter&) = delete;
    ref_counter& operator=(const ref_counter&) = delete;
protected: // members only
    /// @brief Number of refs holding the object. The object is destroyed when it reaches zero.
    std::atomic<uint32> _strong_counter = 0;
    /// @brief Number of weak references. Present but unused - see the note above.
    std::atomic<uint32> _weak_counter = 0;
};


}; // end of namespace coid