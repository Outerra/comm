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


#include "ref_policy_simple.h"
#include "ref_intrusive_base.h"

namespace coid
{

/// @brief Policy that takes the object from a coid::pool and returns it there instead of deleting it
/// @tparam Type - type of the counted object
/// @note The pool is passed to create() as a coid::pool<Type>*; without one the global pool of Type
///  is used. The pool must outlive every object taken from it.
/// @note A pooled object is recycled, never destructed or reconstructed: it keeps its member values
///  between uses, and its constructor runs once, on first allocation.
/// @note The policy objects themselves are pooled too. Non copyable, non movable.
template <typename Type>
class ref_policy_pooled : public ref_policy_simple<Type>
{
    using this_type = ref_policy_pooled<Type>;
    using policy_pool_type = coid::pool<ref_policy_pooled<Type>>;
    using type_pool = coid::pool<Type>;
public: // methods only
    COIDNEWDELETE(ref_policy_pooled);

    ref_policy_pooled(const ref_policy_pooled&) = delete;
    ref_policy_pooled& operator=(const ref_policy_pooled&) = delete;
    /// @brief An idle policy, before create() gives it an object and a pool
    ref_policy_pooled() = default;

    /// @brief Takes an object from the global pool of Type, with a policy for it
    /// @return the policy, with the object behind it
    static ref_policy_pooled<Type>* create()
    {
        return create(&type_pool::global());
    }

    /// @brief Creates a policy for an existing object, which goes to the global pool of Type on release
    /// @param object_ptr - the object to count
    /// @return the policy
    static ref_policy_pooled<Type>* create(Type* object_ptr)
    {
        return create(object_ptr, &type_pool::global());
    }

    /// @brief Takes an object from the given pool, with a policy for it
    /// @param type_pool_ptr - pool to take the object from and return it to
    /// @return the policy, with the object behind it
    /// @note The pool must outlive the object.
    static ref_policy_pooled<Type>* create(coid::pool<Type>* type_pool_ptr)
    {
        Type* object_ptr = type_pool_ptr->create_item();
        return create(object_ptr, type_pool_ptr);
    }

    /// @brief Creates a policy for an existing object, which goes to the given pool on release
    /// @param object_ptr - the object to count
    /// @param type_pool_ptr - pool the object is returned to
    /// @return the policy
    /// @note The pool must outlive the object.
    static ref_policy_pooled<Type>* create(Type* object_ptr, coid::pool<Type>* type_pool_ptr)
    {
        ref_policy_pooled<Type>* result_ptr = policy_pool_type::global().create_item();
        result_ptr->_type_pool_ptr = type_pool_ptr;
        result_ptr->_original_ptr = object_ptr;
        return result_ptr;
    }

    /// @brief Returns the object to its pool and this policy to the policy pool
    /// @note The object is recycled, not destroyed, so an intrusively counted one first lets go of
    ///  its pointer to this policy - the policy is about to be handed to some other object.
    virtual void on_destroy() override
    {
        ref_intrusive_base::clear_policy_on_recycle(this->_original_ptr);

        _type_pool_ptr->release_item(this->_original_ptr);
        _type_pool_ptr = nullptr;

        this_type* t = this;
        policy_pool_type::global().release_item(t);
    }

protected: // methods only
protected: // members only
    /// @brief The pool the object came from and goes back to. Null while the policy is idle.
    type_pool* _type_pool_ptr = nullptr;
};

}; // end of namespace coid