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

#include "ref_counter.h"
#include "ref_policy_base.h"

namespace coid
{

/// @brief Base class that makes a type usable with ref_intrusive
/// @note Holds the pointer to the policy counting the object; the count itself is in the policy.
///  Because the object knows its policy, a raw pointer to it can be turned back into a counted
///  reference - which is what sets ref_intrusive apart from ref_shared.
/// @note Costs every instance a vptr and a pointer, and makes the type polymorphic. See
///  doc/refs.html, "ref_intrusive".
class ref_intrusive_base
{
    template<typename Type> friend class ref_intrusive;          // make all template instances friends
public: // methods only
    COIDNEWDELETE(ref_intrusive_base);

    /// @brief Virtual, so that a policy can delete the object through this base
    virtual ~ref_intrusive_base()
    {
        _policy_ptr = nullptr;
    };

    /// @brief Drops one reference to this object
    /// @return true when the count reached zero, so the caller must dispose of the object
    /// @note Destroys nothing itself; ref_intrusive calls the policy's on_destroy() when it
    ///  returns true.
    bool decrease_strong_counter()
    {
        DASSERT_RET(_policy_ptr, false);
        return _policy_ptr->_counter.decrease_strong_counter();
    }

    /// @brief Adds one reference to this object
    void increase_strong_counter()
    {
        DASSERT_RET(_policy_ptr);
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Current strong reference count of this object
    /// @return the number of refs pointing at this object, 0 when no policy is attached
    /// @note Asks the object rather than a ref, so it needs only a valid pointer. The object must
    ///  be alive, which after the last release is true only for a pooled type - a simple
    ///  policy deletes it. See doc/refs.html, "Pooling and recycling".
    uint32 get_strong_refcount() const
    {
        return _policy_ptr != nullptr ? _policy_ptr->_counter.get_strong_counter_value() : 0;
    }

    /// @brief Whether a reference counting policy is currently attached to this object
    /// @note False before the first ref adopts the object and again after it is recycled, true for
    ///  exactly as long as it is owned. Not the same question as get_strong_refcount() == 0, which
    ///  is only a proxy for it.
    bool has_policy() const
    {
        return _policy_ptr != nullptr;
    }

    /// @brief Clears the policy back pointer of an object that is being recycled instead of destroyed
    /// @tparam Type - type of the recycled object, need not be intrusively counted
    /// @param object_ptr - the object on its way back to its pool
    /// @note A pooling policy keeps its object alive while handing itself back to the policy pool,
    ///  so a recycled object that kept its back pointer would share the refcount of whichever
    ///  object gets that policy next.
    /// @note No-op for types that do not derive from ref_intrusive_base.
    template<typename Type>
    static void clear_policy_on_recycle(Type* object_ptr)
    {
        if constexpr (std::is_base_of_v<ref_intrusive_base, Type>)
        {
            if (object_ptr != nullptr)
            {
                static_cast<ref_intrusive_base*>(object_ptr)->_policy_ptr = nullptr;
            }
        }
    }

public: // legacy methods
    /// @brief Adds a reference by hand, with no ref to account for it
    /// @deprecated Legacy, to be removed - hold an iref instead.
    void add_refcount_legacy() {
        increase_strong_counter();
    }

    /// @brief Drops a reference taken by add_refcount_legacy(); the last one destroys the object
    /// @deprecated Legacy, to be removed - hold an iref instead.
    void release_refcount_legacy()
    {
        const bool is_zeo = decrease_strong_counter();

        if (is_zeo)
        {
            _policy_ptr->on_destroy();
        }
    }

protected: // methods only
    /// @brief Only derived types construct it; the policy pointer starts null
    ref_intrusive_base() = default;
    ref_intrusive_base(const ref_intrusive_base&) = delete;
    ref_intrusive_base& operator=(const ref_intrusive_base&) = delete;
protected: // members only
    /// @brief The policy counting this object, null before the first ref adopts it and again once
    ///  a pooled object is recycled. See has_policy().
    ref_policy_base* _policy_ptr = nullptr;
};

}; // end of namespace coid