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


#include "ref_policy_simple.h"

namespace coid
{

/// @brief Pooled policy is using getting object from pool on create and returing the object back to the pool od destroy
/// @tparam Type - type of reference counted object
/// @note policy object is pooled, non copyable, non movable
/// @note The pool can by specified by additional ref create function argument. The type must by coid::pool
/// @note If the pool is not specified, the global pool is used.
template <typename Type>
class ref_policy_pooled : public ref_policy_simple<Type>
{
    using this_type = ref_policy_pooled<Type>;
    using policy_pool_type = coid::pool<ref_policy_pooled<Type>>;
    using type_pool = coid::pool<Type>;
public: // methods only
    ref_policy_pooled(const ref_policy_pooled&) = delete;
    ref_policy_pooled& operator=(const ref_policy_pooled&) = delete;
    ref_policy_pooled() = default;

    /// @brief Create policy instance along with Type object instance form default global pool
    /// @return policy instance pointer
    static ref_policy_pooled<Type>* create()
    {
        return create(&type_pool::global());
    }

    /// @brief Staic create method of the policy, the object is returned to the global pool
    /// @param object_ptr - the pointer for the object the policy is counting the references for
    /// @return policy instance pointer
    static ref_policy_pooled<Type>* create(Type* object_ptr)
    {
        return create(object_ptr, &type_pool::global());
    }

    /// @brief Creates policy instance along with Type object instance form pool passed in type_pool_ptr argument
    /// @param type_pool_ptr - pointer to pool from which the object instance will be created
    /// @return policy instance pointer
    /// @note User is responsible for life time of the object pool!
    static ref_policy_pooled<Type>* create(coid::pool<Type>* type_pool_ptr)
    {
        Type* object_ptr = type_pool_ptr->create_item();
        return create(object_ptr, type_pool_ptr);
    }

    /// @brief Staic create method of the policy
    /// @param object_ptr - the pointer for the object the policy is counting the references for
    /// @param type_pool_ptr - pointer to pool of object of type Type where the object will be returned after deletition
    /// @return policy instance pointer
    /// @note User is responsible for life time of the object pool!
    static ref_policy_pooled<Type>* create(Type* object_ptr, coid::pool<Type>* type_pool_ptr)
    {
        ref_policy_pooled<Type>* result_ptr = policy_pool_type::global().create_item();
        result_ptr->_type_pool_ptr = type_pool_ptr;
        result_ptr->_original_ptr = object_ptr;
        return result_ptr;
    }

    /// @brief  Returns object to its object pool and policy to global policy pool
    virtual void on_destroy() override
    {
        _type_pool_ptr->release_item(this->_original_ptr);
        _type_pool_ptr = nullptr;

        this_type* t = this;
        policy_pool_type::global().release_item(t);
    }

protected: // methods only
protected: // members only
    type_pool* _type_pool_ptr = nullptr;
};

}; // end of namespace coid