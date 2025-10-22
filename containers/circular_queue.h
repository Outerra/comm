#pragma once

#include "../dynarray.h"



namespace coid
{
    template <typename T, uints min_size>
    class circular_queue
    {
    public: // methods only
        circular_queue()
            : _items()
        {
            _items.add(_size);
        }

        void push(const T& item)
        {
            push() = item;
        }

        T& push()
        {
            _head = (_head + 1) & _size_mask;

            if (_head == _tail)
                _tail = (_tail + 1) & _size_mask;

            return _items[_head];
        }

        bool is_empty() const { return _head == _tail; }

        uints sizes() const { return _head >= _tail ? _head - _tail : _size; }
        uint32 size() const { return coid::down_cast<uint32>(sizes()); }


        ///Invoke functor on each element
        /// @param fn functor as fn([const] T&) or fn([const] T&)
        template<typename Func>
        void for_each(Func fn, iterator_direction dir = iterator_direction::forward) const
        {
            uints idx = dir == iterator_direction::forward ? _tail : _head;
            ints inc = dir == iterator_direction::forward ? 1 : -1;

            const uints size = sizes();

            for (uints i = 0 ; i < size; ++i) 
            {
                T& v = const_cast<T&>(_items[idx]);
                fn(v);
                idx = (idx + inc) & _size_mask;
            }
        }


    protected: // members only
        
    protected: // members only
        static constexpr uints _size = constexpr_nearest_high_pow2(min_size);
        static constexpr uints _size_mask = _size - 1;

        dynarray32<T> _items;
        uints _head = 0;
        uints _tail = 0;
    };
}; // end of namespace coid