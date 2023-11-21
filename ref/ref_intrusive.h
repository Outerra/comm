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
#include "../commassert.h"
#include "ref_intrusive_base.h"
#include "ref_default_policy_trait.h"

namespace coid
{

template <typename Type>
class ref_intrusive
{
    template<typename> friend class ref_intrusive; // make all template instances friends
public: // methods only
    ref_intrusive()
        : _object_ptr(nullptr) 
    {}

    ref_intrusive(nullptr_t)
        : _object_ptr(nullptr) 
    {}

    /// @brief Constructor for ref_intrusive object from object pointer of base or derived type with policy from from default_ref_policy_trait<BaseOrDerivedType>
    /// @tparam BaseOrDerivedType - base or derived type
    /// @tparem PolicyArguments - argumes for default policy creation
    /// @param object_ptr - object pointer of base or derived type
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    ref_intrusive(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
        : _object_ptr(reinterpret_cast<Type*>(object_ptr))
    {
        if (_object_ptr)
        {
            ref_intrusive_base* base_ptr = reinterpret_cast<ref_intrusive_base*>(_object_ptr);

            if (base_ptr->_policy_ptr == nullptr)
            {
                base_ptr->_policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));

            }

            base_ptr->_policy_ptr->_counter.increase_strong_counter();
        }
    }

    /// @brief Copy constructor from base type
    /// @param  rhs - right hand side
    ref_intrusive(const ref_intrusive& rhs)
        : _object_ptr(rhs.add_refcount_and_fetch_ptr_internal()) 
    {}

    /// @brief Copy constructor from derived type
   /// @tparam DerivedType - type derived form Type
    template<class DerivedType>
    ref_intrusive(const ref_intrusive<DerivedType>& rhs)
        : _object_ptr(static_cast<Type*>(rhs.add_refcount_and_fetch_ptr_internal()))
    {
    }


    //ref_intrusive(ref_intrusive&& rhs) noexcept COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type>))
    //{
    //    _object_ptr = static_cast<Type*>(rhs.get()); 

    //    takeover_internal(std::forward<ref_intrusive>(rhs));
    //}

    /// @brief Move constructor from base or derived type
    /// @tparam DerivedType - type derived form Type
    template<class BaseOrDerivedType>
    //COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type> && std::is_base_of_v<Type, BaseOrDerivedType>))
    ref_intrusive(ref_intrusive<BaseOrDerivedType>&& rhs) noexcept
    {
        _object_ptr = static_cast<Type*>(rhs.get());
        rhs._object_ptr = nullptr;
    }


    /// @brief Assignment operator from base or derived class object pointer
    /// @node default policy of BaseOrDerived class will be used if given object has no policy set
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    ref_intrusive& operator=(BaseOrDerivedType* rhs) COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type>))
    {
        if (_object_ptr == rhs)
        {
            return *this;
        }

        release();

        if (rhs != nullptr)
        {
            if (rhs->_policy_ptr == nullptr)
            {
                rhs->_policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(rhs));
            }

            _object_ptr = static_cast<Type*>(rhs);
            _object_ptr->_policy_ptr = rhs->_policy_ptr;
            _object_ptr->_policy_ptr->_counter.increase_strong_counter();
        }
        return *this;
    }

    /// @brief Assignment operator from base class
    /// @param rhs - right hand side term 
    ref_intrusive& operator= (const ref_intrusive& rhs) 
    {
        if (this != &rhs)
        {
            release();

            if (rhs.is_set())
            {
                _object_ptr = rhs.add_refcount_and_fetch_ptr_internal();
            }
        }

        return *this;
    }

    /// @brief Assignment operator from derived class
    /// @param rhs - right hand side term    
    template<class DerivedType>
    ref_intrusive& operator= (const ref_intrusive<DerivedType>& rhs)
    {
        release();
        if (rhs.is_set())
        {
            _object_ptr = static_cast<Type*>(rhs.add_refcount_and_fetch_ptr_internal());
        }

        return *this;
    }


    /// @brief Move operator from base or derived class
    /// @param rhs - right hand side term 
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    const ref_intrusive& operator= (ref_intrusive<BaseOrDerivedType>&& rhs) noexcept 
    {
        takeover_internal(std::forward<ref_intrusive>(rhs));
        return *this;
    }

    /// @brief Equality operator
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    friend bool operator==(const ref_intrusive& lhs, const ref_intrusive<BaseOrDerivedType>& rhs)
    {
        return lhs.get() == static_cast<Type*>(rhs.get());
    }

    /// @brief Destructor
    ~ref_intrusive()
    {
        release();
    }

    /// @brief  Releases reference counted object
    void release()
    {
        if (_object_ptr != nullptr)
        {
            ref_intrusive_base* base_ptr = reinterpret_cast<ref_intrusive_base*>(_object_ptr);
            if (base_ptr->_policy_ptr->_counter.decrease_strong_counter())
            {
                base_ptr->_policy_ptr->on_destroy();
            }
            _object_ptr = nullptr;
        }
    }

    /// @brief Create method with object construction determined by default policy trait
   /// @note for more info about default policy trait see ref_default_policy_trait.h
    template<typename BaseOrDerivedType = Type, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType> && std::is_base_of_v<ref_intrusive_base, Type>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<BaseOrDerivedType*>(policy_ptr->get_original_ptr()));
        policy_ptr->_counter.increase_strong_counter();
        _object_ptr->_policy_ptr = policy_ptr;
    }

    /// @brief Create method with Type object construction determined by used policy
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy<Policy>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(Policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(policy_ptr->get_original_ptr());
        policy_ptr->_counter.increase_strong_counter();
        _object_ptr->_policy_ptr = policy_ptr;
    }

    /// @brief Create method with DerivedType object construction determined by used policy
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    template<typename DerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::negation_v<std::is_same<Type, DerivedType>> && std::is_base_of_v<Type, DerivedType> && is_ref_policy<Policy>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(Policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<DerivedType*>(policy_ptr->get_original_ptr()));
        policy_ptr->_counter.increase_strong_counter();
        _object_ptr->_policy_ptr = policy_ptr;
    }

    /// @brief Create method form BaseOrDerivedType object pointer with policy from default_ref_policy_trait<DerivedType> or use policy from intrusive base if exists
    /// @tparam DerivedType - type derived from Type
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "DerivedType" to be reference counted 
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
    void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        DASSERT_RET(object_ptr != nullptr);

        if (object_ptr->_policy_ptr == nullptr)
        {
            ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
            _object_ptr = static_cast<Type*>(static_cast<BaseOrDerivedType*>(policy_ptr->get_original_ptr()));
            _object_ptr->_policy_ptr = policy_ptr;
        }
        else 
        {
            _object_ptr = static_cast<Type*>(static_cast<BaseOrDerivedType*>(object_ptr->_policy_ptr->get_original_ptr()));
        }

        _object_ptr->_policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Create method form object pointer of type Type 
    /// @tparam Policy - reference counting policy(must be base of ref_policy_base)
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "Type" to be reference counted 
    /// @note Intrusive base policy ptr should be nullptr
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy<Policy>))
    void create(Type* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        DASSERT_RET(object_ptr != nullptr);
        DASSERT_RET(object_ptr->_policy_ptr == nullptr);

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<Type*>(policy_ptr->get_original_ptr()));
        policy_ptr->_counter.increase_strong_counter();
        _object_ptr->_policy_ptr = policy_ptr;
    }

    /// @brief Create method form object pointer of type DerivedType 
    /// @tparam DerivedType - type derived from Type
    /// @tparam ...PolicyArguments  - variadic arguments for policy creation function
    /// @param object_ptr - object pointer of type "DerivedType" to be reference counted 
    /// @note Intrusive base policy ptr should be nullptr
    template<typename DerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::negation_v<std::is_same<Type, DerivedType>>&& std::is_base_of_v<Type, DerivedType>&& is_ref_policy<Policy>))
    void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<DerivedType*>(policy_ptr->get_original_ptr()));
        policy_ptr->_counter.increase_strong_counter();
        _object_ptr->_policy_ptr = policy_ptr;
    }

    /// @brief Get pointer of reference counted object
    /// @return object ptr
    Type* get() const { return _object_ptr; }
    Type& operator*() const { DASSERT(is_set()); return *_object_ptr; }
    Type* operator->() const { DASSERT(is_set()); return _object_ptr; }

    /// @brief operator bool for testing if this ref_intrusive object is set
    explicit operator bool() const { return is_set(); }

    /// @brief Tests if this ref_intrusive object is set 
    /// @return true if is set
    bool is_set() const { return _object_ptr != nullptr; }

    /// @brief Tests if this ref_intrusive object is empty
    /// @return true if is empty
    bool is_empty() const { return _object_ptr == nullptr; }

    /// @brief  Get current reference count
    /// @return current count of references
    uint32 get_strong_refcount() const { return _object_ptr->_policy_ptr->_counter.get_strong_counter_value(); }

    /// @brief Downcasts this ref to the ref of derived type
    /// @tparam DerivedType - type derived from Type
    template<typename DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    ref_intrusive<DerivedType> downcast() const
    {
        ref_intrusive<DerivedType> result(static_cast<DerivedType*>(_object_ptr));
        return result;
    }

    /// @brief Helper for move
    /// @return rvalue of ThisType
    ref_intrusive<Type>&& move()
    {
        return static_cast<ref_intrusive<Type>&&>(*this);
    }

protected: // methods only
    /// @brief Discard ref data without decrementing refcount
    void discard_internal() { _object_ptr = 0; }

    /// @brief Helper for move constructors
    /// @tparam DerivedType - type derived form Type
    /// @param rhs - right hand side assignment expresion
    template<class DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    void takeover_internal(ref_intrusive<DerivedType>&& rhs) {
        if (_object_ptr == static_cast<Type*>(rhs.get())) 
        {
            rhs.release();
            return;
        }

        release();
        _object_ptr = static_cast<Type*>(rhs.get());
        rhs.discard_internal();
    }

    /// @brief Adds refcount if _object_ptr is not null and returns _object_ptr
    /// @note helper for copy constructor 
    Type* add_refcount_and_fetch_ptr_internal() const
    {
        if (_object_ptr)
        {
            reinterpret_cast<ref_intrusive_base*>(_object_ptr)->_policy_ptr->_counter.increase_strong_counter();
        }
        
        return _object_ptr;
    }

protected: // members only
    Type* _object_ptr = nullptr;
};


}; // end of namespace coid
