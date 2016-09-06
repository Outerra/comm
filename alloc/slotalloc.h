#pragma once

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
#include "../atomic/atomic.h"
#include "../namespace.h"
#include "../commexception.h"
#include "../dynarray.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
/**
@brief Allocator for efficient slot allocation/deletion of objects.

Allocates array of slots for given object type, and allows efficient allocation and deallocation of objects without
having to resort to expensive system memory allocators.
Objects within the array have a unique slot id that can be used to retrieve the objects by id or to have an id of the 
object during its lifetime.
The allocator has for_each and find_if methods that can run functors on each object managed by the allocator.

Optionally can be constructed in pool mode, in which case the removed/freed objects aren't destroyed, and subsequent
allocation can return one of these without having to call the constructor. This is good when the objects allocate
their own memory buffers and it's desirable to avoid unnecessary freeing and allocations there as well. Obviously,
care must be taken to initialize the initial state of allocated objects in this mode.
All objects are destroyed with the destruction of the allocator object.

Safe to use from a single producer / single consumer threading mode, as long as the working set is
reserved in advance.

@param POOL if true, do not call destructors on item deletion, only on container deletion
@param ATOMIC if true, ins/del operations and versioning are done as atomic operations
**/
template<class T, bool POOL=false, bool ATOMIC=false>
class slotalloc
{
public:

    ///Construct slotalloc container
    //@param pool true for pool mode, in which removed objects do not have destructors invoked
    slotalloc() : _count(0), _version(0)
    {}

    explicit slotalloc( uints reserve_items ) : _count(0), _version(0) {
        reserve(reserve_items);
    }

    ~slotalloc() {
        if(!POOL)
            reset();
    }

    ///Reset content. Destructors aren't invoked in the pool mode, as the objects may still be reused.
    void reset()
    {
        //destroy occupied slots
        if(!POOL) {
            for_each([](T& p) {destroy(p);});

            _relarrays.for_each([&](relarray& ra) {
                ra.reset();
            });
        }

        if(ATOMIC)
            atomic::exchange(&_count, 0);
        else
            _count = 0;

        _relarrays.for_each([&](relarray& ra) {
            ra.set_count(0);
        });

        _array.set_size(0);
        _allocated.set_size(0);
    }

    ///Discard content. Also destroys pooled objects and frees memory
    void discard()
    {
        //destroy occupied slots
        if(!POOL) {
            for_each([](T& p) {destroy(p);});

            _relarrays.for_each([&](relarray& ra) {
                ra.set_count(0);
            });

            _array.set_size(0);
            _allocated.set_size(0);
        }

        if(ATOMIC)
            atomic::exchange(&_count, 0);
        else
            _count = 0;

        _array.discard();
        _allocated.discard();

        _relarrays.for_each([&](relarray& ra) {
            ra.discard();
        });
    }


    ///Append related array of type R which will be managed alongside the main array of type T
    //@return array index
    //@note types aren't initialized when allocated
    template<typename R>
    uints append_relarray( dynarray<R>** parray = 0 ) {
        uints idx = _relarrays.size();
        relarray* p = new(_relarrays.add_uninit())
            relarray(&type_creator<R>::constructor, &type_creator<R>::destructor);
        p->elemsize = sizeof(T);

        auto& dyn = dynarray<R>::from_dynarray_conforming_ptr(p->data);
        dyn.reserve(_array.reserved_total() / sizeof(T), true);

        if(parray)
            *parray = &dyn;

        return idx;
    }

    //@return value from related array
    template<typename R>
    R* value( const T* v, uints index ) const {
        return reinterpret_cast<R*>(_relarrays[index].item(get_item_id(v)));
    }

    void swap( slotalloc<T>& other ) {
        _array.swap(other._array);
        _allocated.swap(other._allocated);
        std::swap(_count, other._count);

        _relarrays.swap(other._relarrays);
    }

    //@return byte offset to the newly rebased array
    ints reserve( uints nitems ) {
        T* old = _array.ptr();
        T* p = _array.reserve(nitems, true);

        _relarrays.for_each([&](relarray& ra) {
            ra.reserve(nitems);
        });

        return (uints)p - (uints)old;
    }

    ///Insert object
    //@return pointer to the newly inserted object
    T* push( const T& v ) {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(POOL) destroy(*p);
            return new(p) T(v);
        }
        return new(append()) T(v);
    }

    ///Insert object
    //@return pointer to the newly inserted object
    T* push( T&& v ) {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(POOL) destroy(*p);
            return new(p) T(v);
        }
        return new(append()) T(v);
    }

    ///Add new object initialized with default constructor
    T* add() {
        return _count < _array.size()
            ? (POOL ? alloc(0) : new(alloc(0)) T)
            : new(append()) T;
    }

    ///Add new object, uninitialized (no constructor invoked on the object)
    //@param newitem optional variable that receives whether the object slot was newly created (true) or reused from the pool (false)
    //@note if newitem == 0 within the pool mode and thus no way to indicate the item has been reused, the reused objects have destructors called
    T* add_uninit( bool* newitem = 0 ) {
        if(_count < _array.size()) {
            T* p = alloc(0);
            if(POOL) {
                if(!newitem) destroy(*p);
                else *newitem = false;
            }
            return p;
        }
        if(newitem) *newitem = true;
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

        if(!POOL)
            p->~T();
        clear_bit(id);
        //--_count;
        if(ATOMIC) {
            atomic::dec(&_count);
            atomic::inc(&_version);
        }
        else {
            --_count;
            ++_version;
        }
    }

    //@return number of used slots in the container
    uints count() const { return _count; }

    //@return true if next allocation would rebase the array
    bool full() const { return (_count + 1)*sizeof(T) > _array.reserved_total(); }

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

    const T& operator [] (uints idx) const {
        return *get_item(idx);
    }

    T& operator [] (uints idx) {
        return *get_item(idx);
    }

    ///Get a particular free slot
    //@param id item id
    //@param is_new optional if not null, receives true if the item was newly created
    T* get_or_create( uints id, bool* is_new=0 )
    {
        if(id < _array.size()) {
            if(get_bit(id)) {
                if(is_new) *is_new = false;
                return _array.ptr() + id;
            }

            set_bit(id);
            //++_count;
            if(ATOMIC) {
                atomic::inc(&_count);
                atomic::inc(&_version);
            }
            else {
                ++_count;
                ++_version;
            }

            if(is_new) *is_new = true;
            return _array.ptr() + id;
        }

        uints n = id+1 - _array.size();

        _relarrays.for_each([&](relarray& ra) {
            DASSERT( ra.count() == _count );
            ra.add(n);
        });

        _array.add(n);

        set_bit(id);
        //++_count;
        if(ATOMIC) {
            atomic::inc(&_count);
            atomic::inc(&_version);
        }
        else {
            ++_count;
            --_version;
        }

        if(is_new) *is_new = true;
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
    //@note const version doesn't handle array insertions/deletions during iteration
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
                    f(d[s+i]);
            }
        }
    }

    ///Invoke a functor on each used item.
    //@note non-const version handles array insertions/deletions during iteration
    template<typename Func>
    void for_each( Func f )
    {
        T* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();
        uints version = _version;

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1) {
                    f(d[s+i]);
                    if(version != _version) {
                        //handle alterations and rebase
                        d = _array.ptr();
                        ints x = p - b;
                        b = _allocated.ptr();
                        e = _allocated.ptre();
                        p = b + x;
                        m = *p >> i;
                        version = _version;
                    }
                }
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

    ///Find first element for which the predicate returns true
    //@return pointer to the element or null
    template<typename Func>
    T* find_if(Func f)
    {
        T* d = _array.ptr();
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();

        for(uints const* p=b; p!=e; ++p) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if(m&1) {
                    T& v = d[s + i];
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

    typedef typename std::conditional<ATOMIC, volatile uints, uints>::type uint_type;

    dynarray<T> _array;
    dynarray<uints> _allocated;     //< bit mask for allocated/free items
    uint_type _count;
    uint_type _version;

    ///Related data array that's maintained together with the main one
    struct relarray
    {
        void* data;                 //< dynarray-conformant pointer
        uint elemsize;              //< element size
        void (*destructor)(void*);
        void (*constructor)(void*);

        relarray(
            void (*constructor)(void*),
            void (*destructor)(void*))
            : data(0), elemsize(0), constructor(constructor), destructor(destructor)
        {}

        ~relarray() {
            discard();
        }

        uints count() const { return comm_array_allocator::count(data); }

        void reset() {
            uints n = comm_array_allocator::count(data);
            if(destructor) {
                uint8* p = (uint8*)data;
                for(uints i=0; i<n; ++i, p+=elemsize)
                    destructor(p);
            }
            comm_array_allocator::set_count(data, 0);
        }

        void discard() {
            reset();
            comm_array_allocator::free(data);
            data = 0;
        }

        void set_count( uints n ) {
            comm_array_allocator::set_count(data, n);
        }
        
        void add( uints addn ) {
            uints curn = comm_array_allocator::count(data);
            data = comm_array_allocator::add(data, addn, elemsize);

            if(constructor) {
                uint8* p = (uint8*)data + curn * elemsize;
                for(uints i=0; i<addn; ++i, p+=elemsize)
                    constructor(p);
            }
        }

        void reserve( uints n ) {
            uints curn = comm_array_allocator::count(data);
            data = comm_array_allocator::realloc(data, n, elemsize);
            comm_array_allocator::set_count(data, curn);
        }

        void* item( uints index ) const {
            return (uint8*)data + index * elemsize;
        }
    };

    dynarray<relarray> _relarrays;  //< related data arrays

    ///Return allocated slot
    T* alloc( uints* id )
    {
        DASSERT( _count < _array.size() );

        uint_type* p = _allocated.ptr();
        uint_type* e = _allocated.ptre();
        for(; p!=e && *p==UMAXS; ++p);

        if(p == e)
            *(p = _allocated.add()) = 0;

        uint8 bit = lsb_bit_set((uints)~*p);
        uints slot = ((uints)p - (uints)_allocated.ptr()) * 8;

        DASSERT( !get_bit(slot+bit) );

        if(id)
            *id = slot + bit;
        //*p |= uints(1) << bit;
        if(ATOMIC) {
            atomic::aor(p, uints(1) << bit);

            atomic::inc(&_count);
            atomic::inc(&_version);
        }
        else {
            *p |= uints(1) << bit;
            ++_count;
            ++_version;
        }
        return _array.ptr() + slot + bit;
    }

    T* append()
    {
        uints count = _count;

        DASSERT( count == _array.size() );
        set_bit(count);

        _relarrays.for_each([&](relarray& ra) {
            DASSERT( ra.count() == count );
            ra.add(1);
        });

        if(ATOMIC) {
            atomic::inc(&_count);
            atomic::inc(&_version);
        }
        else {
            ++_count;
            ++_version;
        }

        return _array.add_uninit(1);
    }

    void set_bit( uints k )
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));

        if(ATOMIC)
            atomic::aor(const_cast<uint_type*>(&_allocated.get_or_addc(s)), uints(1) << b);
        else
            _allocated.get_or_addc(s) |= uints(1) << b;
    }

    void clear_bit( uints k )
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));
        
        if(ATOMIC)
            atomic::aand(const_cast<uint_type*>(&_allocated.get_or_addc(s)), ~(uints(1) << b));
        else
            _allocated.get_or_addc(s) &= ~(uints(1) << b);
    }

    bool get_bit( uints k ) const
    {
        uints s = k / (8*sizeof(uints));
        uints b = k % (8*sizeof(uints));

        if(ATOMIC)
            return s < _allocated.size()
                && (*const_cast<uint_type*>(_allocated.ptr()+s) & (uints(1) << b)) != 0;
        else
            return s < _allocated.size() && (_allocated[s] & (uints(1) << b)) != 0;
    }

    //WA for lambda template error
    void static destroy(T& p) {p.~T();}
};



COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_SLOTALLOC__HEADER_FILE__

