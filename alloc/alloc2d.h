#pragma once
#ifndef __OUTERRA_ENG3_VT_ALLOC_H__
#define __OUTERRA_ENG3_VT_ALLOC_H__

#include <comm/commtypes.h>
#include <comm/alloc/slotalloc.h>

COID_NAMESPACE_BEGIN

/// @brief Allocator that manages 2D regions associated with user data.
/// @tparam vec2d_type Type used for size and position.  
///         Must provide accessible members `x` and `y`,  
///         a constructor of the form `vec2d_type(value_type x, value_type y)`,  
///         and a typedef `vec2d_type::value_type` representing the coordinate type.  
/// @tparam data_type Type of data associated with each allocated region.
template<typename vec2d_type, typename data_type>
class alloc_2d
{
public: // internal definitions only

    class handle
    {
        friend alloc_2d;
    public: // methods only
        handle() = default;
        bool is_valid() const { return _value != UINT32_MAX; }
    protected: // methods only
        handle(uint32 value) : _value(value) {}
    private: // members only
        uint32 _value = UINT32_MAX;
    };

    class node;
    using vec2d_t = vec2d_type;
    using value_type = vec2d_type::value_type;

public: // method only

    /// @brief Constructor for alloc_2d.
    /// @param size Length of a side of the rectangular allocation area.
    /// @param initial_split_size Size of the initial subdivision of the area. (TODO: clarify)
    alloc_2d(const value_type size, const value_type initial_split_size)
        : _node_pool()
        , _root(_node_pool.new_node(vec2d_type(0, 0), vec2d_type(size), handle()))
        , _initial_split_size(initial_split_size)
        , _size(size)
    {
        if (_initial_split_size > 0) {
            int depth = 0;
            node::divide(_root, vec2d_type(_initial_split_size, _initial_split_size), _node_pool, depth);
        }
    }

    ~alloc_2d() {}

    /// @brief Allocates a 2D region of the given size.
    /// @param size Size of the region to allocate.
    /// @return A handle to the allocated region, or an invalid handle if allocation fails.
    handle alloc(const vec2d_type& size)
    {
        const handle id = node::insert(_root, size, _node_pool);
        return id;
    }

    /// @brief Frees a previously allocated region.
    /// @param id Handle of the region to free.
    void free(handle id)
    {
        free_internal(id, true);
    }

    /// @brief Retrieves the data associated with a region.
    /// @param id Handle of the allocated region.
    /// @return The data associated with the region.
    const data_type& get_data(const handle id) { return _node_pool.ptr(id)->_data; }

    /// @brief Sets the data associated with an allocated region.
    /// @param id Handle of the allocated region.
    /// @param value Data value to associate with the region.
    void set_data(const handle id, data_type value) { _node_pool.ptr(id)->_data = value; }


    /// @brief Retrieves the position of an allocated region.
    /// @param id Handle of the allocated region.
    /// @return The position of the region.
    const vec2d_type& get_position(handle id) const { return _node_pool.ptr(id)->get_pos(); };

    /// @brief Retrieves the size of an allocated region.
    /// @param id Handle of the allocated region.
    /// @return The size of the region.
    const vec2d_type& get_size(handle id) const { return _node_pool.ptr(id)->get_size(); };

    /// @brief Frees an allocated region along with its associated data.
    void reset()
    {
        _node_pool.clear();
        _root = _node_pool.new_node(vec2d_type(0, 0), vec2d_type(_size), handle());
    }

protected: // methods only 
    void free_internal(handle id, bool is_leaf)
    {
        node* const n = _node_pool.ptr(id);

        const handle parent_id = n->_parent;

        if (parent_id.is_valid()) {
            node* const parent = _node_pool.ptr(n->_parent);
            const node* const child0 = _node_pool.ptr(parent->_child.x);
            const node* const child1 = _node_pool.ptr(parent->_child.y);

            if (is_leaf)
                n->_flags = 0; // clean data flag

            if (child0->can_release() && child1->can_release())
            {
                _node_pool.delete_node(parent->_child.x);
                _node_pool.delete_node(parent->_child.y);
                parent->_child.x = parent->_child.y = handle();
                free_internal(parent_id, false);
            }
        }
    }

private: // internal definitions only

    struct children
    {
        handle x, y;
    };

    class node_pool
    {
    private:
        slotalloc<node> _nodes;

    public:
        handle new_node(const vec2d_type& pos, const vec2d_type& size, const handle parent)
        {
            node* const n = new (_nodes.add_uninit()) node(pos, size, parent);
            const handle id(static_cast<uint32>(_nodes.get_item_id(n)));
            return id;
        }

        void delete_node(const handle id) { _nodes.del_item(id._value); }

        node* ptr(const handle id) { return _nodes.get_item(id._value); }
        const node* ptr(const handle id) const { return _nodes.get_item(id._value); }


        handle id(const node* const ptr) const { return _nodes.get_item_id(ptr); }

        void clear() { _nodes.reset(); }
    };

    class node
    {
        friend alloc_2d;

    private:
        vec2d_type _pos;
        vec2d_type _size;
        children _child;
        handle _parent;
        uint _flags;

    public:
        data_type _data;

        const vec2d_type& get_pos() const { return _pos; }
        const vec2d_type& get_size() const { return _size; }
        bool is_leaf() const { return !_child.x.is_valid() && !_child.y.is_valid(); }
        bool has_data() const { return _flags != 0; }
        bool can_release() const { return is_leaf() && !has_data(); }

    protected:

        node(const vec2d_type& pos, const vec2d_type& size, const handle parent)
            : _pos(pos)
            , _size(size)
            , _child()
            , _parent(parent)
            , _flags(0)
            , _data()
        {}

        const node* get_node_ptr(handle id) { return _node_pool.ptr(id); }

        static handle insert(const handle id, const vec2d_type& size, node_pool& pool)
        {
            node* const n = pool.ptr(id);
            children _child = n->_child;
            const vec2d_type _size = n->_size;
            const uint _flags = n->_flags;

            if (!pool.ptr(id)->is_leaf()) {
                handle n = insert(_child.x, size, pool);
                if (!n.is_valid())
                    n = insert(_child.y, size, pool);
                return n;
            }
            else {
                if (_flags != 0 || size.x > _size.x || size.y > _size.y)
                    return handle();

                if (size.x == _size.x && size.y == _size.y) {
                    pool.ptr(id)->_flags = 1;
                    return id;
                }

                _child = split(id, size, pool);

                return insert(_child.x, size, pool);
            }
        }

        static void divide(const handle id, const vec2d_type& size, node_pool& pool, int& depth)
        {
            node* n = pool.ptr(id);
            const vec2d_type _size = n->_size;
            children _child = n->_child;

            if (!n->is_leaf()) {
                divide(_child.x, size, pool, depth);
                divide(_child.y, size, pool, depth);
            }
            else {
                if (n->_flags != 0 || size.x > _size.x || size.y > _size.y)
                    return;

                if (size.x == _size.x && size.y == _size.y) {
                    ++depth;
                    return;
                }

                _child = split(id, size, pool);

                divide(_child.x, size, pool, depth);
                divide(_child.y, size, pool, depth);
            }
        }

        static children split(const handle id, const vec2d_type& size, node_pool& pool)
        {
            node* n = pool.ptr(id);
            const vec2d_type _pos = n->_pos;
            const vec2d_type _size = n->_size;
            const vec2d_type d = _size - size;

            children child;
            if (d.x > d.y) {
                child.x = pool.new_node(_pos, vec2d_type(size.x, _size.y), id);
                child.y = pool.new_node(vec2d_type(_pos.x + size.x, _pos.y), vec2d_type(_size.x - size.x, _size.y), id);
            }
            else {
                child.x = pool.new_node(_pos, vec2d_type(_size.x, size.y), id);
                child.y = pool.new_node(vec2d_type(_pos.x, _pos.y + size.y), vec2d_type(_size.x, _size.y - size.y), id);
            }

            n = pool.ptr(id);

            return n->_child = child;
        }
    };

private: // members only
    node_pool _node_pool;
    handle _root;
    const value_type _initial_split_size;
    const value_type _size;

};

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

COID_NAMESPACE_END

#endif // __OUTERRA_ENG3_VT_ALLOC_H__
