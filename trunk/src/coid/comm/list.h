#pragma once
#ifndef __COID_LIST_H__
#define __COID_LIST_H__

#include "atomic/basic_pool.h"
#include "alloc/commalloc.h"
#include "ref_helpers.h"

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
		COIDNEWDELETE(list::node);

		union {
			node *_next_basic_pool;
			node *_next;
		};
	    node *_prev;
		uchar _item[sizeof(T)];

	    node() : _next(0), _prev(0) {}

	    node(bool) { _next = _prev = this; memset(&_item, 0xff, sizeof(_item)); }

        void operator = (const node &n) { DASSERT(false); }

		T& item() { return *reinterpret_cast<T*>(_item); }

		const T& item() const { return *reinterpret_cast<const T*>(_item); }

		T* item_ptr() { return reinterpret_cast<T*>(_item); }

		const T* item_ptr() const { return reinterpret_cast<const T*>(_item); }
    };

	struct _list_iterator_base
        : std::iterator<std::bidirectional_iterator_tag,T>
    {
		node*  _node;

		bool operator == (const _list_iterator_base& p) const { return p._node==_node; }
		bool operator != (const _list_iterator_base& p) const { return p._node!=_node; }

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

		_list_iterator& operator ++() { _node=_node->_next; return *this; }
		_list_iterator& operator --() { _node=_node->_prev; return *this; }

		_list_iterator  operator ++(int) { _list_iterator x(_node); _node=_node->_next;  return x; }
		_list_iterator  operator --(int) { _list_iterator x(_node); _node=_node->_prev;  return x; }

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

		_list_reverse_iterator& operator ++() { _node=_node->_prev; return *this; }
		_list_reverse_iterator& operator --() { _node=_node->_next; return *this; }

		_list_reverse_iterator operator ++(int) { _list_reverse_iterator x(_node); _node=_node->_prev;  return x; }
		_list_reverse_iterator operator --(int) { _list_reverse_iterator x(_node); _node=_node->_next;  return x; }

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

		_list_const_iterator& operator ++() { _node=_node->_next; return *this; }
		_list_const_iterator& operator --() { _node=_node->_prev; return *this; }

		_list_const_iterator operator ++(int) { _list_const_iterator x(_node); _node=_node->_next;  return x; }
		_list_const_iterator operator --(int) { _list_const_iterator x(_node); _node=_node->_prev;  return x; }

		_list_const_iterator() : _node(0) {}
		explicit _list_const_iterator(node* p) : _node(p) {}
	};

	struct _list_const_reverse_iterator 
        : public _list_iterator_base
	{
		typedef T value_type;

    #ifdef SYSTYPE_MSVC
	#pragma warning( disable : 4284 )
	#endif //SYSTYPE_MSVC

		_list_const_reverse_iterator& operator ++() { _node=_node->_prev; return *this; }
		_list_const_reverse_iterator& operator --() { _node=_node->_next; return *this; }

		_list_const_reverse_iterator operator ++(int) { _list_const_reverse_iterator x(_node); _node=_node->_prev;  return x; }
		_list_const_reverse_iterator operator --(int) { _list_const_reverse_iterator x(_node); _node=_node->_next;  return x; }

		_list_const_reverse_iterator() : _list_iterator_base() {}
		explicit _list_const_reverse_iterator(node* p) : _list_iterator_base(p) {}
	};

    typedef _list_iterator iterator;
	typedef _list_reverse_iterator reverse_iterator;
    typedef _list_const_iterator const_iterator;
    typedef _list_const_reverse_iterator const_reverse_iterator;

protected:
	///
	atomic::basic_pool<node> _npool;

	///
	node _node;

	// prev tail item_0 ---------- item_n head next

protected:

	node* new_node(node *itpos,const T &item)
	{
		node *nn=_npool.pop_new();

		nn->_next=itpos;
		nn->_prev=itpos->_prev;
		new (nn->_item) T(item);

		return nn;
	}

	node* new_node_take(node *itpos,T &item)
	{
		node *nn=_npool.pop_new();

		nn->_next=itpos;
		nn->_prev=itpos->_prev;
		
		new (nn->_item) T();
        coid::queue_helper_trait<T>::take(nn->item(),item);

		return nn;
	}

	void delete_node(node *n)
	{
		n->item().~T();
		_npool.push(n);
	}

	///
	void insert(node* itpos,const T &item)
	{
		node * const nn=new_node(itpos,item);
		nn->_prev->_next=itpos->_prev=nn;
	}

	///
	void insert_take(node* itpos,T &item)
	{
		node * const nn=new_node_take(itpos,item);
		nn->_prev->_next=itpos->_prev=nn;
	}

	///
	T* insert_uninit(node* itpos)
	{
		node *nn=_npool.pop_new();

		nn->_next=itpos;
		nn->_prev=itpos->_prev;
		nn->_prev->_next=itpos->_prev=nn;

		return &nn->item();
	}

public:

	///
	list() : _npool(),_node(false) {}

	///
	~list() {}

	///
	void push_front(const T &item) { insert(_node._next,item); }

	///
	void push_front_take(T &item) { insert_take(_node._next,item); }

	///
	T* push_front_uninit() { return insert_uninit(_node._next); }

	///
	void push_back(const T &item) { insert(&_node,item); }

	///
	void push_back_take(T &item) { insert_take(&_node,item); }

	///
	T* push_back_uninit() { return insert_uninit(&_node); }

    ///
    void insert(_list_iterator_base &it, const T &item) {
        insert(it._node->_next, item);
    }

    ///
	void insert_before(_list_iterator_base &it, const T &item)
	{
        insert(it._node, item);
	}

    ///
	void erase(_list_iterator_base &it)
	{
		if(it._node!=&_node) {
			it._node->_prev->_next=it._node->_next;
			it._node->_next->_prev=it._node->_prev;
			delete_node(it._node);
		}
	}

	///
	bool pop_front(T &item)
	{
		if(!is_empty()) {
	        coid::queue_helper_trait<T>::take(item,front());
			erase(iterator(_node._next));
			return true;
		} 
		return false;
	}

	///
	T* pop_front_ptr()
	{
		if(!is_empty()) {
			T *item=&front();
			erase(iterator(_node._next));
			return item;
		} 
		return 0;
	}

	///
	bool pop_back(T &item)
	{
		if(!is_empty()) {
	        coid::queue_helper_trait<T>::take(item,back());
			erase(iterator(_node._prev));
			return true;
		} 
		return false;
	}

	///
	T* pop_back_ptr()
	{
		if(!is_empty()) {
			T *item=&back();
			erase(iterator(_node._prev));
			return item;
		} 
		return 0;
	}

	///
	void clear()
	{
		node* n=_node._next;
		_node._next=_node._prev=&_node;
		while(n->_next!=&_node) {
			n=n->_next;
			delete_node(n->_prev);
		}
	}

	///
	bool is_empty() const { return _node._next==&_node; }

	T& front() const { DASSERT(!is_empty()); return _node._next->item(); }
	T& back() const { DASSERT(!is_empty()); return _node._prev->item(); }

    iterator begin() { return iterator(_node._next); }
	iterator end() { return iterator(&_node); }

    const_iterator begin() const { return _list_const_iterator(_node._next); }
    const_iterator end() const { return _list_const_iterator(_node); }

    reverse_iterator rbegin() { return reverse_iterator(_node._prev); }
	reverse_iterator rend() { return reverse_iterator(&_node); }

    const_reverse_iterator rbegin() const { return const_reverse_iterator(_node._prev); }
    const_reverse_iterator rend() const { return const_reverse_iterator(_&node); }

    iterator find(const T &item)
    {
        auto i = begin(), e = end();
        for(; *i != item && i != e; ++i);
        return i;
    }

    const_iterator find(const T &item) const
    {
        return find(item);
    }

    void erase(const T &item)
    {
        auto i = find(item);
        if(i != end()) erase(i);
    }
};

} //end of namespace coid

#endif // __COID_LIST_H__
