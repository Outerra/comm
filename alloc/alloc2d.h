#pragma once
#ifndef __OUTERRA_ENG3_VT_ALLOC_H__
#define __OUTERRA_ENG3_VT_ALLOC_H__

#include <comm/commtypes.h>
#include <comm/alloc/slotalloc_bmp.h>

#include <ot/glm/glm_ext.h>

COID_NAMESPACE_BEGIN

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

template<typename T, typename D>
class alloc_2d
{
public:

    class node;
    typedef T type_t;

private:

    class node_pool
    {
    private:
        slotalloc_bmp<node> _nodes;

    public:
	    uint new_node(const type_t &pos, const type_t &size, const uint parent)
        {
            node * const n = new (_nodes.add_uninit()) node(pos, size, parent);
            const uint id = _nodes.get_item_id(n);
            return id;
        }
	
        void delete_node(const uint id)
        {
            _nodes.del(id);
        }

        node* ptr(const uint id) { return _nodes.get_item(id); }

        uint id(const node* const ptr) const { return _nodes.get_item_id(ptr); }

        uint next(const uint id) const { return _nodes.next(id); }
    };

public:

    class node 
    {
        friend alloc_2d;

    private:
	    type_t _pos;
	    type_t _size;
	    uint2 _child;
	    uint _parent;
        uint _flags;
    
    public:
        D _data;

        const type_t& get_pos() const { return _pos; }
        const type_t& get_size() const { return _size; }
        bool is_leaf() const { return _child[0] == -1 && _child[1] == -1; }
        bool has_data() const { return _flags != 0; }

    protected:

	    node(const type_t &pos, const type_t &size, const uint parent)
		    : _pos(pos)
		    , _size(size)
            , _child(-1)
		    , _parent(parent)
            , _flags(0)
            , _data()
        {}

	    static uint insert(const uint id, const type_t &size, node_pool &pool)
        {
            node * const n = pool.ptr(id);
            uint2 _child = n->_child;
            const T _size = n->_size;
            const uint _flags = n->_flags;

            if (!pool.ptr(id)->is_leaf()) {
                uint n = insert(_child[0], size, pool);
                if (n == -1)
                    n = insert(_child[1], size, pool);
                return n;
		    }
		    else {
                if (_flags != 0 || size.x > _size.x || size.y > _size.y)
                    return -1;

                if (size.x == _size.x && size.y == _size.y) {
                    pool.ptr(id)->_flags = 1;
                    return id;
                }
		
			    _child = split(id, size, pool);

                return insert(_child[0], size, pool);
		    }
	    }

	    static void divide(const uint id, const type_t &size, node_pool &pool, int &depth)
        {
            node * n = pool.ptr(id);
            const T _size = n->_size;
            uint2 _child = n->_child;

            if (!n->is_leaf()) {
                divide(_child[0], size, pool, depth);
                divide(_child[1], size, pool, depth);
		    }
		    else {
                if (n->_flags != 0 || size.x > _size.x || size.y > _size.y)
                    return;

                if (size.x == _size.x && size.y == _size.y) {
			        ++depth;
                    return;
			    }

			    _child = split(id, size, pool);

                divide(_child[0], size, pool, depth);
                divide(_child[1], size, pool, depth);
		    }
	    }

	    static uint2 split(const uint id, const T &size, node_pool &pool)
        {
            node * n = pool.ptr(id);
            const T _pos = n->_pos;
            const T _size = n->_size;
            const type_t d = _size - size;

            uint2 child;
            if (d.x > d.y) {
			    child[0] = pool.new_node(_pos, type_t(size.x, _size.y), id);
			    child[1] = pool.new_node( type_t(_pos.x + size.x, _pos.y), type_t(_size.x - size.x, _size.y), id);
		    }
		    else {
			    child[0] = pool.new_node(_pos, type_t(_size.x, size.y), id);
			    child[1] = pool.new_node(type_t(_pos.x, _pos.y + size.y), type_t(_size.x, _size.y - size.y), id);
		    }

            n = pool.ptr(id);

            return n->_child = child;
	    }
    };

private:

    node_pool _node_pool;
	uint const _root;
	const int _initial_split_size;

public:

	alloc_2d(const int size, const int initial_split_size)
		: _node_pool()
		, _root(_node_pool.new_node(type_t(0), type_t(size), -1))
		, _initial_split_size(initial_split_size)
	{
		if(initial_split_size > 0) {
            int depth = 0;
            node::divide(_root, type_t(initial_split_size), _node_pool, depth);
        }
	}

	~alloc_2d() {}

	/// return position in virtual 2D space
	uint alloc_space(const T &size)
    { 
        const uint id = node::insert(_root, size, _node_pool);

        DASSERT(_node_pool.ptr(id)->has_data());

        return id;
	}

	///
	void free_space(uint id, const bool leaf = true)
    {
        node * const n = _node_pool.ptr(id);

        const uint parent_id = n->_parent;

        if (parent_id != -1) {
            node * const parent = _node_pool.ptr(n->_parent);
            node * const child0 = _node_pool.ptr(parent->_child.x);
            node * const child1 = _node_pool.ptr(parent->_child.y);

            if (leaf)
                n->_flags = 0; // clean data flag

            if (child0->is_leaf() && child1->is_leaf()
                && !child0->has_data() && !child1->has_data()) {
                _node_pool.delete_node(parent->_child.x);
                _node_pool.delete_node(parent->_child.y);
                parent->_child.x = parent->_child.y = -1;
                free_space(parent_id, false);
            }
        }
	}

    ///
    D* get_ptr(const uint id) { return &_node_pool.ptr(id)->_data; }

    ///
    uint get_next(const uint id) const { return _node_pool.next(id); }

    const node* get_node_ptr(uint id) { return _node_pool.ptr(id); }
};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

COID_NAMESPACE_END

#endif // __OUTERRA_ENG3_VT_ALLOC_H__
