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
#include "ref_default_policy_trait.h"
#include "ref_unique.h"

#include "../metastream/metastream.h"

namespace coid
{

/// @brief Template class for strong reference counted object with policy for creation/destruction of the object.
/// @tparam Type - type of referenced counted object
template<typename Type>
class ref_shared
{
   template<typename> friend class ref_shared;      // make all template instances friends
public: //methods only
    /// @brief nullptr constructor
    explicit ref_shared(nullptr_t)
    {
    }

    /// @brief default constructor
    ref_shared() = default;


    /// @brief Constructor for ref_shared object from object pointer of type "Type" with policy from from default_ref_policy_trait<Type>
    /// @param object_ptr - object pointer of type "Type" to be reference counted 
    //template<typename... PolicyArguments>
    //explicit ref_shared(Type* object_ptr, PolicyArguments&&... policy_arguments)
    //{
    //    _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<Type>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
    //    _object_ptr = object_ptr;
    //    _policy_ptr->_counter.increase_strong_counter();
    //}

    /// @brief Constructor for ref_shared object from object pointer of base or derived type with policy from from default_ref_policy_trait<BaseOrDerivedType>
    /// @param object_ptr - object pointer of base or derived type
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    explicit ref_shared(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }

    ///// @brief Copy constructor from base
    ref_shared(const ref_shared& rhs) 
    {
        if (rhs.is_set())
        {
            const bool increased = rhs._policy_ptr->_counter.try_increase_strong_counter();

            if (increased)
            {
                _policy_ptr = rhs._policy_ptr;
                _object_ptr = rhs._object_ptr;
            }
        }
    }

    /// @brief Copy constructor from derived type
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    ref_shared(const ref_shared<DerivedType>& rhs)
    {
        if (rhs.is_set())
        {
            const bool increased = rhs._policy_ptr->_counter.try_increase_strong_counter();

            if (increased)
            {
                _policy_ptr = rhs._policy_ptr;
                _object_ptr = static_cast<Type*>(rhs._object_ptr);
            }
        }
    }

    /// @brief  Move constructor from other ref_shared of base or derived type
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
        ref_shared(ref_shared<BaseOrDerivedType>&& rhs) noexcept
    {
        _policy_ptr = rhs._policy_ptr;
        _object_ptr = rhs._object_ptr;
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;
    }

    /// @brief  Move constructor from ref_unique of base or derived type
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
        ref_shared(ref_unique<BaseOrDerivedType>&& rhs) noexcept
    {
        if (rhs._policy_ptr == nullptr)
        {
            _policy_ptr = static_cast<ref_policy_base*>(ref_policy_simple<BaseOrDerivedType>::create(rhs._object_ptr));
        }
        else
        {
            _policy_ptr = rhs._policy_ptr;
        }

        _object_ptr = rhs._object_ptr;
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;

        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Assignment operator from base type
    ref_shared& operator=(const ref_shared& rhs)
    {
        if (this != &rhs)
        {
            release();

            if (rhs.is_set())
            {
                const bool increased = rhs._policy_ptr->_counter.try_increase_strong_counter();
                if (increased)
                {
                    _object_ptr = rhs._object_ptr;
                    _policy_ptr = rhs._policy_ptr;
                }
            }
        }

        return *this;
    }

    /// @brief Assignment operator from derived type
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    ref_shared& operator=(const ref_shared<DerivedType>& rhs)
    {
        release();
        if (rhs.is_set())
        {
            const bool increased = rhs._policy_ptr->_counter.try_increase_strong_counter();
            if (increased)
            {
                _policy_ptr = rhs._policy_ptr;
                _object_ptr = static_cast<Type*>(rhs._object_ptr);
            }
        }
        return *this;
    }

    /// @brief Move assignment operator from base type
    ref_shared& operator=(ref_shared&& rhs)
    {
        if (this != &rhs)
        {
            release();

            if (rhs.is_set())
            {
                _object_ptr = rhs._object_ptr;
                _policy_ptr = rhs._policy_ptr;
                rhs._object_ptr = nullptr;
                rhs._policy_ptr = nullptr;
            }
        }

        return *this;
    }

    /// @brief Move assignment operator from derived type
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    ref_shared& operator=(ref_shared<DerivedType>&& rhs)
    {
        release();

        if (rhs.is_set())
        {
            _object_ptr = rhs._object_ptr;
            _policy_ptr = rhs._policy_ptr;
            rhs._object_ptr = nullptr;
            rhs._policy_ptr = nullptr;
        }

        return *this;
    }

    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    friend bool operator==(const ref_shared& lhs, const ref_shared<BaseOrDerivedType>& rhs)
    {
        return lhs.get() == static_cast<Type*>(rhs.get());
    }

    /// @brief Create method with object construction determined by default policy trait
    /// @note for more info about default policy trait see ref_default_policy_trait.h
    template<typename BaseOrDerivedType = Type, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<BaseOrDerivedType*>(_policy_ptr->get_original_ptr()));
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Create method with Type object construction determined by used policy
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy<Policy>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();
    
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(_policy_ptr->get_original_ptr());
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Create method with DerivedType object construction determined by used policy
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    template<typename DerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::negation_v<std::is_same<Type, DerivedType>> && std::is_base_of_v<Type, DerivedType> && is_ref_policy<Policy>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<DerivedType*>(_policy_ptr->get_original_ptr()));
        _policy_ptr->_counter.increase_strong_counter();
    }

    ///// @brief Create method form Type object pointer with policy from default_ref_policy_trait<Type>
    ///// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    ///// @param object_ptr - object pointer of type "Type" to be reference counted 
    //template<typename... PolicyArguments>
    //void create(Type* object_ptr, PolicyArguments&&... policy_arguments)
    //{
    //    release();
    //
    //    _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<Type>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
    //    _object_ptr = object_ptr;
    //    _policy_ptr->_counter.increase_strong_counter();
    //}

    /// @brief Create method form DerivedType object pointer with policy from default_ref_policy_trait<DerivedType>
    /// @tparam DerivedType - type derived from Type
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "DerivedType" to be reference counted 
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Create method form object pointer of type Type 
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "Type" to be reference counted 
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy<Policy>))
    void create(Type* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();
        
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Create method form object pointer of type DerivedType 
    /// @tparam DerivedType - type derived from Type
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "DerivedType" to be reference counted 
    template<typename DerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::negation_v<std::is_same<Type, DerivedType>>&& std::is_base_of_v<Type, DerivedType>&& is_ref_policy<Policy>))
    void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }


    /// @brief Downcasts this ref to the ref of derived type
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    ref_shared<DerivedType> downcast() const
    {
        ref_shared<DerivedType> result(static_cast<DerivedType*>(_policy_ptr->get_original_ptr()), _policy_ptr);
        return result;
    }

    /// @brief desturctor
    ~ref_shared()
    {
        release();
    }

    /// @brief Get pointer of reference counted object
    /// @return object ptr
    Type* get() const { return _object_ptr; }
    Type& operator*() const { DASSERT(is_set()); return *get(); }
    Type* operator->() const { DASSERT(is_set()); return get(); }

    /// @brief operator bool for testing if this ref_shared object is set
    explicit operator bool() const { return is_set(); }

    /// @brief  Releases reference counted object
    void release()
    {
        if (_policy_ptr != nullptr)
        {
            if (_policy_ptr->_counter.decrease_strong_counter())
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
    uint32 get_strong_refcount() const { return _policy_ptr->_counter.get_strong_counter_value(); }

    friend void swap(ref_shared<Type>& a, ref_shared<Type>& b) 
    {
        std::swap(a._policy_ptr, b._policy_ptr);
        std::swap(a._object_ptr, b._object_ptr);
    }
    
    void swap(ref_shared<Type>& rhs) 
    {
        std::swap(_policy_ptr, rhs._policy_ptr);
        std::swap(_object_ptr, rhs._object_ptr);
    }

protected: //methods only

    /// @brief Constructor for ref_shared object from object pointer of type "Type" and existing policy
    /// @param object_ptr - object pointer of type "Type" to be reference counted 
    /// @param policy_ptr - pointer to existing policy
    /// @note Used only for downcasts
    explicit ref_shared(Type* object_ptr, ref_policy_base* policy_ptr)
    {
        bool increased = policy_ptr->_counter.try_increase_strong_counter();
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