#ifndef __COMM_ATOMIC_STACK_H__
#define __COMM_ATOMIC_STACK_H__

#include "atomic.h"

namespace atomic {

template <class T, class S = void>
class stack
{
public:
	//!
	template<class T>
	struct node
	{
		node() : _nexts(0) {}

		node * _nexts;
	};

	typedef node<T> node_t;

private:
	struct ptr_t
	{
		ptr_t() : _ptr(0), _pops(0) {}

		ptr_t(node_t * const ptr, const uint pops)
			: _ptr(ptr), _pops(pops) {}

		union {
			struct {
				node_t * _ptr;
				volatile uint _pops;
			};
			__int64 _data;
		};
	};

	ptr_t _head;

public:
	stack()	: _head() {}

	void push(T * item)
	{
		node_t * const newnode = item;

		for (;;) {
			newnode->_nexts = _head._ptr;
			if (b_cas_ptr(reinterpret_cast<void**>(&_head._ptr), newnode, newnode->_nexts))
				break;
		}
	}

	T * pop()
	{
		for (;;) {
			const ptr_t head = _head;

			if (head._ptr == 0) return 0;

			const ptr_t next(head._ptr->_nexts, head._pops + 1);

			if (b_cas(&_head._data, next._data, head._data)) {
				return static_cast<T*>(head._ptr);
			}
		}
	}
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_STACK_H__