#ifndef __COMM_ATOMIC_STACK_H__
#define __COMM_ATOMIC_STACK_H__

#include "atomic.h"

namespace atomic {

template <class T, class S>
class stack
{
public:
	//!
	template<class T>
	struct node
	{
		node() : m_pNext(0) {}

		T * m_pNext;
	};

	typedef node<T> node_t;

private:
	T * volatile m_pHead;

	volatile uint m_nPops;

public:
	stack()	: m_pHead(0), m_nPops(0) {}

	void push(T * pNewNode)
	{
		for (;;) {
			S::get(pNewNode).m_pNext = m_pHead;
			if (cas<T*>(&m_pHead, pNewNode, S::get(pNewNode).m_pNext))
				break;
		}
	}

	T * pop()
	{
		for (;;) {
			T * pHead = m_pHead;
			uint nPops = m_nPops;

			if (pHead == 0) return 0;

			T * pNext = S::get(pHead).m_pNext;

			if (cas<T*>(&m_pHead, pNext, nPops + 1, pHead, nPops)) 
				return pHead;
		}
	}
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_STACK_H__