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
    using this_type = ref_intrusive<Type>;
    template<typename> friend class ref_intrusive; // make all template instances friends
public: // methods only
    ref_intrusive() COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type>))
        : _object_ptr(nullptr) 
    {}

    ref_intrusive(nullptr_t) COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type>))
        : _object_ptr(nullptr) 
    {}

    /// @brief Constructor for ref_intrusive object from object pointer of base or derived type with policy from from default_ref_policy_trait<BaseOrDerivedType>
    /// @tparam BaseOrDerivedType - base or derived type
    /// @tparem PolicyArguments - argumes for default policy creation
    /// @param object_ptr - object pointer of base or derived type
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_base_of_v<Type, BaseOrDerivedType>))
        explicit ref_intrusive(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
        : _object_ptr(static_cast<Type*>(object_ptr))
    {
        if (_object_ptr)
        {
            ref_intrusive_base* base_ptr = static_cast<ref_intrusive_base*>(_object_ptr);

            if (base_ptr->_policy_ptr == nullptr)
            {
                base_ptr->_policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));

            }

            base_ptr->_policy_ptr->_counter.increase_strong_counter();
        }
    }

    /// @brief Copy constructor from base type
    /// @param  rhs - right hand side
    ref_intrusive(const ref_intrusive& rhs) COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type>))
        : _object_ptr(rhs.add_refcount_and_fetch_ptr_internal()) 
    {}

    /// @brief Copy constructor from derived type
   /// @tparam DerivedType - type derived form Type
    template<class DerivedType>
    COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type> && std::is_base_of_v<Type, DerivedType>))
    ref_intrusive(const ref_intrusive<DerivedType>& rhs)
        : _object_ptr(static_cast<Type*>(rhs.add_refcount_and_fetch_ptr_internal()))
    {
    }


    ref_intrusive(this_type&& rhs) noexcept COID_REQUIRES((std::is_base_of_v<ref_intrusive_base, Type>))
    {
        takeover_internal(std::forward<this_type>(rhs));
    }

    ~ref_intrusive()
    {
        release();
    }

   

    /// @brief Move constructor from derived type
    /// @tparam DerivedType - type derived form Type
    template<class DerivedType>
    COID_REQUIRES((std::is_base_of_v<Type, DerivedType>))
    ref_intrusive(ref_intrusive<DerivedType>&& rhs) : _object_ptr(0) 
    {
        takeover_internal(std::forward<this_type>(rhs));
    }

    /// @brief Assignment operator 
    /// @param rhs - right hand side term 
    /// @return this
    this_type& operator= (const this_type& rhs) 
    {
        Type* object_ptr = rhs.add_refcount_and_fetch_ptr_internal();
        release();
        _object_ptr = object_ptr;
        return *this;
    }

    friend bool operator== (const ref_intrusive<Type>& lhs, const ref_intrusive<Type>& rhs) 
    {
        return lhs._object_ptr == rhs._object_ptr;
    }

    void release() {
        if (_object_ptr)
        {
            ref_intrusive_base* base_ptr = static_cast<ref_intrusive_base*>(_object_ptr);
            if (base_ptr->_policy_ptr->_counter.decrease_strong_counter())
            {
                base_ptr->_policy_ptr->on_destroy();
            }
        }
        _object_ptr = 0;
    }

    void create(Type* object_ptr)
    {
        DASSERT_RET(object_ptr != _object_ptr);
        object_ptr->_policy_ptr->_counter.increase_strong_counter();
        release();
        _object_ptr = object_ptr;
    }


    /// @brief Move operator 
    /// @param rhs - right hand side term 
    /// @return this
    const this_type& operator= (this_type&& rhs) noexcept {
        takeover_internal(std::forward<this_type>(rhs));
        return *this;
    }

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
            static_cast<ref_intrusive_base*>(_object_ptr)->_policy_ptr->_counter.increase_strong_counter();
        }
        
        return _object_ptr;
    }

protected: // members only
    Type* _object_ptr = nullptr;
};


}; // end of namespace coid