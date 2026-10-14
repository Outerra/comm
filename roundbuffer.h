#ifndef __COMM_ROUNDBUFFER_H__
#define __COMM_ROUNDBUFFER_H__

#include "namespace.h"
#include "commtypes.h"
#include "dynarray.h"

#include <atomic>

// ONE reader ONE writer

COID_NAMESPACE_BEGIN

template<class T,uint SIZE>
class roundbuffer
    : protected dynarray<T>
{
private:

    static_assert(SIZE >= 2 && (SIZE & (SIZE - 1)) == 0, "roundbuffer: SIZE must be a power of two");

    // Each index is written by one thread only - _head by the writer, _tail by the reader - and read
    // by the other, so both are atomics: a plain or volatile variable shared like that is a data race.
    std::atomic<uint> _head = 0;            ///< slot of the last pushed item - written by the writer
    std::atomic<uint> _tail = 0;            ///< slot of the last popped item - written by the reader

    static const uint SIZE_MASK=SIZE-1;

public:

    /// size MUST be 2^n
    roundbuffer()
        : dynarray<T>()
    {
        this->resize(SIZE);
    }

    /// @brief Appends an item - the writer thread only
    /// @return false when the buffer is full, which is normal flow control - the writer tries again
    ///  later; the buffer holds at most SIZE - 1 items
    bool push( const T& i )
    {
        const uint head = _head.load(std::memory_order_relaxed);
        const uint tmphead=(head+1)&SIZE_MASK;

        // acquire: the reader has finished with the slot it gave back before it moved the tail
        if( tmphead==_tail.load(std::memory_order_acquire) )
            return false;

        this->ptr()[tmphead]=i;

        // release: the reader that sees the new head sees the item in the slot
        _head.store(tmphead, std::memory_order_release);
        return true;
    }

    /// @brief Takes the oldest item - the reader thread only
    /// @return false when the buffer is empty
    bool pop( T& i )
    {
        const uint tail = _tail.load(std::memory_order_relaxed);

        // acquire: the item the writer stored before it moved the head is visible
        if( tail==_head.load(std::memory_order_acquire) )
            return false;

        const uint tmptail=(tail+1)&SIZE_MASK;

        i= this->ptr()[tmptail];

        // release: the writer that sees the new tail may overwrite the slot
        _tail.store(tmptail, std::memory_order_release);

        return true;
    }

};

COID_NAMESPACE_END

#endif // __COMM_ROUNDBUFFER_H__

