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
#include "ref_default_policy_trait.h"
#include "ref_unique.h"
#include "ref_intrusive_base.h"

namespace coid
{

/// @brief Shared owner of an object, counted by a policy that every ref to the object holds
/// @tparam Type - type of the owned object
/// @note The default choice for shared ownership; works with any type. See doc/refs.html,
///  "Choosing between them".
/// @note Thread safety: the reference count is atomic, a ref instance is not. Two threads each
///  holding their own ref to one object is safe; two threads touching the same ref instance while
///  either mutates it is not, and no memory ordering can make it so.
/// @note See doc/refs.html
template<typename Type>
class ref_shared
{
   template<typename> friend class ref_shared;      // make all template instances friends
public: //methods only
    COIDNEWDELETE(ref_shared);

    /// @brief Creates an empty ref, so that nullptr can be passed where a ref is expected
    ref_shared(nullptr_t)
    {
    }

    /// @brief Creates an empty ref
    ref_shared() = default;

    /// @brief Adopts an already constructed object, with the policy default_ref_policy_trait selects
    /// @tparam BaseOrDerivedType - the concrete type of the object, Type itself or one derived from it
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own
    /// @param policy_arguments - forwarded to the policy's create()
    /// @note Explicit, so that a raw pointer never becomes an owner silently at a call boundary.
    ///  Adopting one pointer into two ref_shareds gives it two independent policies and so a
    ///  double free - see doc/refs.html, "Adopting a raw pointer".
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    explicit ref_shared(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        assert_not_intrusive();
        _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Shares the object of rhs, adding a reference
    /// @param rhs - the ref to share with
    /// @note Cannot be folded into the templated overload: a template is never a copy constructor,
    ///  so removing this one lets the compiler declare an implicit one that copies the members
    ///  without touching the refcount, and that one wins for a same-type copy.
    /// @note A plain increment, not try_increase_strong_counter(): the source holds a reference for
    ///  the whole call, so the count cannot be at zero and the CAS could only fail spuriously.
    ref_shared(const ref_shared& rhs) 
    {
        if (rhs.is_set())
        {
            rhs._policy_ptr->_counter.increase_strong_counter();
            _policy_ptr = rhs._policy_ptr;
            _object_ptr = rhs._object_ptr;
        }
    }

    /// @brief Shares the object of a ref to a derived type, adding a reference
    /// @tparam DerivedType - type derived from Type
    /// @param rhs - the ref to share with
    template<typename DerivedType>
    COID_REQUIRES((std::is_convertible_v<DerivedType*, Type*>))
    ref_shared(const ref_shared<DerivedType>& rhs)
    {
        if (rhs.is_set())
        {
            rhs._policy_ptr->_counter.increase_strong_counter();
            _policy_ptr = rhs._policy_ptr;
            _object_ptr = static_cast<Type*>(rhs._object_ptr);
        }
    }

    /// @brief Takes over the reference of rhs, of this type or a derived one, without counting
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @param rhs - the ref to take from; left empty
    /// @note noexcept because it only steals pointers - it never releases anything, so it cannot
    ///  reach the pooling path that can throw. Keep it that way: a vector relocates with
    ///  move-if-noexcept, so losing this would make it copy refs instead of moving them.
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
        ref_shared(ref_shared<BaseOrDerivedType>&& rhs) noexcept
    {
        _policy_ptr = rhs._policy_ptr;
        _object_ptr = rhs._object_ptr;
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;
    }

    /// @brief Takes over the object of a ref_unique and starts counting it
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @param rhs - the ref_unique to take from; left empty
    /// @note Keeps the ref_unique's policy if it has one. Otherwise it attaches a ref_policy_simple,
    ///  which deletes the object on the last release - as the ref_unique would have.
    /// @note Not noexcept, unlike the move constructor above: creating that policy can allocate.
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
        ref_shared(ref_unique<BaseOrDerivedType>&& rhs)
    {
        assert_not_intrusive();

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

    /// @brief Releases what this ref holds and shares the object of rhs
    /// @param rhs - the ref to share with
    /// @return this ref
    /// @note Cannot be folded into the templated overload: a template is never a copy assignment
    ///  operator, so removing this one lets the compiler declare an implicit one that assigns the
    ///  members without releasing the old object or counting the new one.
    ref_shared& operator=(const ref_shared& rhs)
    {
        if (this != &rhs)
        {
            release();

            if (rhs.is_set())
            {
                rhs._policy_ptr->_counter.increase_strong_counter();
                _object_ptr = rhs._object_ptr;
                _policy_ptr = rhs._policy_ptr;
            }
        }

        return *this;
    }

    /// @brief Releases what this ref holds and shares the object of a ref to a derived type
    /// @tparam DerivedType - type derived from Type
    /// @param rhs - the ref to share with
    /// @return this ref
    template<typename DerivedType>
    COID_REQUIRES((std::is_convertible_v<DerivedType*, Type*>))
    ref_shared& operator=(const ref_shared<DerivedType>& rhs)
    {
        release();
        if (rhs.is_set())
        {
            rhs._policy_ptr->_counter.increase_strong_counter();
            _policy_ptr = rhs._policy_ptr;
            _object_ptr = static_cast<Type*>(rhs._object_ptr);
        }
        return *this;
    }

    /// @brief Releases what this ref holds and takes over the reference of rhs
    /// @param rhs - the ref to take from; left empty
    /// @return this ref
    /// @note Not strictly required - unlike the copy operations, a move assignment operator is not
    ///  implicitly declared here (the user-declared destructor suppresses it), so the templated
    ///  overload below would take over. Kept as the same-type fast path.
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

    /// @brief Releases what this ref holds and takes over the reference of a ref to a derived type
    /// @tparam DerivedType - type derived from Type
    /// @param rhs - the ref to take from; left empty
    /// @return this ref
    /// @note Takes rhs's reference before releasing its own, so a move between two refs holding the
    ///  same object ends with rhs empty and exactly one reference released.
    template<typename DerivedType>
    COID_REQUIRES((std::is_convertible_v<DerivedType*, Type*>))
    ref_shared& operator=(ref_shared<DerivedType>&& rhs)
    {
        ref_policy_base* taken_policy_ptr = rhs._policy_ptr;
        Type* taken_object_ptr = static_cast<Type*>(rhs._object_ptr);
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;

        release();

        _policy_ptr = taken_policy_ptr;
        _object_ptr = taken_object_ptr;

        return *this;
    }

    /// @brief Equality with another ref, of this type or of a type derived from it
    /// @note Converts before comparing, so a base ref and a derived ref holding one object compare
    ///  equal although their stored addresses differ under multiple inheritance.
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    friend bool operator==(const ref_shared& lhs, const ref_shared<BaseOrDerivedType>& rhs)
    {
        return lhs.get() == static_cast<Type*>(rhs.get());
    }

    /// @brief Comparison against a raw pointer to the object
    /// @note Non-owning: nothing is adopted and no count moves. A derived pointer converts, and
    ///  so adjusts, on the way in.
    /// @note No operator!= anywhere in the refs - C++20 synthesizes it and the reversed operand
    ///  order from this one declaration, and declaring != would suppress both (MSVC C7692).
    friend bool operator==(const ref_shared& lhs, const Type* rhs) { return lhs._object_ptr == rhs; }

    /// @brief Comparison against nullptr, so a ref reads like the pointer it stands for
    friend bool operator==(const ref_shared& lhs, nullptr_t) { return lhs._object_ptr == nullptr; }

    /// @brief Orders two refs by the address of the object they point at
    /// @note A total but arbitrary order, for keying a map or sorting a container - addresses
    ///  differ from run to run, so never for reproducible output.
    /// @note Same type only: C++20 synthesizes a reversed == but not a reversed <, so a cross-type
    ///  ordering would work in one direction only.
    friend bool operator<(const ref_shared& lhs, const ref_shared& rhs)
    {
        return lhs.get() < rhs.get();
    }

    /// @brief Creates the object through the policy default_ref_policy_trait selects for BaseOrDerivedType
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param policy_arguments - forwarded to the policy's create(), for example the pool to use
    /// @note Releases whatever this ref held first, like every create() overload.
    template<typename BaseOrDerivedType = Type, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    void create(PolicyArguments&&... policy_arguments)
    {
        assert_not_intrusive();
        release();

        _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = static_cast<Type*>(static_cast<BaseOrDerivedType*>(_policy_ptr->get_original_ptr()));
        _policy_ptr->_counter.increase_strong_counter();
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
        assert_not_intrusive();
        release();

        Policy* policy_ptr = Policy::create(std::forward<PolicyArguments>(policy_arguments)...);
        _policy_ptr = static_cast<ref_policy_base*>(policy_ptr);
        _object_ptr = static_cast<typename Policy::element_type*>(policy_ptr->get_original_ptr());
        _policy_ptr->_counter.increase_strong_counter();
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
        assert_not_intrusive();
        release();

        Policy* policy_ptr = Policy::create(std::forward<PolicyArguments>(policy_arguments)...);
        _policy_ptr = static_cast<ref_policy_base*>(policy_ptr);
        _object_ptr = static_cast<typename Policy::element_type*>(policy_ptr->get_original_ptr());
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Adopts an already constructed object, with the policy default_ref_policy_trait selects
    /// @tparam BaseOrDerivedType - the concrete type of the object, Type itself or one derived from it
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own
    /// @param policy_arguments - forwarded to the policy's create()
    /// @note Releases whatever this ref held first, like every create() overload.
    template<typename BaseOrDerivedType, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    void create(BaseOrDerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        assert_not_intrusive();
        release();

        _policy_ptr = static_cast<ref_policy_base*>(default_ref_policy_trait<BaseOrDerivedType>::policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Adopts an already constructed object, with a named policy
    /// @tparam Policy - a policy for Type, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own
    /// @param policy_arguments - forwarded to Policy::create()
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy_for<Policy, Type>))
    void create(Type* object_ptr, PolicyArguments&&... policy_arguments)
    {
        assert_not_intrusive();
        release();
        
        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }

    /// @brief Adopts an already constructed object of a derived type, with a named policy
    /// @tparam DerivedType - type derived from Type, the one the policy counts
    /// @tparam Policy - a policy for DerivedType, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param object_ptr - object to own
    /// @param policy_arguments - forwarded to Policy::create()
    template<typename DerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::negation_v<std::is_same<Type, DerivedType>> && std::is_convertible_v<DerivedType*, Type*> && is_ref_policy_for<Policy, DerivedType>))
    void create(DerivedType* object_ptr, PolicyArguments&&... policy_arguments)
    {
        assert_not_intrusive();
        release();

        _policy_ptr = static_cast<ref_policy_base*>(Policy::create(object_ptr, std::forward<PolicyArguments>(policy_arguments)...));
        _object_ptr = object_ptr;
        _policy_ptr->_counter.increase_strong_counter();
    }


    /// @brief Downcasts this ref to the ref of derived type
    /// @tparam DerivedType - type derived from Type
    /// @return ref of the derived type, empty when this ref is empty
    /// @note The cast has to go through the object pointer. The policy hands out an untyped void*,
    ///  and casting that to DerivedType* skips the this-adjustment whenever DerivedType is not the
    ///  policy's own type - which silently yields a pointer into the wrong subobject.
    template<typename DerivedType>
    COID_REQUIRES((is_static_castable<Type*, DerivedType*>))
    ref_shared<DerivedType> downcast() const
    {
        if (is_empty())
        {
            return ref_shared<DerivedType>();
        }

        ref_shared<DerivedType> result(static_cast<DerivedType*>(_object_ptr), _policy_ptr);
        return result;
    }

    /// @brief Drops this ref's reference - see release()
    ~ref_shared()
    {
        release();
    }

    /// @brief Get pointer of the referenced object, optionally as a more derived type
    /// @tparam BaseOrDerivedType - type to view the object as, Type (the default) or one derived from it
    /// @return object pointer, nullptr when this ref is empty
    /// @note The cast is a downcast, so it is constrained on static-castability rather than
    ///  convertibility - that also rejects a derived type reached through a virtual base, where
    ///  static_cast is ill-formed. Asking for a type the object is not is the caller's
    ///  responsibility; use downcast() if a counted ref is wanted instead of a raw pointer.
    template<typename BaseOrDerivedType = Type>
    COID_REQUIRES((is_static_castable<Type*, BaseOrDerivedType*>))
    BaseOrDerivedType* get() const { return static_cast<BaseOrDerivedType*>(_object_ptr); }

    /// @brief The referenced object
    /// @note Asserts in debug that this ref is set; dereferencing an empty ref is undefined.
    Type& operator*() const { DASSERT(is_set()); return *get(); }
    /// @brief Member access on the referenced object
    /// @note Asserts in debug that this ref is set.
    Type* operator->() const { DASSERT(is_set()); return get(); }

    /// @brief True when this ref holds an object
    explicit operator bool() const { return is_set(); }

    /// @brief Drops this ref's reference and empties it; the last reference destroys the object
    ///  through its policy
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

    /// @brief This ref as an rvalue, for passing it on - same as std::move(*this)
    /// @return rvalue reference to this ref
    ref_shared<Type>&& move()
    {
        return static_cast<ref_shared<Type>&&>(*this);
    }

    /// @brief Whether this ref holds an object
    /// @return true when set
    bool is_set() const { return _object_ptr != nullptr; }

    /// @brief Whether this ref holds nothing
    /// @return true when empty
    bool is_empty() const { return _object_ptr == nullptr; }

    /// @brief Number of refs sharing the object
    /// @return the count, 0 when this ref is empty; a snapshot, since other threads may change it
    uint32 get_strong_refcount() const
    {
        return _policy_ptr != nullptr ? _policy_ptr->_counter.get_strong_counter_value() : 0;
    }

protected: //methods only

    /// @brief Joins an existing policy's count, with an already adjusted object pointer
    /// @param object_ptr - the object, already cast to Type
    /// @param policy_ptr - the policy already counting it
    /// @note Only for downcast(), where the source ref keeps the count above zero throughout.
    explicit ref_shared(Type* object_ptr, ref_policy_base* policy_ptr)
    {
        bool increased = policy_ptr->_counter.try_increase_strong_counter();
        DASSERT(increased); // This should never happen because this constructor is only used in down cast method 
        // so the counter can never drop to zero while performing this action

        _policy_ptr = policy_ptr;
        _object_ptr = object_ptr;
    }
    /// @brief Rejects intrusively reference counted types
    /// @note Deliberately a function body rather than a class scope static_assert: the check needs
    ///  Type complete, and at class scope it would be evaluated on implicit instantiation, turning
    ///  every forward declared ref_shared member into a hard error. Called from the paths that take
    ///  ownership, which need Type complete anyway.
    static void assert_not_intrusive()
    {
        static_assert(!std::is_base_of_v<ref_intrusive_base, Type>,
            "ref_shared<Type>: Type is intrusively reference counted, use iref (coid::ref_intrusive) "
            "instead. ref_shared keeps its own policy and never touches the object's intrusive "
            "refcount, so an iref adopting the same object would become a second, independent owner.");
    }

protected: // members only
    /// @brief The policy counting the object, null when this ref is empty
    ref_policy_base* _policy_ptr = nullptr;
    /// @brief The object, as this ref sees it - adjusted for Type, so not necessarily the address
    ///  the policy holds. See doc/refs.html, "Pointer adjustment and multiple inheritance".
    Type* _object_ptr = nullptr;
};
}; // end of namespace coid