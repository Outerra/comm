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

#include "ref_policy_base.h"
#include "ref_policy_shared.h"
#include "ref_default_policy.h"

#include "../metastream/metastream.h"

namespace coid
{

/// @brief Base class for counted references with external counter
/// @tparam Type - type of reference counted object
template<typename Type>
class ref_shared
{
   template<typename> friend class ref_shared;                         // make all template instances friends
public: //methods only
    /// @brief nullptr constructor
    explicit ref_shared(nullptr_t)
    {
    }

    /// @brief default constructor
    ref_shared() = default;

    /// @brief Constructor for ref_shared object from object pointer of type "Type"
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "Type" to be reference counted 
    template<typename Policy = ref_policy_shared<Type>, typename... PolicyArguments>
    explicit ref_shared(Type* object_ptr, PolicyArguments&&... policy_arguments)
    {
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
    }

    /// @brief Constructor for ref_shared object from object pointer of type DerivedType
    /// @param object_ptr - object pointer of type "DerivedType" to be reference counted 
    template<typename DerivedType, typename Policy = ref_policy_shared<Type>, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    explicit ref_shared(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
    }

    // toto potom refactornut a vymazat lebo to je podla mna hovadina
    template<typename Policy>
    COID_REQUIRES((std::is_base_of_v<ref_policy_base, Policy>))
    explicit ref_shared(Policy* existing_policy)
        : _policy_ptr(existing_policy)
        , _object_ptr(static_cast<Type*>(existing_policy->get_original_ptr()))
    {
        _policy_ptr->increase_strong_counter();
    }


    /// @brief  Move construcotr
    ref_shared(ref_shared&& rhs) noexcept
    {
        _policy_ptr = rhs._policy_ptr;
        _object_ptr = rhs._object_ptr;
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;
    }

    /// @brief  Copy constructor
    ref_shared(const ref_shared& rhs)
    {
        if (rhs.is_set())
        {
            const bool increased = rhs._policy_ptr->try_increase_strong_counter();

            if (increased)
            {
                _policy_ptr = rhs._policy_ptr;
                _object_ptr = rhs._object_ptr;
            }
        }
    }

    /// @brief  Copy constructor from derived type
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    ref_shared(const ref_shared<DerivedType>& rhs)
    {
        if (rhs.is_set())
        {
            const bool increased = rhs._policy_ptr->try_increase_strong_counter();

            if (increased)
            {
                _policy_ptr = rhs._policy_ptr;
                _object_ptr = static_cast<Type*>(rhs._object_ptr);
            }
        }
    }

    /// @brief Assignment operator
    const ref_shared& operator=(const ref_shared& rhs)
    {
        if (this != &rhs)
        {
            release();

            if (rhs.is_set())
            {
                const bool increased = rhs._policy_ptr->try_increase_strong_counter();
                if (increased)
                {
                    _object_ptr = rhs._object_ptr;
                    _policy_ptr = rhs._policy_ptr;
                }
            }
        }

        return *this;
    }

    /// @brief Assignment operator for ref_shader<DerivedType>
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    const ref_shared& operator=(const ref_shared<DerivedType>& rhs)
    {
        release();

        if (rhs.is_set())
        {
            const bool increased = rhs._policy_ptr->try_increase_strong_counter();
            if (increased)
            {
                _policy_ptr = rhs._policy_ptr;
                _object_ptr = static_cast<Type*>(rhs._object_ptr);
            }
        }

        return *this;
    }

    /// @brief Create method with default object construction determined by default policy
    /// @note default policy is specified by DEFAULT_REF_POLICY macro in ref_default_policy.h
    void create_default()
    {
        release();
        _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy<Type>::policy::create());
        _object_ptr = static_cast<Type*>(_policy_ptr->get_original_ptr());
    }

    /// @brief Create method with default object consturction determined by used policy
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    template<typename Policy = ref_policy_shared<Type>, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy<Policy>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(_policy_ptr->get_original_ptr());
    }

    /// @brief Create method form object pointer of type Type 
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "Type" to be reference counted 
    /// @node If ref_shared object is already set, it is released first
    template<typename Policy = ref_policy_shared<Type>, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy<Policy>))
    void create(Type* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
    }

    /// @brief Create method form object pointer of type DerivedType 
    /// @tparam DerivedType - type derived from Type
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "DerivedType" to be reference counted 
    /// @note DerivedType must be derived from Type
    /// @note If ref_shared object is already set, it is released first
    template<typename DerivedType, typename Policy = ref_policy_shared<Type>, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of<Type, DerivedType>::value&& is_ref_policy<Policy>))
    void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
    }


    /// @brief Downcasts this ref to the ref of derived type
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of<Type, DerivedType>::value))
    ref_shared<DerivedType> downcast() const
    {
        ref_shared<DerivedType> result(static_cast<DerivedType*>(_object_ptr), _policy_ptr);
        return result;
    }

    /// @brief desturctor
    ~ref_shared()
    {
        release();
    }

    /// @brief Get pointer of reference counted object
    /// @return object ptr
    Type* get() const
    {
        return _object_ptr;
    }

    Type& operator*() const
    {
        DASSERT(is_set());
        return *get();
    }

    Type* operator->() const
    {
        DASSERT(is_set());
        return get();
    }


   
    /// @brief operator bool for testing if this ref_shared object is set
    explicit operator bool() const { return is_set(); }

    /// @brief Takeover other ref object without refcount manipulations
    /// @param other_ref - other ref object
    /// @note This object is released first.
    void takeover(ref_shared<Type>& other_ref)
    {
        release();
        _policy_ptr = other_ref._policy_ptr;
        _object_ptr = other_ref._object_ptr;
        other_ref._policy_ptr = nullptr;
        other_ref._object_ptr = nullptr;
    }

    /// @brief  Releases reference counted object
    void release()
    {
        if (_policy_ptr != nullptr)
        {
            if (_policy_ptr->decrease_strong_counter())
            {
                _policy_ptr->on_destroy();
            }

            _policy_ptr = nullptr;
            _object_ptr = nullptr;
        }
    }

    /// @brief Helper for move
    /// @return rvalue of ThisType
    ref_shared<Type>&& move()
    {
        return static_cast<ref_shared<Type>&&>(*this);
    }

    /// @brief Tests if this ref_shared object is set 
    /// @return true if is set
    bool is_set() const { return _object_ptr != nullptr; }

    /// @brief Tests if this ref_shared object is empty
    /// @return true if is empty
    bool is_empty() const { return _object_ptr == nullptr; }

    /// @brief  Get current strong reference count
    /// @return strong reference count
    uint32 get_strong_refcount() const { return _policy_ptr->get_strong_counter_value(); }

    friend void swap(ref_shared<Type>& a, ref_shared<Type>& b) {
        std::swap(a._policy_ptr, b._policy_ptr);
        std::swap(a._object_ptr, b._object_ptr);
    }

    void swap(ref_shared<Type>& rhs) {
        std::swap(_policy_ptr, rhs._policy_ptr);
        std::swap(_object_ptr, rhs._object_ptr);
    }



    /// @brief Metastream operator
    /// @param m 
    /// @param s 
    /// @return 
    /*friend metastream& operator || (metastream& m, ref_shared<Type,Policy>& s)
    {
        if (m.stream_writing())
        {
            m.write_optional(s.get());
        }
        else if (m.stream_reading())
        {
            s.create(m.read_optional<Type>());
        }
        else {
            if (m.meta_decl_raw_pointer(
                typeid(s).name(),
                false,
                0,
                [](const void* a) -> const void* { return static_cast<const ref_shared<Type, Policy>*>(a)->_object_ptr; },
                [](const void* a) -> uints { return static_cast<const ref_shared<Type, Policy>*>(a)->is_empty() ? 0 : 1; },
                [](void* a, uints& i) -> void* { return static_cast<ref_shared<Type, Policy>*>(a)->_object_ptr; },
                [](const void* a, uints& i) -> const void* { return static_cast<const ref_shared<Type, Policy>*>(a)->_object_ptr; }
            ))
                m || *s._object_ptr;
        }
        return m;
    }*/

protected: //methods only

    /// @brief Constructor for ref_shared object from object pointer of type "Type" and existing policy
    /// @param object_ptr - object pointer of type "Type" to be reference counted 
    /// @param policy_ptr - pointer to existing policy
    /// @note Used only for downcasts
    explicit ref_shared(Type* object_ptr, ref_policy_base* policy_ptr)
    {
        bool increased = policy_ptr->try_increase_strong_counter();
        DASSERT(increased); // This should never happen because this constructor is only used in down cast method 
        // so the counter can never drop to zero while performing this action
        
        _policy_ptr = policy_ptr;
        _object_ptr = object_ptr;   
    }

protected: // members only
    ref_policy_base* _policy_ptr = nullptr;
    Type* _object_ptr = nullptr;
};
}; // end of namespace coid