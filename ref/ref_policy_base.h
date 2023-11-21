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

#include "ref_counter.h"
#include "../alloc/memtrack.h"

namespace coid
{

/// @brief Base of every reference counting policy - holds the counter, decides nothing else
/// @note Non copyable, non movable.
/// @note A derived policy owes three things this base cannot declare for it: a typedef
///  element_type naming the type get_original_ptr() points at, checked by is_ref_policy_for below;
///  one or more static create() overloads returning the derived type, whose arguments are the
///  policy's own; and the two virtuals declared here.
/// @note Deliberately no create() declared here. An inherited fallback would be picked up by a
///  policy that forgot its own and silently yield a null policy; without one, that mistake fails to
///  compile.
/// @note See doc/refs.html, "Writing your own policy"
class ref_policy_base
{
public: // methods only
    COIDNEWDELETE(ref_policy_base);

    ref_policy_base(const ref_policy_base&) = delete;
    ref_policy_base(const ref_policy_base&&) = delete;
    const ref_policy_base& operator=(const ref_policy_base&) = delete;

    /// @brief Called when the strong count reaches zero
    /// @note Must dispose of both the object and this policy - nothing refers to either afterwards.
    virtual void on_destroy() = 0;

    /// @brief The counted object's address, as the policy stored it
    /// @return the object as an element_type*, typed void*. Cast it back through element_type
    ///  before converting any further, or a base subobject ends up at the wrong address.
    virtual void* get_original_ptr() = 0;

    /// @brief Starts with a zero count
    ref_policy_base() = default;
    virtual ~ref_policy_base() = default;

protected: // methods only
public: // members only
    /// @brief The reference counts. Public because the refs manipulate them directly.
    ref_counter _counter;
};

#ifdef COID_CONCEPTS
/// @brief A reference counting policy - anything deriving from ref_policy_base
template<typename Derived>
concept is_ref_policy = std::is_base_of<ref_policy_base, Derived>::value;

/// @brief A reference counting policy whose object can be referenced as Type*.
/// @note The policy has to name the type of its counted object as element_type. get_original_ptr()
///  hands out a void*, and casting that straight to Type* performs no this-adjustment - so without
///  element_type a ref holding a base pointer to an object created through a policy for a derived
///  type would silently point into the wrong subobject.
template<typename Policy, typename Type>
concept is_ref_policy_for = is_ref_policy<Policy>
    && requires { typename Policy::element_type; }
    && std::is_convertible_v<typename Policy::element_type*, Type*>;
#endif

}; // end of namespace coid
