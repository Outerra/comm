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

#include "ref_policy_base.h"

#include "../atomic/pool.h"

namespace coid
{

/// @brief Policy that creates the object with new and destroys it with delete
/// @tparam Type - type of the counted object
/// @note The default policy for every type - see default_ref_policy_trait.
/// @note The policy objects themselves are pooled; the counted object is not.
template<typename Type>
class ref_policy_simple : public ref_policy_base
{
    using this_type = ref_policy_simple<Type>;
    using policy_pool_type = coid::pool<this_type>;
public: // types only
    /// @brief The type get_original_ptr() actually points to.
    /// @note get_original_ptr() is declared as void*, so a caster that does not know this type
    ///  cannot adjust the pointer for a base subobject. Every ref that creates an object through
    ///  a policy casts the original pointer back to element_type* first, and only then lets the
    ///  implicit conversion to its own Type* do the adjustment.
    using element_type = Type;

public: // methods only
    COIDNEWDELETE(ref_policy_simple);

    ref_policy_simple(const ref_policy_simple&) = delete;
    ref_policy_simple& operator=(const ref_policy_simple&) = delete;

    /// @brief An idle policy, before create() gives it an object
    ref_policy_simple() = default;
    
    /// @brief Constructs a policy for an already constructed object
    /// @param original_ptr - object to count references for
    explicit ref_policy_simple(Type* original_ptr)
        : _original_ptr(original_ptr)
    {}
    

    /// @brief Creates a policy for an already constructed object
    /// @param object_ptr - object to count references for, must have come from new
    /// @return policy instance, taken from the policy pool when one is free
    static this_type* create(Type* object_ptr)
    {
        this_type* result = policy_pool_type::global().get_item();

        if (result == nullptr)
        {
            result = new this_type(object_ptr);
        }
        else 
        {
            result->_original_ptr = object_ptr;
        }

        return result;
    }

    /// @brief Creates both the object and its policy
    /// @return policy instance, with a default constructed Type behind it
    /// @note Default construction only - no create() in the refs forwards constructor arguments.
    ///  Build the object yourself and use the overload above to pass any.
    static this_type* create()
    {
        this_type* result = policy_pool_type::global().get_item();

        Type* object_ptr = new Type();
        
        if (result == nullptr)
        {
            result = new this_type(object_ptr);
        }

        result->_original_ptr = object_ptr;

        return result;
    }

    /// @brief Deletes the object and returns this policy to the policy pool
    void on_destroy() override
    {
        delete this->_original_ptr;
        this->_original_ptr = nullptr;

        this_type* t = this;
        policy_pool_type::global().release_item(t);
    }

    /// @brief The counted object's address, as stored - unadjusted
    /// @note Declared void*, so callers must cast it back through element_type first. See
    ///  doc/refs.html, "Pointer adjustment and multiple inheritance".
    void* get_original_ptr() override
    {
        return _original_ptr;
    }

protected: // methods only
protected: // members only
    /// @brief The counted object. Deleted by on_destroy().
    Type* _original_ptr;
};


}; // end of namespace coid