
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
 * Ladislav Hrabcak
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
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

#ifndef __COMM_ATOMIC_STACK_H__
#define __COMM_ATOMIC_STACK_H__

#include "detail/stack_base.h"
#include "pool.h"

#include <new>
#include <utility>

namespace coid {

/// @brief Lock-free stack of values for any number of pushing and popping threads
/// @tparam T - type of the items, stored by value; push needs it copy or move constructible, pop
///  move assignable
/// @note Every item lives in a node: push constructs it there, pop moves it out and destroys it,
///  so an idle node holds no item. pop() touches the item only after its compare-and-swap took the
///  node, when no other thread can reach it - a thread with a stale view of the stack reads only
///  the node's next pointer.
/// @note The nodes come from a global pool of nodes, shared by every stack of T. A popped node goes
///  back to that pool, never to the allocator, so every node stays alive while any stack may use
///  it - see atomic::detail::stack_base.
template <class T>
class stack
    : protected atomic::detail::stack_base<atomic::detail::item_storage<T>>
{
    using stack_t = atomic::detail::stack_base<atomic::detail::item_storage<T>>;
    using node = typename stack_t::node;
    using node_pool = coid::pool<node>;

    /// @brief The global pool of nodes for stacks of T; deletes the nodes when it is destroyed
    static node_pool& pool() { return node_pool::global(); }

public:
    /// @brief Creates an empty stack
    /// @note Touches the global node pool, so that a stack owned by a singleton is destroyed before
    ///  the pool its nodes go back to - singletons are destroyed in reverse order of creation.
    stack() { pool(); }

    /// @brief Destroys the items left in the stack and returns their nodes to the node pool
    ~stack();

    /// @brief Pushes a copy of an item - lock-free, from any thread
    /// @param item - the item to copy
    void push(const T& item) { push_value_internal(item); }

    /// @brief Pushes an item, moving it into the stack - lock-free, from any thread
    /// @param item - the item to move from
    void push(T&& item) { push_value_internal(std::move(item)); }

    /// @brief Pops the most recently pushed item - lock-free, from any thread
    /// @param item - receives the item, moved out of the stack; left untouched when the stack is empty
    /// @return true if an item was popped, false when the stack is empty
    /// @note If moving the item into the target throws, the item is destroyed and its node goes back
    ///  to the pool - the item is lost, the stack stays consistent.
    bool pop(T& item);

private:
    /// @brief Constructs the item in a node from the node pool and pushes the node
    /// @note If constructing the item throws, the node goes back to the pool and nothing is pushed.
    template <class U>
    void push_value_internal(U&& item);
};

////////////////////////////////////////////////////////////////////////////////
template <class T>
stack<T>::~stack()
{
    while (node* n = this->pop_internal()) {
        n->_item.ptr()->~T();
        pool().release_item(n);
    }
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
template <class U>
void stack<T>::push_value_internal(U&& item)
{
    node* n = pool().get_or_create_item();

    try {
        new (n->_item._bytes) T(std::forward<U>(item));
    }
    catch (...) {
        pool().release_item(n);
        throw;
    }

    this->push_internal(n);
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
bool stack<T>::pop(T& item)
{
    node* const n = this->pop_internal();
    if (!n)
        return false;

    // The node is out of the stack now, so this function is the only thing that still knows about
    // it. Its item has to be destroyed and the node returned to the pool whatever happens next -
    // also when the move assignment below throws (it is T's code). A guard does that in its
    // destructor, which runs during stack unwinding as well; plain statements after the move would
    // be skipped by an exception, leaving the item undestroyed and the node lost to both the stack
    // and the pool.
    struct release_on_exit
    {
        node* _node;                        // a copy: release_item() sets the pointer it is given to nullptr
        ~release_on_exit() {
            _node->_item.ptr()->~T();
            pool().release_item(_node);
        }
    } release{ n };

    item = std::move(*n->_item.ptr());
    return true;
}

} // end of namespace coid

#endif // __COMM_ATOMIC_STACK_H__
