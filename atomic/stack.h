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
		node() : m_pNext(0) {}

		node * volatile m_pNext;
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
				uint _pops;
			};
			__int64 _data;
		};
	};

	ptr_t _head;

public:
	stack()	: _head() {}

	void push(T * pNewItem)
	{
		node_t * const pNewNode = pNewItem;

		for (;;) {
			pNewNode->m_pNext = _head._ptr;
			if (b_cas_ptr(reinterpret_cast<void**>(&_head._ptr), pNewNode, pNewNode->m_pNext))
				break;
		}
	}

	T * pop()
	{
		for (;;) {
			const ptr_t head = _head;

			if (head._ptr == 0) return 0;

			const ptr_t next(head._ptr->m_pNext, head._pops + 1);

			if (b_cas(&_head._data, next._data, head._data)) {
				return static_cast<T*>(head._ptr);
			}
		}
	}
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_STACK_H__