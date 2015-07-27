#pragma once
#ifndef __COMM_ALLOC_SLOTALLOC_BMP_H__
#define __COMM_ALLOC_SLOTALLOC_BMP_H__

#include "../dynarray.h"

#include "../mathi.h"

COID_NAMESPACE_BEGIN

typedef uint BLOCK_TYPE;

template<typename T>
class slotalloc_bmp
{
private:

    dynarray<BLOCK_TYPE> _bmp;
    dynarray<T> _items;

public:

    /// size: preallocated size
    slotalloc_bmp(const uint size = 32)
        : _bmp()
        , _items()
    {
        _bmp.add_uninit(size);
        memset(_bmp.ptr(), 0, _bmp.byte_size());
        _items.add_uninit(_bmp.size() * sizeof(BLOCK_TYPE) * 8);
    }

    ~slotalloc_bmp() { clear(); }

    /// have to call this in destructor because we have item allocated with add_uninit...
    void clear()
    {
        uints id = first();
        while (id != -1) {
            get_item(id)->~T();
            id = next(id);
        }

        _items.set_size(0);
    }

    /// TODO use SSE for faster iteration to free block?
    /// SSE can do 16bytes at once our array is aligned
    T* add_uninit()
    {
        // find empty block 32bit
        BLOCK_TYPE * b = _bmp.ptr();
        BLOCK_TYPE * const be = _bmp.ptre();
        while (b != be && *b == 0xffffffff)
            b++;

        if (b == be) {
            const uints size = _bmp.size();
            b = _bmp.add_uninit(size);
            memset(b, 0, size * sizeof(BLOCK_TYPE));
            _items.add_uninit(size * sizeof(BLOCK_TYPE) * 8);
        }
        else {
            // in this case we now new block is empty we could skip search phase
        }

        uchar index = lsb_bit_set(~*b);

        *b |= 1 << index;

        return _items.ptr() + (b - _bmp.ptr()) * 32 + index;
    }

    uints get_item_id(const T * const ptr) const
    {
        DASSERT(ptr < _items.ptre());
        const uints id = ptr - _items.ptr();

        return is_valid(id) ? id : -1;
    }

    T* get_item(const uint id) { DASSERT(is_valid(id) && id < _items.size());  return _items.ptr() + id; }
    const T* get_item(const uint id) const { DASSERT(is_valid(id) && id < _items.size());  return _items.ptr() + id; }

    bool is_valid(const uints id) const
    {
        const uints s0 = id >> 3;
        const uchar slots = s0 < (_bmp.size() << 2) ? reinterpret_cast<const uchar*>(_bmp.ptr())[s0] : 0;
        return (slots & (1 << (id & 7))) != 0;
    }

    void del(const uints id)
    {        
        DASSERT((id >> 5) <_bmp.size() && _items.size() != 0);

        const uints s0 = id >> 3;
        const uchar block = reinterpret_cast<uchar*>(_bmp.ptr())[s0];
        const uchar slot = 1 << (id & 7);
        
        DASSERT((block & slot) != 0);

        reinterpret_cast<uchar*>(_bmp.ptr())[s0] ^= slot;
        
        _items[id].~T();        
    }

    uints first() const
    {
        const BLOCK_TYPE * b = _bmp.ptr();
        const BLOCK_TYPE * const be = _bmp.ptre();

        while (b != be && *b == 0)
            ++b;
        if (b == be) return -1;

        uchar index = lsb_bit_set(*b);

        return (b - _bmp.ptr()) * 32 + index;
    }

    uints next(uints id) const
    {
        ++id;

        DASSERT(id != -1 && id <= _bmp.size() * 32);

        const BLOCK_TYPE * b = _bmp.ptr() + (id >> 5);
        const BLOCK_TYPE * const be = _bmp.ptre();
        uchar index = id & 31;

        if (b == be)
            return -1;

        if (index != 0) {
            const uint tmp = *b & ~((1 << index) - 1);
            index = tmp != 0
                ? lsb_bit_set(tmp)
                : 32;
        }

        if (index == 32) {
            ++b;
            while (b != be && *b == 0)
                ++b;

            if (b == be)
                return -1;

            index = lsb_bit_set(*b);
        }

        return (b - _bmp.ptr()) * 32 + index;
    }
};

COID_NAMESPACE_END

#endif // __COMM_ALLOC_SLOTALLOC_BMP_H__