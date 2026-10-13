
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

#ifndef __COMM_ATOMIC_DETAIL_STACK_BASE_H__
#define __COMM_ATOMIC_DETAIL_STACK_BASE_H__

#include "../atomic.h"
#include "../../alloc/commalloc.h"

#include <atomic>
#include <cstdint>
#include <new>

namespace coid {
template<class T> class pool;
}

namespace atomic {
namespace detail {

/// @brief Uninitialized storage for one T, so that a node can hold a value without T having to be
///  default constructible, and an idle node holds no value at all
template <class T>
struct item_storage
{
    alignas(T) unsigned char _bytes[sizeof(T)];

    /// @brief The value constructed in the storage
    T* ptr() { return std::launder(reinterpret_cast<T*>(_bytes)); }
};

/// @brief Core of the lock-free stacks (Treiber stack): linked nodes that any number of threads
///  push and pop
/// @tparam T - type of the item a node holds, by value - a pointer for coid::pool, raw storage for
///  coid::stack
/// @note Used only by derived classes and by coid::pool, through push_internal and pop_internal.
///  They allocate the nodes and must keep every node alive while any other thread may be
///  popping: a thread that pops reads the next pointer of a node another thread may have popped
///  meanwhile, which is harmless only while that node's memory is valid - its compare-and-swap
///  then fails and it retries.
/// @note The head is a node pointer plus a tag, compared and swapped as one 2-pointer-wide value
///  through std::atomic_ref. Every push and pop increments the tag, so a head that returns to the
///  same node after a pop and a push never compares equal to the head a slower thread read before
///  them (the ABA problem).
template <class T>
class stack_base
{
    template<class> friend class coid::pool;

protected:
    /// @brief An element of the stack, holding one item
    struct node
    {
        COIDNEWDELETE(node);

        std::atomic<node*> _next{ nullptr };    ///< next node; atomic because a thread with a stale view of a stack may read it while the node is reused elsewhere
        T _item{};                          ///< the item; written before the push that publishes the node, read after the pop that takes it
    };

    stack_base() = default;
    ~stack_base() = default;

    stack_base(const stack_base&) = delete;
    stack_base& operator=(const stack_base&) = delete;

    /// @brief Pushes a node - lock-free, from any thread
    /// @param n - the node, in no stack
    void push_internal(node* n);

    /// @brief Pops the most recently pushed node - lock-free, from any thread
    /// @return the node, nullptr when the stack is empty
    node* pop_internal();

    /// @brief Takes every node at once, leaving the stack empty - lock-free, from any thread
    /// @return the nodes linked through _next, the most recently pushed first; nullptr when empty
    node* take_all_internal();

private:
    /// @brief The stack's head: the top node and a tag, accessed only through std::atomic_ref
    /// @note Not a std::atomic: on MSVC std::atomic of a 16-byte type takes an internal lock, while
    ///  std::atomic_ref of it is lock-free (cmpxchg16b).
    struct alignas(2 * sizeof(void*)) ptr_t
    {
        node* _ptr = nullptr;               ///< top node, nullptr when the stack is empty
        std::uintptr_t _tag = 0;            ///< incremented by every push and pop, so a reused node never repeats a head value
    };

    static_assert(sizeof(ptr_t) == 2 * sizeof(void*), "the head must be exactly two pointers wide");
#ifdef _MSVC_STL_VERSION
    static_assert(std::atomic_ref<ptr_t>::is_always_lock_free, "the head must be swapped lock-free (cmpxchg16b on x64)");
#endif

    ptr_t _head;
};

////////////////////////////////////////////////////////////////////////////////
template <class T>
void stack_base<T>::push_internal(node* n)
{
    std::atomic_ref<ptr_t> head(_head);
    ptr_t oldhead = head.load(std::memory_order_relaxed);

    for (;;) {
        // atomic: a thread still popping with a head it read before this node was popped may read
        // the next pointer concurrently
        n->_next.store(oldhead._ptr, std::memory_order_relaxed);

        const ptr_t newhead{ n, oldhead._tag + 1 };

        // release: the thread that pops the node sees its item and next pointer
        if (head.compare_exchange_weak(oldhead, newhead, std::memory_order_release, std::memory_order_relaxed))
            return;
    }
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
typename stack_base<T>::node* stack_base<T>::pop_internal()
{
    std::atomic_ref<ptr_t> head(_head);

    // acquire, also on a failed compare-and-swap: the node of the head read is dereferenced next
    ptr_t oldhead = head.load(std::memory_order_acquire);

    for (;;) {
        if (oldhead._ptr == nullptr)
            return nullptr;

        // the node may already be popped by another thread: the value read is then stale, the node
        // memory is alive (see the class note) and the compare-and-swap below fails
        node* const next = oldhead._ptr->_next.load(std::memory_order_relaxed);

        const ptr_t newhead{ next, oldhead._tag + 1 };

        if (head.compare_exchange_weak(oldhead, newhead, std::memory_order_acquire, std::memory_order_acquire))
            return oldhead._ptr;
    }
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
typename stack_base<T>::node* stack_base<T>::take_all_internal()
{
    std::atomic_ref<ptr_t> head(_head);

    // acquire: the items and next pointers of the nodes taken are visible
    ptr_t oldhead = head.load(std::memory_order_acquire);

    for (;;) {
        if (oldhead._ptr == nullptr)
            return nullptr;

        // the tag goes up like on every other change, so no thread compares an earlier head equal
        const ptr_t newhead{ nullptr, oldhead._tag + 1 };

        if (head.compare_exchange_weak(oldhead, newhead, std::memory_order_acquire, std::memory_order_acquire))
            return oldhead._ptr;
    }
}

} // end of namespace detail
} // end of namespace atomic

#endif // __COMM_ATOMIC_DETAIL_STACK_BASE_H__
