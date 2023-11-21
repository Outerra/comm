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
#include "../commassert.h"
#include "ref_intrusive_base.h"
#include "ref_default_policy_trait.h"

namespace coid
{

/// @brief Shared owner of an object that holds the pointer to its own counting policy
/// @tparam Type - type of the owned object, must derive from ref_intrusive_base
/// @note The count is in the policy, as it is for ref_shared; what is different here is that the
///  object holds the policy pointer, in its ref_intrusive_base. So a raw pointer can be turned back
///  into a counted reference, and two refs reached by different routes share one count.
/// @note Thread safety: the reference count is atomic, a ref instance is not. Two threads each
///  holding their own ref to one object is safe; two threads touching the same ref instance while
///  either mutates it is not, and no memory ordering can make it so.
/// @note See doc/refs.html
template <typename Type>
class ref_intrusive
{
    template<typename> friend class ref_intrusive; // make all template instances friends
public: // methods only
    COIDNEWDELETE(ref_intrusive);

    /// @brief Creates an empty ref
    ref_intrusive()
        : _object_ptr(nullptr) 
    {}

    /// @brief Creates an empty ref, so that nullptr can be passed where a ref is expected
    ref_intrusive(nullptr_t)
        : _object_ptr(nullptr) 
    {}

    /// @brief Adopts an object, attaching a policy only if it does not have one yet
    /// @tparam BaseOrDerivedType - the concrete type of the object, Type itself or one derived from it
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own, may be null
    /// @param policy_arguments - forwarded to the policy's create() when one is attached
    /// @note If the object already has a policy this joins its count, which is how a raw pointer -
    ///  this included - is turned back into a counted reference. See doc/refs.html,
    ///  "Returning an iref from this".
    /// @note Converts upwards only: a pointer to a base of Type is rejected rather than cast down on
    ///  trust. The policy is looked up for BaseOrDerivedType, so pass the concrete type.
    /// @note Explicit, so that a raw pointer never becomes an owner silently at a call boundary. See
    ///  doc/refs.html, "Adopting a raw pointer".
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    explicit ref_intrusive(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
        : _object_ptr(static_cast<Type*>(object_ptr))
    {
        if (_object_ptr)
        {
            ref_intrusive_base* base_ptr = base();

            if (base_ptr->_policy_ptr == nullptr)
            {
                base_ptr->_policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
            }

            base_ptr->_policy_ptr->_counter.increase_strong_counter();
        }
    }

    /// @brief Shares the object of rhs, adding a reference
    /// @param rhs - the ref to share with
    /// @note Cannot be folded into the templated overload: a template is never a copy constructor,
    ///  so removing this one lets the compiler declare an implicit one that copies _object_ptr
    ///  without touching the refcount, and that one wins for a same-type copy.
    ref_intrusive(const ref_intrusive& rhs)
        : _object_ptr(rhs.add_refcount_and_fetch_ptr_internal())
    {}

    /// @brief Shares the object of a ref to a derived type, adding a reference
    /// @tparam DerivedType - type derived from Type
    /// @param rhs - the ref to share with
    template<class DerivedType>
    COID_REQUIRES((std::is_convertible_v<DerivedType*, Type*>))
    ref_intrusive(const ref_intrusive<DerivedType>& rhs)
        : _object_ptr(static_cast<Type*>(rhs.add_refcount_and_fetch_ptr_internal()))
    {
    }


    /// @brief Takes over the reference of rhs, of this type or a derived one, without counting
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @param rhs - the ref to take from; left empty
    /// @note noexcept because it only steals pointers - it never releases anything, so it cannot
    ///  reach the pooling path that can throw. Keep it that way: a vector relocates with
    ///  move-if-noexcept, so losing this would make it copy refs instead of moving them.
    template<class BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    ref_intrusive(ref_intrusive<BaseOrDerivedType>&& rhs) noexcept
    {
        _object_ptr = static_cast<Type*>(rhs.get());
        rhs._object_ptr = nullptr;
    }


    /// @brief Releases what this ref holds and adopts an object, attaching a policy only if it has none
    /// @tparam BaseOrDerivedType - the concrete type of the object, Type itself or one derived from it
    /// @param rhs - object to own, may be null
    /// @return this ref
    /// @note Implicit, unlike the constructor, because the target's type is visible at its
    ///  declaration.
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    ref_intrusive& operator=(BaseOrDerivedType* rhs)
    {
        if (_object_ptr == rhs)
        {
            return *this;
        }

        release();

        if (rhs != nullptr)
        {
            ref_intrusive_base* rhs_base = base_of(rhs);

            if (rhs_base->_policy_ptr == nullptr)
            {
                rhs_base->_policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(rhs));
            }

            _object_ptr = static_cast<Type*>(rhs);
            base()->_policy_ptr->_counter.increase_strong_counter();
        }
        return *this;
    }

    /// @brief Releases what this ref holds and shares the object of rhs
    /// @param rhs - the ref to share with
    /// @return this ref
    /// @note Cannot be folded into the templated overload: a template is never a copy assignment
    ///  operator, so removing this one lets the compiler declare an implicit one that assigns the
    ///  members without releasing the old object or counting the new one.
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

    /// @brief Releases what this ref holds and shares the object of a ref to a derived type
    /// @tparam DerivedType - type derived from Type
    /// @param rhs - the ref to share with
    /// @return this ref
    template<class DerivedType>
    COID_REQUIRES((std::is_convertible_v<DerivedType*, Type*>))
    ref_intrusive& operator= (const ref_intrusive<DerivedType>& rhs)
    {
        release();
        if (rhs.is_set())
        {
            _object_ptr = static_cast<Type*>(rhs.add_refcount_and_fetch_ptr_internal());
        }

        return *this;
    }


    /// @brief Releases what this ref holds and takes over the reference of rhs
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @param rhs - the ref to take from; left empty
    /// @return this ref
    /// @note Do not route this through std::forward to the injected class name: for a differently
    ///  typed rhs that names ref_intrusive<Type> and materialises a temporary through the COPY
    ///  constructor, leaving the source set and the count one too high.
    /// @note Takes the reference over before releasing its own, which keeps self-move and a move
    ///  between two refs holding the same object both correct.
    /// @note Not noexcept, unlike the move constructor: releasing can reach the pooling path, which
    ///  allocates. See doc/refs.html, "Pooling and recycling".
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    ref_intrusive& operator= (ref_intrusive<BaseOrDerivedType>&& rhs)
    {
        if constexpr (std::is_same_v<Type, BaseOrDerivedType>)
        {
            if (this == &rhs)
            {
                return *this;
            }
        }

        Type* taken_object_ptr = static_cast<Type*>(rhs._object_ptr);
        rhs._object_ptr = nullptr;

        release();
        _object_ptr = taken_object_ptr;

        return *this;
    }

    /// @brief Equality with another ref, of this type or of a type derived from it
    /// @note Converts before comparing, so a base ref and a derived ref holding one object compare
    ///  equal although their stored addresses differ under multiple inheritance.
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    friend bool operator==(const ref_intrusive& lhs, const ref_intrusive<BaseOrDerivedType>& rhs)
    {
        return lhs.get() == static_cast<Type*>(rhs.get());
    }

    /// @brief Comparison against a raw pointer to the object
    /// @note Non-owning: nothing is adopted and no count moves. A derived pointer converts, and
    ///  so adjusts, on the way in.
    /// @note No operator!= anywhere in the refs - C++20 synthesizes it and the reversed operand
    ///  order from this one declaration, and declaring != would suppress both (MSVC C7692).
    friend bool operator==(const ref_intrusive& lhs, const Type* rhs) { return lhs._object_ptr == rhs; }

    /// @brief Comparison against nullptr, so a ref reads like the pointer it stands for
    friend bool operator==(const ref_intrusive& lhs, nullptr_t) { return lhs._object_ptr == nullptr; }

    /// @brief Orders two refs by the address of the object they point at
    /// @note A total but arbitrary order, for keying a map or sorting a container - addresses
    ///  differ from run to run, so never for reproducible output.
    /// @note Same type only: C++20 synthesizes a reversed == but not a reversed <, so a cross-type
    ///  ordering would work in one direction only.
    friend bool operator<(const ref_intrusive& lhs, const ref_intrusive& rhs)
    {
        return lhs.get() < rhs.get();
    }

    /// @brief Drops this ref's reference - see release()
    ~ref_intrusive()
    {
        release();
    }

    /// @brief Drops this ref's reference and empties it; the last reference destroys the object
    ///  through its policy
    void release()
    {
        if (_object_ptr != nullptr)
        {
            ref_intrusive_base* base_ptr = base();
            if (base_ptr->_policy_ptr->_counter.decrease_strong_counter())
            {
                base_ptr->_policy_ptr->on_destroy();
            }
            _object_ptr = nullptr;
        }
    }

    /// @brief Creates the object through the policy default_ref_policy_trait selects for BaseOrDerivedType
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param policy_arguments - forwarded to the policy's create(), for example the pool to use
    /// @note Releases whatever this ref held first, like every create() overload.
    template<typename BaseOrDerivedType = Type, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*> && std::is_base_of_v<ref_intrusive_base, Type>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<BaseOrDerivedType*>(policy_ptr->get_original_ptr()));
        policy_ptr->_counter.increase_strong_counter();
        base()->_policy_ptr = policy_ptr;
    }

    /// @brief Creates the object through a named policy
    /// @tparam Policy - a policy for Type, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param policy_arguments - forwarded to Policy::create()
    /// @note Releases whatever this ref held first, like every create() overload.
    /// @note The original pointer is cast back through Policy::element_type, so a base subobject
    ///  is adjusted correctly.
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy_for<Policy, Type>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        Policy* policy_ptr = Policy::create(std::forward<PolicyArguments>(policy_arguments)...);
        _object_ptr = static_cast<typename Policy::element_type*>(policy_ptr->get_original_ptr());
        policy_ptr->_counter.increase_strong_counter();
        base()->_policy_ptr = static_cast<ref_policy_base*>(policy_ptr);
    }

    /// @brief Creates an object of a derived type through a named policy
    /// @tparam DerivedType - type derived from Type, the one the policy counts
    /// @tparam Policy - a policy for DerivedType, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param policy_arguments - forwarded to Policy::create()
    template<typename DerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::negation_v<std::is_same<Type, DerivedType>> && std::is_convertible_v<DerivedType*, Type*> && is_ref_policy_for<Policy, DerivedType>))
    void create(PolicyArguments&&... policy_arguments)
    {
        release();

        Policy* policy_ptr = Policy::create(std::forward<PolicyArguments>(policy_arguments)...);
        _object_ptr = static_cast<typename Policy::element_type*>(policy_ptr->get_original_ptr());
        policy_ptr->_counter.increase_strong_counter();
        base()->_policy_ptr = static_cast<ref_policy_base*>(policy_ptr);
    }

    /// @brief Adopts an object, attaching a policy only if it does not have one yet
    /// @tparam BaseOrDerivedType - the concrete type of the object, Type itself or one derived from it
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own, must not be null
    /// @param policy_arguments - forwarded to the policy's create() when one is attached
    /// @note Releases whatever this ref held first, like every create() overload.
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        DASSERT_RET(object_ptr != nullptr);

        // the caller already handed us a typed pointer, so convert from that rather than from the
        // policy's void* - an already owned object may well have been created through a policy for
        // a type more derived than BaseOrDerivedType, whose original pointer needs a different
        // adjustment than the one the cast here would apply
        _object_ptr = static_cast<Type*>(object_ptr);

        if (base_of(object_ptr)->_policy_ptr == nullptr)
        {
            base()->_policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        }

        base()->_policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Adopts an object that has no policy yet, with a named policy
    /// @tparam Policy - a policy for Type, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own, must not be null
    /// @param policy_arguments - forwarded to Policy::create()
    /// @note The object must not already have a policy - asserted in debug.
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy_for<Policy, Type>))
    void create(Type* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        DASSERT_RET(object_ptr != nullptr);
        DASSERT_RET(base_of(object_ptr)->_policy_ptr == nullptr);

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;   // already typed, no need to go through the policy's void*
        policy_ptr->_counter.increase_strong_counter();
        base()->_policy_ptr = policy_ptr;
    }

    /// @brief Adopts an object of a derived type that has no policy yet, with a named policy
    /// @tparam DerivedType - type derived from Type, the one the policy counts
    /// @tparam Policy - a policy for DerivedType, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own
    /// @param policy_arguments - forwarded to Policy::create()
    /// @note The object must not already have a policy.
    template<typename DerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::negation_v<std::is_same<Type, DerivedType>> && std::is_convertible_v<DerivedType*, Type*> && is_ref_policy_for<Policy, DerivedType>))
    void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        release();

        ref_policy_base* policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(object_ptr);   // already typed, no need to go through the policy's void*
        policy_ptr->_counter.increase_strong_counter();
        base()->_policy_ptr = policy_ptr;
    }

    /// @brief Pointer to the referenced object, optionally as a more derived type
    /// @tparam BaseOrDerivedType - type to view the object as, Type (the default) or one derived from it
    /// @return object pointer, nullptr when this ref is empty
    /// @note Constrained on static-castability, so a derived type reached through a virtual base is
    ///  rejected. Asking for a type the object is not is the caller's responsibility; use
    ///  downcast() for a counted ref instead of a raw pointer.
    template<typename BaseOrDerivedType = Type>
    COID_REQUIRES((is_static_castable<Type*, BaseOrDerivedType*>))
    BaseOrDerivedType* get() const { return static_cast<BaseOrDerivedType*>(_object_ptr); }

    /// @brief The referenced object
    /// @note Asserts in debug that this ref is set; dereferencing an empty ref is undefined.
    Type& operator*() const { DASSERT(is_set()); return *_object_ptr; }
    /// @brief Member access on the referenced object
    /// @note Asserts in debug that this ref is set.
    Type* operator->() const { DASSERT(is_set()); return _object_ptr; }

    /// @brief True when this ref holds an object
    explicit operator bool() const { return is_set(); }

    /// @brief Whether this ref holds an object
    /// @return true when set
    bool is_set() const { return _object_ptr != nullptr; }

    /// @brief Whether this ref holds nothing
    /// @return true when empty
    bool is_empty() const { return _object_ptr == nullptr; }

    /// @brief Number of refs sharing the object
    /// @return the count, 0 when this ref is empty; a snapshot, since other threads may change it
    /// @note A set ref always holds a policy - every path that stores an object pointer attaches
    ///  one first. An object does exist without a policy while it waits in its pool, but its
    ///  refcount had to reach zero to get there, so no ref holds it then.
    uint32 get_strong_refcount() const
    {
        if (is_empty())
        {
            return 0;
        }

        DASSERT_RET(base()->_policy_ptr != nullptr, 0);
        return base()->_policy_ptr->_counter.get_strong_counter_value();
    }

    /// @brief A counted ref to the same object, as a more derived type
    /// @tparam DerivedType - type derived from Type
    /// @return ref of the derived type sharing this ref's count, empty when this ref is empty
    /// @note Asking for a type the object is not is the caller's responsibility.
    template<typename DerivedType>
    COID_REQUIRES((is_static_castable<Type*, DerivedType*>))
    ref_intrusive<DerivedType> downcast() const
    {
        ref_intrusive<DerivedType> result(static_cast<DerivedType*>(_object_ptr));
        return result;
    }

    /// @brief This ref as an rvalue, for passing it on - same as std::move(*this)
    /// @return rvalue reference to this ref
    ref_intrusive<Type>&& move()
    {
        return static_cast<ref_intrusive<Type>&&>(*this);
    }

protected: // methods only
    /// @brief Adds a reference if this ref is set, and returns the object pointer
    /// @note Shared by the copy constructors and copy assignments.
    Type* add_refcount_and_fetch_ptr_internal() const
    {
        if (_object_ptr)
        {
            base()->_policy_ptr->_counter.increase_strong_counter();
        }

        return _object_ptr;
    }

    /// @brief The single point where a reference-counted object is viewed as its ref base.
    /// @note Every member that touches the policy must go through here. The check lives in a
    ///  function body on purpose: it is instantiated only when the member is actually used, so
    ///  ref_intrusive<T> can be *declared* for an incomplete T (forward-declared members).
    ///  Do not lift this into a class-scope static_assert or a requires-clause that mentions only
    ///  Type - both are evaluated when the class is implicitly instantiated and would make every
    ///  forward-declared ref member a hard error (MSVC C2139).
    template<typename T>
    static ref_intrusive_base* base_of(T* object_ptr)
    {
        static_assert(sizeof(T) > 0,
            "ref_intrusive<Type>: Type must be complete here. Include its definition in this "
            "translation unit, or give the enclosing class an out-of-line destructor defined "
            "where Type is complete.");
        static_assert(std::is_base_of_v<ref_intrusive_base, T>,
            "ref_intrusive<Type> requires Type to be derived from coid::ref_intrusive_base");
        return static_cast<ref_intrusive_base*>(object_ptr);
    }

    /// @brief This ref's object viewed as its ref base
    ref_intrusive_base* base() const { return base_of(_object_ptr); }

protected: // members only
    /// @brief The object, as this ref sees it. Neither the policy nor the count is here: the object
    ///  holds the policy pointer in its ref_intrusive_base, which may sit at a non-zero offset, so
    ///  base() adjusts to reach it.
    Type* _object_ptr = nullptr;
};


}; // end of namespace coid
