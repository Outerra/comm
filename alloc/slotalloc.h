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
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2013
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Brano Kemen
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

#ifndef __COID_COMM_SLOTALLOC__HEADER_FILE__
#define __COID_COMM_SLOTALLOC__HEADER_FILE__

#include <new>
#include "../namespace.h"
#include "../commexception.h"
#include "../dynarray.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Allocator for efficient slot allocation/deletion.
///Keeps a linked list of freed objects in places where the objects have been
/// allocated (objects must be >= sizeof(void*)
template<class T>
class slotalloc
{
public:

    void reset()
    {
        //destroy occupied slots
        if(!_pool)
            for_each([](T& p) {destroy(p);});

        _count = 0;
        _array.set_size(0);
        _allocated.set_size(0);
    }

    slotalloc( bool pool = false ) : _count(0), _pool(pool)
    {}

    ~slotalloc() {
        if(!_pool)
            reset();
    }

    void swap( slotalloc<T>& other ) {
        _array.swap(other._array);
        _allocated.swap(other._allocated);
        std::swap(_count, other._count);
        std::swap(_pool, other._pool);
    }

    //@return byte offset to the newly rebased array
    ints reserve( uint nitems ) {
        T* old = _array.ptr();
        T* p = _array.reserve(nitems, true);

        return (uints)p - (uints)old;
    }

    ///Insert object
    //@return pointer to the newly inserted object
    T* push( const T& v ) {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(_pool) destroy(*p);
            return new(p) T(v);
        }
        return new(append()) T(v);
    }

    ///Add new object initialized with default constructor
    T* add() {
        return _count < _array.size()
            ? (_pool ? alloc(0) : new(alloc(0)) T)
            : new(append()) T;
    }

    ///Add new object, uninitialized
    T* add_uninit() {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(_pool) destroy(*p);
            return p;
        }
        return append();
    }
/*
    //@return id of the next object that will be allocated with add/push methods
    uints get_next_id() const {
        return _unused != reinterpret_cast<const T*>(this)
            ? _unused - _array.ptr()
            : _array.size();
    }*/

    ///Delete object in the container
    void del( T* p )
    {
        uints id = p - _array.ptr();
        if(id >= _array.size())
            throw exception("object outside of bounds");

        DASSERT( get_bit(id) );

        if(!_pool)
            p->~T();
        clear_bit(id);
        --_count;
    }

    //@return number of used slots in the container
    uints count() const { return _count; }

    ///Return an item given id
    //@param id id of the item
    const T* get_item( uints id ) const
    {
        DASSERT( id < _array.size() && get_bit(id) );
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    T* get_item( uints id )
    {
        DASSERT( id < _array.size() && get_bit(id) );
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    ///Get a particular free slot
    T* get_or_create( uints id )
    {
        if(id < _array.size()) {
            if(get_bit(id))
                return _array.ptr() + id;
            set_bit(id);
            ++_count;
            return _array.ptr() + id;
        }

        _array.add(id+1 - _array.size());

        set_bit(id);
        ++_count;
        return _array.ptr() + id;
    }

    //@return id of given item, or UMAXS if the item is not managed here
    uints get_item_id( const T* p ) const
    {
        uints id = p - _array.ptr();
        return id < _array.size()
            ? id
            : UMAXS;
    }

    //@return true if item is valid
    bool is_valid_item( uints id ) const
    {
        return get_bit(id);
    }

    ///Invoke a functor on each used item.
    template<typename Func>
    void for_each( Func f ) const
    {
        T const* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1)
                    f(d[s + i]);
            }
        }
    }

    ///Invoke a functor on each used item.
    template<typename Func>
    void for_each( Func f )
    {
        T* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1)
                    f(d[s + i]);
            }
        }
    }

    ///Find first element for which the predicate returns true
    //@return pointer to the element or null
    template<typename Func>
    const T* find_if(Func f) const
    {
        T const* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1) {
                    const T& v = d[s + i];
                    if(f(v))
                        return &v;
                }
            }
        }

        return 0;
    }

    //@{ Get internal array directly
    //@note altering the array directly may invalidate the internal structure
    dynarray<T>& get_array() { return _array; }
    const dynarray<T>& get_array() const { return _array; }
    //@}

private:

    dynarray<T> _array;
    dynarray<uints> _allocated;     //< bit mask for allocated/free items
    uints _count;
    bool _pool;                     //< if true, do not call destructors on deletion

    ///Return allocated slot
    T* alloc( uints* id )
    {
        DASSERT( _count < _array.size() );

        uints* p = _allocated.ptr();
        uints* e = _allocated.ptre();
        for(; p!=e && *p==UMAXS; ++p);

        if(p == e)
            *(p = _allocated.add()) = 0;

        uint8 bit = lsb_bit_set((uints)~*p);
        uints slot = ((uints)p - (uints)_allocated.ptr()) * 8;

        DASSERT( !get_bit(slot+bit) );

        if(id)
            *id = slot + bit;
        *p |= uints(1) << bit;

        ++_count;
        return _array.ptr() + slot + bit;
    }

    T* append()
    {
        DASSERT( _count == _array.size() );
        set_bit(_count++);
        return _array.add_uninit(1);
    }

    void set_bit( uints k )
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));
        _allocated.get_or_addc(s) |= uints(1) << b;
    }

    void clear_bit( uints k )
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));
        _allocated.get_or_addc(s) &= ~(uints(1) << b);
    }

    bool get_bit( uints k ) const
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));
        return s < _allocated.size() && (_allocated[s] & (uints(1) << b)) != 0;
    }

    //WA for lambda template error
    void static destroy(T& p) {p.~T();}
};



COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_SLOTALLOC__HEADER_FILE__

