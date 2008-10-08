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
	struct ptr_t
	{
		explicit ptr_t(node * const p) : _ptr(p) , _tag(0) {}

		ptr_t(node * const p, const int32 t) : _ptr(p), _tag(t) {}

		ptr_t() : _ptr(0) , _tag(0) {}

		void set(node * const p, const int32 t) { _ptr = p; _tag = t; }

		bool operator == (const ptr_t & p)
		{
			return (_ptr == p._ptr) && (_tag == p._tag);
		}

		bool operator != (const ptr_t & p) { return !operator == (p); }

		union {
			struct {
				node * volatile _ptr;
				volatile unsigned int _tag;
			};
			volatile int64 _data;
		};
	};

	typedef ptr_t node_ptr_t;

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
	void fixList(node_ptr_t & tail, node_ptr_t & head)
	{
		node_ptr_t curNode, curNodeNext, nextNodePrev;

		curNode = tail;

		while ((head == _head) && (curNode != head)) {
			curNodeNext = curNode._ptr->_next;

			if (curNodeNext._tag != curNode._tag)
				return;

			nextNodePrev = curNodeNext._ptr->_prev;
			if (nextNodePrev != node_ptr_t(curNode._ptr, curNode._tag - 1))
				curNodeNext._ptr->_prev.set(curNode._ptr, curNode._tag - 1);

			curNode.set(curNodeNext._ptr, curNode._tag - 1);
		};
	}

public:
	//!	constructor
	queue() : _tail(new node_t(true)) , _head(_tail._ptr) , _dpool() {}

	//!	destructor (do not clear queue for now)
	~queue() 
	{
		node_t * p;
		while ((p = _dpool.pop()) != 0) delete p;
	} 

	//! return item from the head of the queue
	void push(T * const item)
	{
		node_ptr_t tail;

		node_t * const newnode = item;

		newnode->_prev.set(0, 0);

		for (;;) {
			tail = _tail;
			newnode->_next.set(tail._ptr, tail._tag + 1);
			if (b_cas(&_tail._data, node_ptr_t(newnode, tail._tag + 1)._data, tail._data)) {
				tail._ptr->_prev.set(newnode, tail._tag);
				return;
			}
		}
	}

	//! return item from the head of the queue
	T * pop()
	{
		node_ptr_t head, tail;
		node_t * dummy;

		for (;;) {
			head = _head;
			tail = _tail;
			if (head == _head) {
				if (!head._ptr->_dummy) {
					if (tail != head) {
						if (head._ptr->_prev._tag != head._tag) {
							fixList(tail, head);
							continue;
						}
					} 
					else {
						dummy = _dpool.pop();
						if (dummy == 0) dummy = new node_t(true);
						dummy->_next.set(tail._ptr, tail._tag + 1);
						if (b_cas(&_tail._data, node_ptr_t(dummy, tail._tag + 1)._data, tail._data))
							head._ptr->_prev.set(dummy, tail._tag);
						else
							_dpool.push(dummy);
						continue;
					}
					if (b_cas(&_head._data, node_ptr_t(head._ptr->_prev._ptr, head._tag + 1)._data, head._data))
						return static_cast<T*>(head._ptr);
				} 
				else {
					if (tail._ptr == head._ptr)
						return 0;
					else {	
						if (head._ptr->_prev._tag != head._tag) {
							fixList(tail, head);
							continue;
						}
						if (b_cas(&_head._data, node_ptr_t(head._ptr->_prev._ptr, head._tag + 1)._data, head._data))
							_dpool.push(head._ptr);
					}
				} 
			}
		}
	}
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_QUEUE_H__
