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
#include "ref_policy_base.h"
#include "ref_default_policy_trait.h"
#include "ref_intrusive_base.h"

namespace coid {
/// @brief Sole owner of an object, move only, never counted
/// @tparam Type type of the owned object
/// @note The one ref that uses only the policy's create and destroy functionality. A policy it
///  holds made the object and will destroy it again, but its counter is never touched and stays at
///  zero - release() asserts that. create() never attaches one; name it - create<Policy>() - when
///  the object has to be made or destroyed some other way than new and delete.
/// @note Can be moved to a ref_shared, which then does count it.
/// @note Thread safety: a ref_unique is a single owner, so one instance must not be touched by two
///  threads at once. Move it to the thread that should own it.
/// @note See doc/refs.html
template <class Type>
class ref_unique
{
    template<typename> friend class ref_unique;     // make all template instances friends
    template<typename> friend class ref_shared;     // ref_shared moves a ref_unique's object and policy out
public: // methods only
    COIDNEWDELETE(ref_unique);

    /// @brief Creates an empty ref
    ref_unique() = default;
    
    /// @brief Creates an empty ref, so that nullptr can be passed where a ref is expected
    ref_unique(nullptr_t)
    {};

    /// @brief Releases the object - see release()
    ~ref_unique() 
    { 
        release();
    }

    /// @brief Takes ownership of an already constructed object, without a policy
    /// @param object_ptr - object to take ownership of, must have come from new
    /// @note Explicit, so that a raw pointer never becomes an owner silently at a call boundary.
    ///  It does not make the adoption safe - see doc/refs.html, "Adopting a raw pointer".
    template <class BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    explicit ref_unique(BaseOrDerivedType* object_ptr)
    {
        assert_not_intrusive();
        _object_ptr = object_ptr;
    }

    /// @brief Not copyable - a ref_unique is the sole owner; move it instead
    ref_unique(const ref_unique& object_ptr) = delete;

    /// @brief Takes over the object and policy of another ref_unique, of this type or a derived one
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @param rhs - the ref to take from; left empty
    /// @note noexcept because it only steals pointers - it never releases anything, so it cannot
    ///  reach the pooling path that can throw. Keep it that way: a vector relocates with
    ///  move-if-noexcept, so losing this would make it copy refs instead of moving them.
    template <class BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    ref_unique(ref_unique<BaseOrDerivedType>&& rhs) noexcept
    {
        _policy_ptr = rhs._policy_ptr;
        _object_ptr = rhs._object_ptr;
        rhs._policy_ptr = nullptr;
        rhs._object_ptr = nullptr;
    }

    /// @brief Creates a default constructed object with plain new, and no policy
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @note Releases whatever this ref held first, like every create() overload.
    /// @note default_ref_policy_trait is deliberately not consulted, so what this call does is
    ///  visible at the call site. Name a policy for anything other than new and delete:
    ///  create<ref_policy_pooled<Type>>(). See doc/refs.html, "ref_unique".
    /// @note To pass constructor arguments, build the object yourself: create(new Type(args)).
    /// @note With a type derived from Type, release() deletes through the adjusted base pointer,
    ///  so Type needs a virtual destructor.
    template<typename BaseOrDerivedType = Type>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    void create()
    {
        assert_not_intrusive();
        release();

        _object_ptr = new BaseOrDerivedType();
    }

    /// @brief Creates the object through a named policy, which this ref keeps to destroy it with
    /// @tparam Policy - a policy for Type, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param policy_arguments - forwarded to Policy::create()
    /// @note Releases whatever this ref held first, like every create() overload.
    /// @note Only the policy's create and destroy functionality is used; its counter stays at zero.
    template<typename Policy, typename... PolicyArguments>
    COID_REQUIRES((is_ref_policy_for<Policy, Type>))
    void create(PolicyArguments&&... policy_arguments)
    {
        assert_not_intrusive();
        release();

        Policy* policy_ptr = Policy::create(std::forward<PolicyArguments>(policy_arguments)...);
        _policy_ptr = static_cast<ref_policy_base*>(policy_ptr);
        _object_ptr = static_cast<typename Policy::element_type*>(policy_ptr->get_original_ptr());
    }

    /// @brief Creates an object of Type or of a type derived from it, through a named policy
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it, the one the policy counts
    /// @tparam Policy - a policy for BaseOrDerivedType, see is_ref_policy_for
    /// @tparam PolicyArguments - the policy's own creation arguments
    /// @param policy_arguments - forwarded to Policy::create()
    template<typename BaseOrDerivedType, typename Policy, typename... PolicyArguments>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*> && is_ref_policy_for<Policy, BaseOrDerivedType>))
    void create(PolicyArguments&&... policy_arguments)
    {
        assert_not_intrusive();
        release();

        Policy* policy_ptr = Policy::create(std::forward<PolicyArguments>(policy_arguments)...);
        _policy_ptr = static_cast<ref_policy_base*>(policy_ptr);
        _object_ptr = static_cast<typename Policy::element_type*>(policy_ptr->get_original_ptr());
    }

    /// @brief Takes ownership of an already constructed object, without a policy
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @param object_ptr - object to own, may be null; deleted on release, so it must come from new
    /// @note Releases whatever this ref held first, like every create() overload.
    /// @note This is how constructor arguments reach the object: create(new Type(args)).
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    void create(BaseOrDerivedType* object_ptr)
    {
        assert_not_intrusive();
        release();

        _object_ptr = object_ptr;
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

    /// @brief The stored object pointer, by reference, for code that has to write it directly
    /// @note Bypasses every invariant this class maintains - it neither releases what was held nor
    ///  adjusts anything. Prefer create() or eject().
    Type*& get_ptr_ref() { return _object_ptr; }

    /// @brief True when this ref holds an object
    explicit operator bool() const { return _object_ptr != 0; }

    /// @brief Comparison against a raw pointer to the object
    /// @note Non-owning: nothing is adopted and no count moves. A derived pointer converts, and
    ///  so adjusts, on the way in.
    /// @note No operator!= anywhere in the refs - C++20 synthesizes it and the reversed operand
    ///  order from this one declaration, and declaring != would suppress both (MSVC C7692).
    friend bool operator==(const ref_unique& lhs, const Type* rhs) { return lhs._object_ptr == rhs; }

    /// @brief Comparison against nullptr, so a ref reads like the pointer it stands for
    /// @note The operator==(const Type*) above would already accept a nullptr by conversion, but
    ///  spelling it out keeps this the same as ref_shared and ref_intrusive, and it is the better
    ///  overload match so it wins without ambiguity.
    friend bool operator==(const ref_unique& lhs, nullptr_t) { return lhs._object_ptr == nullptr; }

    /// @brief Orders two refs by the address of the object they point at
    /// @note A total but arbitrary order, for keying a map or sorting a container - addresses
    ///  differ from run to run, so never for reproducible output.
    /// @note Deliberately no ref_unique to ref_unique operator==: two of them can never point at
    ///  the same object, so it could only be true for two empty refs.
    friend bool operator<(const ref_unique& lhs, const ref_unique& rhs)
    {
        return lhs._object_ptr < rhs._object_ptr;
    }

    /// @brief The referenced object
    /// @note Asserts in debug that this ref is set; dereferencing an empty ref is undefined.
    Type& operator *(void) { DASSERT(_object_ptr != nullptr); return *_object_ptr; }
    /// @brief The referenced object, through a const ref
    const Type& operator *(void) const { DASSERT(_object_ptr != nullptr); return *_object_ptr; }

    /// @brief Member access on the referenced object
    /// @note Asserts in debug that this ref is set.
    Type* operator ->(void) { DASSERT(_object_ptr != nullptr); return _object_ptr; }
    /// @brief Member access through a const ref
    const Type* operator ->(void) const { DASSERT(_object_ptr != nullptr); return _object_ptr; }

    /// @brief Takes ownership of an already constructed object, without a policy
    /// @tparam BaseOrDerivedType - Type itself or a type derived from it
    /// @param object_ptr - object to own, may be null; deleted on release, so it must come from new
    /// @return this ref
    /// @note Same as create(object_ptr). Implicit, unlike the constructor, because the target's
    ///  type is visible at its declaration.
    template<typename BaseOrDerivedType>
    COID_REQUIRES((std::is_convertible_v<BaseOrDerivedType*, Type*>))
    ref_unique& operator= (BaseOrDerivedType* object_ptr) {
        assert_not_intrusive();
        release();

        _object_ptr = object_ptr;
        return *this;
    }

    /// @brief Releases what this ref holds and takes over the object and policy of rhs
    /// @param rhs - the ref to take from; left empty
    /// @return this ref
    /// @note Load bearing: a user-declared move assignment operator is what keeps copy assignment
    ///  deleted. The templated overload below does not count, being a template - remove this one
    ///  and the compiler declares operator=(const ref_unique&), which copies the pointer and leaves
    ///  two refs owning one object.
    ref_unique& operator=(ref_unique&& rhs)
    {
        if (&rhs != this)
        {
            release();
            _policy_ptr = rhs._policy_ptr;
            _object_ptr = rhs._object_ptr;
            rhs._policy_ptr = nullptr;
            rhs._object_ptr = nullptr;
        }

        return *this;
    }

    /// @brief Releases what this ref holds and takes over the object and policy of a ref to a
    ///  derived type
    /// @tparam DerivedType - type derived from Type
    /// @param rhs - the ref to take from; left empty
    /// @return this ref
    /// @note Takes rhs's object before releasing its own, so it is safe even when both refer to the
    ///  same object - releasing first would destroy the object about to be stored.
    template<typename DerivedType>
    COID_REQUIRES((std::is_convertible_v<DerivedType*, Type*>))
    ref_unique& operator=(ref_unique<DerivedType>&& rhs)
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

    /// @brief Whether this ref holds an object
    /// @return true when set
    bool is_set() const 
    { 
        return _object_ptr != nullptr; 
    }

    /// @brief Whether this ref holds nothing
    /// @return true when empty
    bool is_empty() const 
    { 
        return _object_ptr == nullptr; 
    }

    /// @brief This ref as an rvalue, for passing it on - same as std::move(*this)
    /// @return rvalue reference to this ref
    ref_unique&& move() { return static_cast<ref_unique&&>(*this); }

    /// @brief Destroys the object - through its policy if it has one, else with delete - and
    ///  empties the ref
    /// @note Without a policy the object is deleted directly, and deleting an incomplete type
    ///  silently skips its destructor - the compiler only warns (C4150). The check lives in this
    ///  function body rather than at class scope so that ref_unique<Type> can be declared
    ///  for a forward declared Type; the way out is to give the enclosing class an out-of-line
    ///  destructor defined where Type is complete, which is where release() then lands.
    void release()
    {
        static_assert(sizeof(Type) > 0,
            "ref_unique<Type>: Type must be complete here - deleting an incomplete type would skip "
            "its destructor. Include its definition in this translation unit, or give the enclosing "
            "class an out-of-line destructor defined where Type is complete.");

        if (_policy_ptr != nullptr)
        {
            DASSERT(_policy_ptr->_counter.get_strong_counter_value() == 0);
            _policy_ptr->on_destroy();
            _policy_ptr = nullptr;
        }
        else if (_object_ptr)
        {
            delete _object_ptr;
        }

        _object_ptr = nullptr;
    }

    /// @brief Give up ownership of the object, and of the policy when this ref owns one
    /// @param policy_ptr_out - receives the policy, or nullptr when there is none. May be omitted
    ///  only when the caller knows no policy is attached.
    /// @return the object pointer, nullptr when this ref was empty
    /// @note An object created through a policy has to go back through it, so the caller takes the
    ///  policy along and finishes with policy_ptr->on_destroy(). Ejecting a policy owning ref with
    ///  nowhere to put the policy aborts. See doc/refs.html, "eject()".
    Type* eject(ref_policy_base** policy_ptr_out = nullptr)
    {
        if (policy_ptr_out != nullptr)
        {
            *policy_ptr_out = _policy_ptr;
        }
        else
        {
            RASSERT_FATALX(_policy_ptr == nullptr,
                "ref_unique::eject() on a ref that owns a policy, with nowhere to put it - the "
                "object would be orphaned from the policy that has to take it back. Pass the "
                "policy out parameter.");
        }

        Type* object_ptr = _object_ptr;
        _policy_ptr = nullptr;
        _object_ptr = nullptr;
        return object_ptr;
    }

protected: // methods only
    /// @brief Rejects intrusively reference counted types
    /// @note Deliberately a function body rather than a class scope static_assert: the check needs
    ///  Type complete, and at class scope it would be evaluated on implicit instantiation, turning
    ///  every forward declared ref_unique member into a hard error. Called from the paths that take
    ///  ownership, which need Type complete anyway.
    static void assert_not_intrusive()
    {
        static_assert(!std::is_base_of_v<ref_intrusive_base, Type>,
            "ref_unique<Type>: Type is intrusively reference counted, use iref (coid::ref_intrusive) "
            "instead. ref_unique keeps its own policy and never touches the object's intrusive "
            "refcount, so an iref adopting the same object would become a second, independent owner.");
    }

protected: // members only
    /// @brief The policy that made the object and will destroy it, when one was named - often null.
    ///  Its counter is unused here. See doc/refs.html, "ref_unique".
    ref_policy_base* _policy_ptr = nullptr;
    /// @brief The object, as this ref sees it
    Type* _object_ptr = nullptr;
};

}; // end of namespace coid