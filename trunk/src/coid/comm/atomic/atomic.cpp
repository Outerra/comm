#include "atomic.h"
#include "queue.h"

// just for check during compile time
struct node_allocator 
{
	template<class T>
	static T* alloc()
	{
		return new T();
	}
};

atomic::queue<int, node_allocator> q;