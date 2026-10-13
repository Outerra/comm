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

#include "detail/stack_base.h"

namespace coid
{

/// @brief Lock-free pool of objects for any number of threads
/// @tparam T - type of the pooled objects; the pool holds T* and takes any object created with new
/// @note Two stacks of nodes: one holding the idle objects, one holding nodes without an object.
///  Taking an object moves its node to the free nodes, releasing one takes a free node (or a new
///  one) for it. Nodes are deleted only by the pool's destructor, so a thread reading a node
///  another thread has just taken reads valid memory - see atomic::detail::stack_base.
/// @note The destructor deletes the objects in the pool and the nodes; objects taken out of the
///  pool are the caller's.
template<class T>
class pool
{
    using stack_t = atomic::detail::stack_base<T*>;
    using node = typename stack_t::node;

public:
    pool() = default;

    pool(const pool&) = delete;
    pool& operator=(const pool&) = delete;

    /// @brief Deletes the objects in the pool and the pool's nodes
    ~pool();

    /// @brief The global pool of T
    static pool& global() { return SINGLETON(pool<T>); }

    /// @brief Takes an object from the pool - lock-free, from any thread
    /// @return the most recently released object, nullptr when the pool is empty
    T* get_item();

    /// @brief Takes an object from the pool, or creates one with new when the pool is empty
    /// @return the object; a pooled one keeps the member values it was released with
    T* get_or_create_item();

    /// @brief Puts an object into the pool - lock-free, from any thread
    /// @param o - the object, owned by the pool from now on; set to nullptr
    void release_item(T*& o);

    /// @brief Deletes the objects in the pool - not while other threads use it
    void reset();

private:
    stack_t _filled;                        ///< nodes holding an idle object
    stack_t _free;                          ///< nodes without an object, for the next release_item
};

////////////////////////////////////////////////////////////////////////////////
template<class T>
pool<T>::~pool()
{
    while (node* n = _filled.pop_internal())
    {
        delete n->_item;
        delete n;
    }

    while (node* n = _free.pop_internal())
    {
        delete n;
    }
}

////////////////////////////////////////////////////////////////////////////////
template<class T>
T* pool<T>::get_item()
{
    node* const n = _filled.pop_internal();
    if (!n)
        return nullptr;

    T* const item = n->_item;
    _free.push_internal(n);
    return item;
}

////////////////////////////////////////////////////////////////////////////////
template<class T>
T* pool<T>::get_or_create_item()
{
    T* const item = get_item();
    return item ? item : new T;
}

////////////////////////////////////////////////////////////////////////////////
template<class T>
void pool<T>::release_item(T*& o)
{
    node* n = _free.pop_internal();
    if (!n)
        n = new node;

    n->_item = o;
    _filled.push_internal(n);
    o = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
template<class T>
void pool<T>::reset()
{
    while (T* item = get_item())
        delete item;
}

}; // end of namespace coid
