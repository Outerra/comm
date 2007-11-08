#ifndef __COMM_ATOMIC_QUEUE_H__
#define __COMM_ATOMIC_QUEUE_H__

#include "atomic.h"

namespace atomic {

//! double linked list FIFO queue
template <class T, class A>
class queue
{
public:
	//! helper pointer with counter
	template <class T>
	struct ptr_t
	{
		ptr_t(T * const p)
			: ptr(p)
			, tag(0) {}

		ptr_t(T * const p, const unsigned int t)
			: ptr(p)
			, tag(t) {}

		ptr_t()
			: ptr(0)
			, tag(0) {}

		bool operator == (const ptr_t & p)
		{
			return (ptr == p.ptr) && (tag == p.tag);
		}

		bool operator != (const ptr_t & p)
		{
			return !operator == (p);
		}

		T * volatile ptr;
		volatile unsigned int tag;
	};

	//!
	template<class T>
	struct node
	{
		node()
			: m_pNext(0)
			, m_pPrev(0)
			, m_bDummy(false) {}

		ptr_t<T> m_pNext;
		ptr_t<T> m_pPrev;
		bool m_bDummy;
	};

	typedef ptr_t<T> node_ptr_t;
	typedef node<T> node_t;

public:
	//! 
	node_ptr_t m_pTail;

	//!
	node_ptr_t m_pHead;

	//!
	volatile unsigned int m_uiPushCollisions;

	//!
	volatile unsigned int m_uiPopCollisions;

	//!
	volatile unsigned int m_uiFixes;

public:
	//!
	queue() throw(...)
		: m_pTail(A::alloc<T>())
		, m_pHead(m_pTail.ptr)
		, m_uiPushCollisions(0L)
		, m_uiPopCollisions(0L)
		, m_uiFixes(0L)
	{
		m_pHead.ptr->m_bDummy = true;
	}

	~queue() throw() {} 

	void push(T * const pNewNode)
	{
		node_ptr_t tail;

		pNewNode->m_pPrev = node_ptr_t(0, 0);

		for (;;) {
			tail = m_pTail;
			pNewNode->m_pNext = node_ptr_t(tail.ptr, tail.tag + 1);
			if (cas<T*>(
				&m_pTail, 
				pNewNode,
				tail.tag + 1,
				tail.ptr,
				tail.tag)) {
					tail.ptr->m_pPrev = node_ptr_t(pNewNode, tail.tag);
					return;
			}
		}
	}

	void fixList(node_ptr_t tail, node_ptr_t head)
	{
		node_ptr_t curNode, curNodeNext, nextNodePrev;

		curNode = tail;

		while ((head == m_pHead) && (curNode != head)) {
			curNodeNext = curNode.ptr->m_pNext;

			if (curNodeNext.tag != curNode.tag)
				return;

			nextNodePrev = curNodeNext.ptr->m_pPrev;
			if (nextNodePrev != node_ptr_t(curNode.ptr, curNode.tag - 1))
				curNodeNext.ptr->m_pPrev = node_ptr_t(curNode.ptr, curNode.tag - 1);

			curNode = node_ptr_t(curNodeNext.ptr, curNode.tag - 1);
		};
	}

	T * pop()
	{
		node_ptr_t head, tail;
		T * pDummy;

		for (;;) {
			head = m_pHead;
			tail = m_pTail;
			if (head == m_pHead) {
				if (!head.ptr->m_bDummy) {
					if (tail != head) {
						if (head.ptr->m_pPrev.tag != head.tag) {
							fixList(tail, head);
							continue;
						}
					} 
					else {
						pDummy = A::alloc<T>();
						pDummy->m_bDummy = true;
						pDummy->m_pNext = node_ptr_t(tail.ptr, tail.tag + 1);
						if (cas<T*>(
							&m_pTail, 
							pDummy, 
							tail.tag + 1, 
							tail.ptr, 
							tail.tag))
							head.ptr->m_pPrev = pDummy;
						else
							A::free(pDummy);
						continue;
					}
					if (cas<T*>(
						&m_pHead, 
						head.ptr->m_pPrev.ptr,
						head.tag + 1,
						head.ptr, 
						head.tag))
						return head.ptr;
				} 
				else {
					if (tail.ptr == head.ptr)
						return 0;
					else {	
						if (head.ptr->m_pPrev.tag != head.tag) {
							fixList(tail, head);
							continue;
						}
						cas<T*>(
							&m_pHead, 
							head.ptr->m_pPrev.ptr, 
							head.tag + 1, 
							head.ptr, 
							head.tag);
						A::free(head.ptr);
					}
				} 
			}
		}
	};
};

} // end of namespace atomic

#endif // __COMM_ATOMIC_QUEUE_H__