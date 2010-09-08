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
 * Ladislav Hrabcak
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
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
#ifndef __COMM_ATOMIC_QUEUE_H__
#define __COMM_ATOMIC_QUEUE_H__

#include "atomic.h"
#include "stack.h"
#include "basic_pool.h"
#include "queue_base.h"

/*
namespace atomic {

struct queue_node;

//! helper pointer with tag
struct queue_ptr
{
	explicit queue_ptr(queue_node * const p) : _ptr(p) , _tag(0) {}

	queue_ptr(queue_node * const p, const int32 t) : _ptr(p), _tag(t) {}

	queue_ptr() : _ptr(0) , _tag(0) {}

	void set(queue_node * const p, const int32 t) { _ptr=p; _tag=t; }

	bool operator == (const queue_ptr & p) { return (_ptr==p._ptr) && (_tag==p._tag); }

	bool operator != (const queue_ptr & p) { return !operator == (p); }

	union {
		struct {
			queue_node * volatile _ptr;
			volatile unsigned int _tag;
		};
		volatile int64 _data;
	};
};

struct queue_node
{
	queue_node() : _next(0) , _prev(0) , _dummy(false) {}

	queue_ptr _next;
	queue_ptr _prev;
	bool _dummy;

protected:
	queue_node(const bool) : _next(0) , _prev(0) , _dummy(true) {}
};

//! atomic double linked list FIFO queue
template <class T>
class queue_ng
{
public:
	struct dummy_node 
		: public queue_node
		, public stack_node 
	{
		dummy_node() : queue_node(true) {}
	};

public:
	//! last pushed item
	queue_ptr _tail;

	//! first pushed item
	queue_ptr _head;

protected:
	stack<dummy_node> _dpool;

protected:

	//! optimistic fix called when prev pointer is not set
	void fixList(queue_ptr & tail, queue_ptr & head)
	{
		queue_ptr curNode, curNodeNext, nextNodePrev;

		curNode = tail;

		while ((head == _head) && (curNode != head)) {
			curNodeNext = curNode._ptr->_next;

			if (curNodeNext._tag != curNode._tag)
				return;

			nextNodePrev = curNodeNext._ptr->_prev;
			if (nextNodePrev != queue_ptr(curNode._ptr, curNode._tag - 1))
				curNodeNext._ptr->_prev.set(curNode._ptr, curNode._tag - 1);

			curNode.set(curNodeNext._ptr, curNode._tag - 1);
		};
	}

public:
	//!	constructor
	queue_ng() : _tail(new dummy_node()) , _head(_tail._ptr) , _dpool() {}

	//!	destructor (do not clear queue for now)
	~queue_ng() { queue_node* p; while ((p = _dpool.pop()) != 0) delete p; } 

protected:
	//template<class T>
	void push(queue_node* newnode)
	{
		queue_ptr tail;

		newnode->_prev.set(0, 0);

		for (;;) {
			tail = _tail;
			newnode->_next.set(tail._ptr, tail._tag + 1);
			if (b_cas(&_tail._data, queue_ptr(newnode, tail._tag + 1)._data, tail._data)) {
				tail._ptr->_prev.set(newnode, tail._tag);
				return;
			}
		}
	}

public:
	/// enqueue item
	template<class T2>
	void push(ref<T2>& item) { push( static_cast<policy_pooled<T>*>(item.add_ref_copy())); }

	/// same as previous push but TAKE given ref (no addref is needed) 
	template<class T2>
	void push_take(ref<T2>& item) { push( static_cast<policy_pooled<T>*>(item.give_me()) ); }

	/// pop item from queue returns false when queue is empty
	bool pop(ref<T> &item)
	{
		queue_ptr head, tail;
		dummy_node * dummy;

		for( ;; ) {
			head=_head;
			tail=_tail;
			if( head==_head ) {
				if( !head._ptr->_dummy ) {
					if( tail!=head ) {
						if( head._ptr->_prev._tag!=head._tag ) {
							fixList(tail,head);
							continue;
						}
					} 
					else {
						dummy=_dpool.pop();
						if( dummy==0 ) dummy=new dummy_node();
						dummy->_next.set( tail._ptr,tail._tag+1 );
						if( b_cas( &_tail._data,queue_ptr( dummy,tail._tag+1 )._data,tail._data ) )
							head._ptr->_prev.set(dummy,tail._tag);
						else
							_dpool.push(dummy);
						continue;
					}
					if( b_cas( &_head._data,queue_ptr( head._ptr->_prev._ptr,head._tag+1 )._data,head._data ) ) {
						item.create( static_cast<policy_pooled<T>*>(head._ptr) );
						return true;
					}
				} 
				else {
					if( tail._ptr==head._ptr )
						return false;
					else {	
						if( head._ptr->_prev._tag!=head._tag ) {
							fixList( tail,head );
							continue;
						}
						if( b_cas( &_head._data,queue_ptr(head._ptr->_prev._ptr,head._tag+1)._data,head._data ) )
							_dpool.push( static_cast<dummy_node*>(head._ptr) );
					}
				} 
			}
		}
	}
};

} // end of namespace atomic
*/
#endif // __COMM_ATOMIC_QUEUE_H__
