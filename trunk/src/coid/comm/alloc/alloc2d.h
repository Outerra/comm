#pragma once
#ifndef __OUTERRA_ENG3_VT_ALLOC_H__
#define __OUTERRA_ENG3_VT_ALLOC_H__

#include <comm/commtypes.h>
#include <comm/atomic/basic_pool.h>

#include <ot/glm/glm_ext.h>

COID_NAMESPACE_BEGIN

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<typename T>
class alloc_2d
{
public:

    class node;
    typedef T type_t;

private:

    class node_pool
	    : public atomic::basic_pool<node>
    {
    public:
	    node* new_node(const type_t &pos, const type_t &size, node * const parent)
        {
	        node * const n = pop();
	        return n
                ? new (n) node(pos, size, parent)
                : new node(pos, size, parent);
        }
	
        void delete_node(node * const n)
        {
    	    push(n);
        }
    };

public:

    class node 
    {
        friend alloc_2d;
        friend atomic::basic_pool<node>;

    private:
        union {
    	    node *_next_basic_pool;
            struct {
	            type_t _pos;
	            type_t _size;
	            node *_child[2];
	            node *_parent;
            };
        };
    
    public:
	    void* _id;

        const type_t& get_pos() const { return _pos; }
        const type_t& get_size() const { return _size; }

    protected:
	    node(const type_t &pos, const type_t &size, node * const parent)
		    : _pos(pos)
		    , _size(size)
		    , _id(0)
		    , _parent(parent)
        {
		    _child[0] = _child[1] = 0;
	    }

	    bool is_leaf() const { return _child[0] == 0; }

	    node* insert(const type_t &size, node_pool &pool)
        {
		    if(!is_leaf()) {
			    node * const n = _child[0]->insert(size, pool);
			    return n ? n : _child[1]->insert(size, pool);
		    }
		    else {
			    if(_id != 0 || size.x > _size.x || size.y > _size.y)
                    return 0;

			    if(size.x == _size.x && size.y == _size.y)
				    return this;
		
			    split(size, pool);

			    return _child[0]->insert(size, pool);
		    }
	    }

	    void divide(const type_t &size, node_pool &pool, int &n)
        {
		    if(!is_leaf()) {
			    _child[0]->divide(size, pool, n);
			    _child[1]->divide(size, pool, n);
		    }
		    else {
			    if(_id != 0 || size.x > _size.x || size.y > _size.y) return;

			    if(size.x == _size.x && size.y == _size.y) { ++n; return; }

			    split(size, pool);

			    _child[0]->divide(size, pool, n);
			    _child[1]->divide(size, pool, n);
		    }
	    }

	    void split(const T &size, node_pool &pool)
        {
		    const type_t d = _size - size;

		    if(d.x > d.y) {
			    _child[0] = pool.new_node(
				    _pos,
				    type_t(size.x, _size.y),
				    this);
			    _child[1] = pool.new_node(
				    type_t(_pos.x + size.x, _pos.y),
				    type_t(_size.x - size.x, _size.y),
				    this);
		    }
		    else {
			    _child[0] = pool.new_node(
				    _pos,
				    type_t(_size.x, size.y),
				    this);
			    _child[1] = pool.new_node(
				    type_t(_pos.x, _pos.y + size.y),
				    type_t(_size.x, _size.y - size.y),
				    this);
		    }
	    }

	    bool merge_up(node_pool &pool)
        {
		    if(_child[0] != 0) {
			    if(_child[0]->merge_up(pool) && _child[1]->merge_up(pool)) {
				    pool.delete_node(_child[0]);
				    pool.delete_node(_child[1]);
				    _child[0] = _child[1] = 0;
			    }
			    else {
				    return false;
			    }
		    }
		
		    return _id == 0 && _child[0] == 0 && _child[1] == 0;
	    }
    };

private:

    node_pool _node_pool;
	node * const _root;
	const int _initial_split_size;

public:

	alloc_2d(const int size, const int initial_split_size)
		: _node_pool()
		, _root(_node_pool.new_node(type_t(0), type_t(size), 0))
		, _initial_split_size(initial_split_size)
	{
		if(initial_split_size > 0) {
            int n = 0;
		    _root->divide(type_t(initial_split_size), _node_pool, n);
        }
	}

	~alloc_2d() {}

	/// return position in virtual 2D space
	node* alloc_space(const T &size)
    { 
		return _root->insert(size, _node_pool); 
	}

	///
	void free_space(node * n)
    {
		n->_id = 0;
		while(n->_size.x <= _initial_split_size && n->merge_up(_node_pool))
            n = n->_parent;
	}
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

COID_NAMESPACE_END

#endif // __OUTERRA_ENG3_VT_ALLOC_H__
