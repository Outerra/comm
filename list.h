#pragma once
#ifndef __COID_LIST_H__
#define __COID_LIST_H__

#include "atomic/basic_pool.h"
#include "alloc/commalloc.h"

#include <iterator>
#include <algorithm>

namespace coid {

/// doubly-linked list
template<class T>
class list
{
public:

    ///
    struct node
    {
        COIDNEWDELETE(node);

        union {
            node* _next_basic_pool;
            node* _next = 0;
        };
        node* _prev = 0;
        union
        {
            uchar _item_mem[sizeof(T)];
            T _item;
        };


        node() {}

        node(bool) { _next = _prev = this; memset(&_item_mem, 0xff, sizeof(_item_mem)); }

        ~node() {};

        void operator = (const node& n) { DASSERT(false); }

        T& item() { return *reinterpret_cast<T*>(_item_mem); }

        const T& item() const { return *reinterpret_cast<const T*>(_item_mem); }

        T* item_ptr() { return reinterpret_cast<T*>(_item_mem); }

        const T* item_ptr() const { return reinterpret_cast<const T*>(_item_mem); }
    };

    struct _list_iterator_base
    {
        node* _node;

        bool operator == (const _list_iterator_base& p) const { return p._node == _node; }
        bool operator != (const _list_iterator_base& p) const { return p._node != _node; }

        T& operator *(void) const { return _node->item(); }

        T* ptr() const { return _node->item_ptr(); }

        T* operator ->(void) const { return ptr(); }

        _list_iterator_base() : _node(0) {}
        explicit _list_iterator_base(node* p) : _node(p) {}
    };

    struct _list_iterator
        : public _list_iterator_base
    {
        typedef T value_type;

#ifdef SYSTYPE_MSVC
#pragma warning( disable : 4284 )
#endif //SYSTYPE_MSVC

        _list_iterator& operator ++() { this->_node = this->_node->_next; return *this; }
        _list_iterator& operator --() { this->_node = this->_node->_prev; return *this; }

        _list_iterator  operator ++(int) { _list_iterator x(this->_node); this->_node = this->_node->_next;  return x; }
        _list_iterator  operator --(int) { _list_iterator x(this->_node); this->_node = this->_node->_prev;  return x; }

        _list_iterator() : _list_iterator_base() {}
        explicit _list_iterator(node* p) : _list_iterator_base(p) {}
    };

    struct _list_reverse_iterator
        : public _list_iterator_base
    {
        typedef T value_type;

#ifdef SYSTYPE_MSVC
#pragma warning( disable : 4284 )
#endif //SYSTYPE_MSVC

        _list_reverse_iterator& operator ++() { this->_node = this->_node->_prev; return *this; }
        _list_reverse_iterator& operator --() { this->_node = this->_node->_next; return *this; }

        _list_reverse_iterator operator ++(int) { _list_reverse_iterator x(this->_node); this->_node = this->_node->_prev;  return x; }
        _list_reverse_iterator operator --(int) { _list_reverse_iterator x(this->_node); this->_node = this->_node->_next;  return x; }

        _list_reverse_iterator() : _list_iterator_base() {}
        explicit _list_reverse_iterator(node* p) : _list_iterator_base(p) {}
    };

    struct _list_const_iterator
        : public _list_iterator_base
    {
        typedef T value_type;

#ifdef SYSTYPE_MSVC
#pragma warning( disable : 4284 )
#endif //SYSTYPE_MSVC

        _list_const_iterator& operator ++() { this->_node = this->_node->_next; return *this; }
        _list_const_iterator& operator --() { this->_node = this->_node->_prev; return *this; }

        _list_const_iterator operator ++(int) { _list_const_iterator x(this->_node); this->_node = this->_node->_next;  return x; }
        _list_const_iterator operator --(int) { _list_const_iterator x(this->_node); this->_node = this->_node->_prev;  return x; }

        _list_const_iterator() {}
        explicit _list_const_iterator(const node* p) : _list_iterator_base(const_cast<node*>(p)) {}
    };

    struct _list_const_reverse_iterator
        : public _list_iterator_base
    {
        typedef const T value_type;

#ifdef SYSTYPE_MSVC
#pragma warning( disable : 4284 )
#endif //SYSTYPE_MSVC

        _list_const_reverse_iterator& operator ++() { this->_node = this->_node->_prev; return *this; }
        _list_const_reverse_iterator& operator --() { this->_node = this->_node->_next; return *this; }

        _list_const_reverse_iterator operator ++(int) { _list_const_reverse_iterator x(this->_node); this->_node = this->_node->_prev;  return x; }
        _list_const_reverse_iterator operator --(int) { _list_const_reverse_iterator x(this->_node); this->_node = this->_node->_next;  return x; }

        _list_const_reverse_iterator() : _list_iterator_base() {}
        explicit _list_const_reverse_iterator(const node* p) : _list_iterator_base(const_cast<node*>(p)) {}
    };

    using iterator = _list_iterator;
    using reverse_iterator = _list_reverse_iterator;
    using const_iterator = _list_const_iterator;
    using const_reverse_iterator = _list_const_reverse_iterator ;

protected:
    ///
    atomic::basic_pool<node> _npool;

    ///
    node _head;

    // prev tail item_0 ---------- item_n head next

protected:

    node* new_node(node* itpos, const T& item)
    {
        node* nn = _npool.pop_new();

        nn->_next = itpos;
        nn->_prev = itpos->_prev;
        new (nn->_item_mem) T(item);

        return nn;
    }

    node* new_node(node* itpos, T&& item)
    {
        node* nn = _npool.pop_new();

        nn->_next = itpos;
        nn->_prev = itpos->_prev;

        new (nn->_item_mem) T(std::move(item));

        return nn;
    }

    void delete_node(node* n)
    {
        n->item().~T();
        _npool.push(n);
    }

    ///
    void insert(node* itpos, const T& item)
    {
        node* const nn = new_node(itpos, item);
        nn->_prev->_next = itpos->_prev = nn;
    }

    ///
    void insert(node* itpos, T&& item)
    {
        node* const nn = new_node(itpos, std::forward<T>(item));
        nn->_prev->_next = itpos->_prev = nn;
    }

    ///
    T* insert_uninit(node* itpos)
    {
        node* nn = _npool.pop_new();

        nn->_next = itpos;
        nn->_prev = itpos->_prev;
        nn->_prev->_next = itpos->_prev = nn;

        return &nn->item();
    }

    /// @brief Erases node pointed by iterator
    /// @param it - iterator
    void erase_internal(_list_iterator_base& it)
    {
        if (it._node != &_head) {
            it._node->_prev->_next = it._node->_next;
            it._node->_next->_prev = it._node->_prev;
            delete_node(it._node);
        }
    }

public:

    ///
    list() : _npool(), _head(false) {}

    ///
    ~list() {}

    ///
    void push_front(const T& item) { insert(_head._next, item); }

    ///
    void push_front(T&& item) { insert(_head._next, std::forward<T>(item)); }

    ///
    T* push_front_uninit() { return insert_uninit(_head._next); }

    ///
    void push_back(const T& item) { insert(&_head, item); }

    ///
    void push_back(T&& item) { insert(&_head, std::forward<T>(item)); }

    ///
    T* push_back_uninit() { return insert_uninit(&_head); }

    ///
    void insert(_list_iterator_base& it, const T& item) {
        insert(it._node->_next, item);
    }

    ///
    void insert_before(_list_iterator_base& it, const T& item)
    {
        insert(it._node, item);
    }

    /// @brief Erase item held by iterator and sets iterator to the next item
    /// @param it - forward iterator
    void erase(iterator& it)
    {
        if (it._node != &_head) {
            it._node->_prev->_next = it._node->_next;
            it._node->_next->_prev = it._node->_prev;
            node* node_to_del_ptr = it._node;
            it._node = it._node->_next;
            delete_node(node_to_del_ptr);
        }
    }

    /// @brief Erase item held by iterator and sets iterator to the prev item
/// @param it - reverse iterator
    void erase(reverse_iterator& it)
    {
        if (it._node != &_head) {
            it._node->_prev->_next = it._node->_next;
            it._node->_next->_prev = it._node->_prev;
            node* node_to_del_ptr = it._node;
            it._node = it._node->_prev;
            delete_node(node_to_del_ptr);
        }
    }

    ///
    bool pop_front(T& item)
    {
        if (!is_empty()) {
            item = std::move(front());
            iterator eit(_head._next);
            erase(eit);
            return true;
        }
        return false;
    }

    ///
    T* pop_front_ptr()
    {
        if (!is_empty()) {
            T* item = &front();
            iterator eit(_head._next);
            erase(eit);
            return item;
        }
        return 0;
    }

    ///
    bool pop_back(T& item)
    {
        if (!is_empty()) {
            item = std::move(back());
            iterator eit(_head._prev);
            erase(eit);
            return true;
        }
        return false;
    }

    ///
    T* pop_back_ptr()
    {
        if (!is_empty()) {
            T* item = &back();
            iterator eit(_head._prev);
            erase(eit);
            return item;
        }
        return 0;
    }

    ///
    void clear()
    {
        node* n = _head._next;
        _head._next = _head._prev = &_head;
        while (n->_next != &_head) {
            n = n->_next;
            delete_node(n->_prev);
        }
    }

    ///
    bool is_empty() const { return _head._next == &_head; }

    T& front() const { DASSERTN(!is_empty()); return _head._next->item(); }
    T& back() const { DASSERTN(!is_empty()); return _head._prev->item(); }

    iterator begin() { return iterator(_head._next); }
    iterator end() { return iterator(&_head); }

    const_iterator begin() const { return const_iterator(_head._next); }
    const_iterator end() const { return const_iterator(&_head); }

    reverse_iterator rbegin() { return reverse_iterator(_head._prev); }
    reverse_iterator rend() { return reverse_iterator(&_head); }

    const_reverse_iterator rbegin() const { return const_reverse_iterator(_head._prev); }
    const_reverse_iterator rend() const { return const_reverse_iterator(&_head); }

    iterator find(const T& item)
    {
        iterator i = begin(), e = end();
        for (; *i != item && i != e; ++i);
        return i;
    }

    const_iterator find(const T& item) const
    {
        return find(item);
    }

    /// @return true if the item was found
    bool erase(const T& item)
    {
        iterator i = find(item);
        bool exists = i != end();
        if (exists)
            erase(i);
        return exists;
    }

    bool del_item(const T& item)
    {
        return erase(item);
    }


    ///Remove each element for which the predicate returns true
    /// @param fn functor as fn([const] T&)
    template<typename Func>
    void del_if(Func fn)
    {
        iterator it = begin();
        const iterator it_e = end();
        while (it != it_e)
        {
            T& v = *it;

            if (fn(v)) 
            {
                erase(it);
            }

            ++it;
        }
    }

    /// @brief Find element for which the predicate returns true 
    /// @tparam Func - void(Func)(T&) 
    /// @param fn - predicate function
    /// @return element ptr or nullptr if not found
    /// @note iterates in forward
    template<typename Func>
    T* find_if(Func fn)
    {
        iterator it = begin();
        const iterator it_e = end();
        while (it != it_e)
        {
            T& v = *it;

            if (fn(v))
            {
                return &(*it);
            }

            ++it;
        }

        return nullptr;
    }

    /// @brief Find element for which the predicate returns true 
    /// @tparam Func - void(Func)(T&) 
    /// @param fn - predicate function
    /// @return element ptr or nullptr if not found
    /// @note iterates in forward
    template<typename Func>
    T* find_if_back(Func fn)
    {
        reverse_iterator it = rbegin();
        const reverse_iterator it_e = rend();
        while (it != it_e)
        {
            T& v = *it;

            if (fn(v))
            {
                return &(*it);
            }
            ++it;
        }

        return nullptr;
    }

    /// @brief Find element for which the predicate returns true 
    /// @tparam Func - void(Func)(const T&) 
    /// @param fn - predicate function
    /// @return element ptr or nullptr if not found
    /// @note iterates in forward
    template<typename Func>
    const T* find_if(Func fn) const
    {
        const_iterator it = begin();
        const const_iterator it_e = end();
        while (it != it_e)
        {
            T& v = *it;

            if (fn(v))
            {
                return &(*it);
            }

            ++it;
        }

        return nullptr;
    }

    /// @brief Find element for which the predicate returns true 
    /// @tparam Func - void(Func)(const T&) 
    /// @param fn - predicate function
    /// @return element ptr or nullptr if not found
    /// @note iterates in forward
    template<typename Func>
    const T* find_if_back(Func fn) const
    {
        const_reverse_iterator it = rbegin();
        const const_reverse_iterator it_e = rend();
        while (it != it_e)
        {
            T& v = *it;

            if (fn(v))
            {
                return &(*it);
            }
            ++it;
        }

        return nullptr;
    }

};

} //end of namespace coid

#endif // __COID_LIST_H__
