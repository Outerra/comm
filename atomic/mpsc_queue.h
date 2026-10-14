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
#include "pool.h"
#include "../commassert.h"
#include "../sync/mutex.h"

#include <new>
#include <utility>

namespace coid {

/// @brief Queue of values for any number of producer threads and one consumer at a time
/// @tparam T - type of the items, stored by value; push needs it copy or move constructible, the
///  pops move assignable
/// @note Producers push lock-free onto a shared list, newest first, with one compare-and-swap each.
///  The consumer takes the whole shared list at once when its own list runs out, reverses it to
///  oldest first and pops from its own list without atomic operations. Because no thread removes
///  a single node from the shared list, the order of a push cannot be confused by a node that
///  left and came back.
/// @note Order: the items of one producer come out in the order it pushed them; items of
///  different producers in the order their pushes took effect.
/// @note One consumer at a time: pop() and pop_if() hold a mutex, so a second consumer waits and
///  stays safe, but it is a usage error - LASSERT, an assert in debug builds and a log in release.
/// @note Every item lives in a node from the global pool of nodes for T, shared with coid::stack<T>:
///  push constructs the item there, a pop moves it out and destroys it.
template <class T>
class mpsc_queue
    : protected atomic::detail::stack_base<atomic::detail::item_storage<T>>
{
protected: // inner definitions only
    using stack_t = atomic::detail::stack_base<atomic::detail::item_storage<T>>;
    using node = typename stack_t::node;
    using node_pool = coid::pool<node>;

    /// @brief The global pool of nodes for T; deletes the nodes when it is destroyed
    static node_pool& pool() { return node_pool::global(); }

public: // methods only
    /// @brief Creates an empty queue
    /// @note Touches the global node pool, so that a queue owned by a singleton is destroyed before
    ///  the pool its nodes go back to - singletons are destroyed in reverse order of creation.
    mpsc_queue();

    mpsc_queue(const mpsc_queue&) = delete;
    mpsc_queue& operator=(const mpsc_queue&) = delete;

    /// @brief Destroys the items left in the queue - not while other threads use it
    ~mpsc_queue();

    /// @brief Appends a copy of an item - lock-free, from any thread
    /// @param item - the item to copy
    void push(const T& item) { push_value_internal(item); }

    /// @brief Appends an item, moving it into the queue - lock-free, from any thread
    /// @param item - the item to move from
    void push(T&& item) { push_value_internal(std::move(item)); }

    /// @brief Removes the oldest item - the consumer
    /// @param item - receives the item, moved out of the queue; left untouched when the queue is empty
    /// @return true if an item was removed, false when the queue is empty
    bool pop(T& item);

    /// @brief Removes the oldest item, if it satisfies a condition - the consumer
    /// @param pred - bool(const T&), asked about the oldest item; must not use this queue
    /// @param item - receives the item, moved out of the queue; left untouched otherwise
    /// @return true if an item was removed; false when the queue is empty or pred refused the item,
    ///  which then stays at the front
    template <class Pred>
    bool pop_if(Pred&& pred, T& item);

    /// @brief Whether the queue holds no item - from any thread, a snapshot
    /// @return true when every item pushed so far has been popped
    /// @note Reads the two counters only, so a thread that waits for the queue to drain does not
    ///  become a consumer. An item counts from the start of its push until a pop takes it out.
    bool is_empty() const;

protected: // inner definitions only
    /// @brief Holds the consumer mutex for one pop; a second consumer is reported and then waits
    struct consumer_lock_internal
    {
        comm_mutex& _mutex;

        explicit consumer_lock_internal(comm_mutex& mutex);
        ~consumer_lock_internal() { _mutex.unlock(); }
    };
protected: // methods only
    /// @brief Constructs the item in a node from the node pool and pushes the node
    /// @note If constructing the item throws, the node goes back to the pool and nothing is pushed.
    template <class U>
    void push_value_internal(U&& item);

    /// @brief The oldest item's node, taking the pushed nodes when the consumer's list is empty
    /// @return the node, still linked; nullptr when the queue is empty
    node* front_internal();

    /// @brief Unlinks the front node, moves its item out, destroys it and returns the node to the pool
    void pop_front_internal(T& item);

    /// @brief Takes the pushed nodes as the consumer's list, oldest first - only when that list is empty
    void take_pushed_internal();
protected: // members only
    // The number of items is kept as two counters that only grow, not as one that goes up and
    // down. One shared counter would cost every pop a locked read-modify-write on a cache line all
    // the producers write to - and the consumer's pops are otherwise free of atomic operations.
    // _pushed is incremented by the producers, right after the head their compare-and-swap already
    // holds; _popped is written by the consumer alone, so a plain store does, and it sits a cache
    // line away from both. is_empty() compares them, which is safe from any thread.
    std::atomic<uint64> _pushed = { 0 };                        ///< items pushed so far - producers, before they publish the item
    char _counter_padding[64 - sizeof(_pushed)];                ///< keeps _popped off the producers' cache line
    
    std::atomic<uint64> _popped = { 0 };                        ///< items popped so far - the consumer only, as each item is unlinked

    comm_mutex _consumer_mutex;             ///< held by pop() and pop_if()
    node* _consumer_list_head = nullptr;    ///< the consumer's list, oldest first - only under _consumer_mutex
};

////////////////////////////////////////////////////////////////////////////////
template <class T>
mpsc_queue<T>::mpsc_queue()
    : _consumer_mutex(500, false, "mpsc_queue::_consumer_mutex")
{
    pool();
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
mpsc_queue<T>::~mpsc_queue()
{
    while (node* n = front_internal()) {
        _consumer_list_head = n->_next.load(std::memory_order_relaxed);
        n->_item.ptr()->~T();
        pool().release_item(n);
    }
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
mpsc_queue<T>::consumer_lock_internal::consumer_lock_internal(comm_mutex& mutex)
    : _mutex(mutex)
{
    if (!_mutex.try_lock()) {
        LASSERTX(false, "mpsc_queue: two threads consume at once - there must be one consumer at a time");
        _mutex.lock();
    }
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
template <class U>
void mpsc_queue<T>::push_value_internal(U&& item)
{
    node* n = pool().get_or_create_item();

    try {
        new (n->_item._bytes) T(std::forward<U>(item));
    }
    catch (...) {
        pool().release_item(n);
        throw;
    }

    // counted before it is published, so it is never popped before it is counted
    _pushed.fetch_add(1, std::memory_order_relaxed);
    this->push_internal(n);
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
bool mpsc_queue<T>::pop(T& item)
{
    consumer_lock_internal lock(_consumer_mutex);

    if (!front_internal())
        return false;

    pop_front_internal(item);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
template <class Pred>
bool mpsc_queue<T>::pop_if(Pred&& pred, T& item)
{
    consumer_lock_internal lock(_consumer_mutex);

    node* const n = front_internal();
    if (!n || !pred(static_cast<const T&>(*n->_item.ptr())))
        return false;

    pop_front_internal(item);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
bool mpsc_queue<T>::is_empty() const
{
    // _popped first: both counters only grow and an item is counted as pushed before it can be
    // popped, so equal values mean every item pushed before _pushed was read had been popped
    const uint64 popped = _popped.load(std::memory_order_acquire);
    const uint64 pushed = _pushed.load(std::memory_order_acquire);
    return pushed == popped;
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
typename mpsc_queue<T>::node* mpsc_queue<T>::front_internal()
{
    if (!_consumer_list_head)
        take_pushed_internal();

    return _consumer_list_head;
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
void mpsc_queue<T>::pop_front_internal(T& item)
{
    node* const n = _consumer_list_head;
    _consumer_list_head = n->_next.load(std::memory_order_relaxed);

    // out of the queue now, also if the move below throws; the consumer is the only writer
    _popped.store(_popped.load(std::memory_order_relaxed) + 1, std::memory_order_release);

    // The node is unlinked now, so this function is the only thing that still knows about it. Its
    // item has to be destroyed and the node returned to the pool whatever happens next - also when
    // the move assignment below throws (it is T's code). A guard does that in its destructor, which
    // runs during stack unwinding as well; plain statements after the move would be skipped by an
    // exception, leaving the item undestroyed and the node lost to both the queue and the pool.
    struct release_on_exit
    {
        node* _node;                        // a copy: release_item() sets the pointer it is given to nullptr
        ~release_on_exit() {
            _node->_item.ptr()->~T();
            pool().release_item(_node);
        }
    } release{ n };

    item = std::move(*n->_item.ptr());
}

////////////////////////////////////////////////////////////////////////////////
template <class T>
void mpsc_queue<T>::take_pushed_internal()
{
    DASSERT(_consumer_list_head == nullptr);

    // the shared list starts with the newest node - reversed, the oldest comes first
    node* pushed = this->take_all_internal();
    node* first = nullptr;
    while (pushed) {
        node* const next = pushed->_next.load(std::memory_order_relaxed);
        pushed->_next.store(first, std::memory_order_relaxed);
        first = pushed;
        pushed = next;
    }

    _consumer_list_head = first;
}

} // end of namespace coid
