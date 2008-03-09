#ifndef __COMM_ATOMIC_QUEUE_H__
#define __COMM_ATOMIC_QUEUE_H__

#include "atomic.h"
#include "stack.h"

namespace atomic {

//! atomic double linked list FIFO queue
template <class T>
class queue
{
public:
	struct node;

	//! helper pointer with tag
	template<class T>
	struct ptr_t
	{
		explicit ptr_t(node * const p) : ptr(p) , tag(0) {}

		ptr_t(node * const p, const unsigned int t) : ptr(p), tag(t) {}

		ptr_t() : ptr(0) , tag(0) {}

		bool operator == (const ptr_t & p)
		{
			return (ptr == p.ptr) && (tag == p.tag);
		}

		bool operator != (const ptr_t & p)
		{
			return !operator == (p);
		}

		union {
			struct {
				node * ptr;
				volatile unsigned int tag;
			};
			__int64 data;
		};
	};

	typedef ptr_t<T> node_ptr_t;

public:

	struct node : public stack<node>::node_t
	{
		node() : _next(0) , _prev(0) , _dummy(false) {}
		node(const bool) : _next(0) , _prev(0) , _dummy(true) {}

		node_ptr_t _next;
		node_ptr_t _prev;
		bool _dummy;
	};

	typedef node node_t;

public:
	//! last pushed item
	node_ptr_t _tail;

	//! first pushed item
	node_ptr_t _head;

protected:
	stack<node> _dpool;

protected:

	//! optimistic fix called when prev pointer is not set
	void fixList(node_ptr_t tail, node_ptr_t head)
	{
		node_ptr_t curNode, curNodeNext, nextNodePrev;

		curNode = tail;

		while ((head == _head) && (curNode != head)) {
			curNodeNext = curNode.ptr->_next;

			if (curNodeNext.tag != curNode.tag)
				return;

			nextNodePrev = curNodeNext.ptr->_prev;
			if (nextNodePrev != node_ptr_t(curNode.ptr, curNode.tag - 1))
				curNodeNext.ptr->_prev = node_ptr_t(curNode.ptr, curNode.tag - 1);

			curNode = node_ptr_t(curNodeNext.ptr, curNode.tag - 1);
		};
	}

public:
	//!	constructor
	queue() throw(...)
		: _tail(new node_t())
		, _head(_tail.ptr)
		, _dpool() {}

	//!	destructor (do not clear queue for now)
	~queue() throw() 
	{
		node_t * p;
		while ((p = _dpool.pop()) != 0) delete p;
	} 

	//! return item from the head of the queue
	void push(T * const item)
	{
		node_ptr_t tail;

		node_t * const newnode = item;

		newnode->_prev = node_ptr_t(0, 0);

		for (;;) {
			tail = _tail;
			newnode->_next = node_ptr_t(tail.ptr, tail.tag + 1);
			if (b_cas(&_tail.data, node_ptr_t(newnode, tail.tag + 1).data, tail.data)) {
				tail.ptr->_prev = node_ptr_t(newnode, tail.tag);
				return;
			}
		}
	}

	//! return item from the head of the queue
	T * pop()
	{
		node_ptr_t head, tail;
		node_t * pDummy;

		for (;;) {
			head = _head;
			tail = _tail;
			if (head == _head) {
				if (!head.ptr->_dummy) {
					if (tail != head) {
						if (head.ptr->_prev.tag != head.tag) {
							fixList(tail, head);
							continue;
						}
					} 
					else {
						pDummy = _dpool.pop();
						if (pDummy == 0)
							pDummy = new node_t();
						pDummy->_dummy = true;
						pDummy->_next = node_ptr_t(tail.ptr, tail.tag + 1);
						if (b_cas(&_tail.data, node_ptr_t(pDummy, tail.tag + 1).data, tail.data))
							head.ptr->_prev = node_ptr_t(pDummy, tail.tag);
						else
							_dpool.push(head.ptr);
						continue;
					}
					if (b_cas(&_head.data, node_ptr_t(head.ptr->_prev.ptr, head.tag + 1).data, head.data))
						return static_cast<T*>(head.ptr);
				} 
				else {
					if (tail.ptr == head.ptr)
						return 0;
					else {	
						if (head.ptr->_prev.tag != head.tag) {
							fixList(tail, head);
							continue;
						}
						if (b_cas(&_head.data, node_ptr_t(head.ptr->_prev.ptr, head.tag + 1).data, head.data))
							_dpool.push(head.ptr);
					}
				} 
			}
		}
	}
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_QUEUE_H__