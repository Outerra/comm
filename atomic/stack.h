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

		node * m_pNext;
	};

	typedef node<T> node_t;

private:
	node_t * volatile m_pHead;

	volatile uint m_nPops;

public:
	stack()	: m_pHead(0), m_nPops(0) {}

	void push(T * pNewItem)
	{
		node_t * const pNewNode = pNewItem;

		for (;;) {
			pNewNode->m_pNext = m_pHead;
			if (cas<node_t*>(&m_pHead, pNewNode, pNewNode->m_pNext))
				break;
		}
	}

	T * pop()
	{
		for (;;) {
			node_t * pHead = m_pHead;
			uint nPops = m_nPops;

			if (pHead == 0) return 0;

			node_t * pNext = pHead->m_pNext;

			if (cas<node_t*>(&m_pHead, pNext, nPops + 1, pHead, nPops)) {
				return static_cast<T*>(pHead);
			}
		}
	}
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_STACK_H__