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
        _unused = reinterpret_cast<T*>(this);   //used as a terminator
        _array.reset();
    }

    slotalloc()
    {
        DASSERT(sizeof(T) >= sizeof(uints));
        reset();
    }

    ///Insert object
    //@return pointer to the newly inserted object
    T* push( const T& v ) {
        T* p = alloc(0);
        return new(p) T(v);
    }

    ///Add new object initialized with default constructor
    T* add() {
        T* p = alloc(0);
        return new(p) T;
    }

    ///Delete object in the container
    void del( T* p )
    {
        uint id = p - _array.ptr();
        if(id >= _array.size())
            throw exception("object outside of bounds");

        p->~T();
        *reinterpret_cast<T**>(p) = _unused;
        _unused = p;
    }

    ///Return an item given id
    //@param id id of the item
    //@note it's not guaranteed that the returned item is not a freed slot
    const T* get_item( uints id ) const
    {
        DASSERT( id < _array.size() );
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    T* get_item( uints id )
    {
        DASSERT( id < _array.size() );
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    ///Get a particular free slot
    //@note 
    T* get_or_create( uints id ) {
        if(id == _array.size())
            return append_new_items(1);
        else if(id > _array.size()) {
            //insert extra items into the free queue
            uint n = id - _array.size();
            T* p = append_new_items(n+1);
            
            T** punused = &_unused;
            T* unused = _unused;
            for(uint i=0; i<n; ++i) {
                *punused = p + i;
                punused = reinterpret_cast<T**>(p+i);
            }
            *punused = unused;
            return p+n;
        }
        else {
            //find in the free queue
            T** punused = &_unused;
            T* terminator = reinterpret_cast<T*>(this);
            while(*punused != terminator) {
                if(*punused - _array.ptr() == id) {
                    T* p = *punused;
                    *punused = *reinterpret_cast<T**>(p);
                    return p;
                }
                else
                    punused = reinterpret_cast<T**>(*punused);
            }

            throw exception("attempt to allocate a non-free block");
        }
    }

    //@return id of given item, or UMAX32 if the item is not managed here
    uint get_item_id( const T* p ) const
    {
        uints id = p - _array.ptr();
        return id < _array.size()
            ? id
            : UMAX32;
    }

    ///Invoke a functor on each used item.
    //@note this assumes that the managed items do not contain a pointer to other slotalloc items at offset 0, as it's used to discern between the free and allocated items
    template<typename Func>
    void for_each( Func f ) const
    {
        T const* pb = _array.ptr();
        T const* pe = _array.ptre();

        for(T const* p=pb; p<pe; ++p) {
            T const* x = *(T const**)p;
            if(x < pb || x >= pe)
                f(*p);
        }
    }

    ///Invoke a functor on each used item.
    //@note this assumes that the managed items do not contain a pointer to other slotalloc items at offset 0, as it's used to discern between the free and allocated items
    template<typename Func>
    void for_each( Func f )
    {
        T* pb = _array.ptr();
        T* pe = _array.ptre();

        for(T* p=pb; p<pe; ++p) {
            T* x = *reinterpret_cast<T**>(p);
            if(x < pb || x >= pe)
                f(*p);
        }
    }

    ///Find first element for which the predicate returns true
    //@return pointer to the element or null
    //@note this assumes that the managed items do not contain a pointer to other slotalloc items at offset 0, as it's used to discern between the free and allocated items
    template<typename Func>
    const T* find_if(Func f) const
    {
        T const* pb = _array.ptr();
        T const* pe = _array.ptre();

        for(T const* p=pb; p<pe; ++p) {
            T const* x = *(T const**)p;
            if((x < pb || x >= pe) && f(*p))
                return p;
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
    T* _unused;                 ///< ptr to the first unused slot, ptr to this if the array needs to be enlarged

    void rebase_free_items( T* oldarray )
    {
        T* terminator = reinterpret_cast<T*>(this);
        T** punused = &_unused;
        while(*punused != terminator) {
            *punused = _array.ptr() + (*punused - oldarray);
            punused = reinterpret_cast<T**>(*punused);
        }
    }

    ///Return allocated slot
    T* alloc( uints* id )
    {
        if(_unused != reinterpret_cast<T*>(this)) {
            T* nextunused = *reinterpret_cast<T**>(_unused);
            DASSERT(uints(nextunused - _array.ptr()) < _array.size()
                || nextunused == reinterpret_cast<T*>(this));

            if(id)
                *id = _unused - _array.ptr();
            
            T* punused = _unused;
            _unused = nextunused;

            return punused;
        }

        return append_new_items(1);
    }

    T* append_new_items( uints n )
    {
        T* oldarray = _array.ptr();
        T* p = _array.add_uninit(n);

        if(oldarray != _array.ptr())
            rebase_free_items(oldarray);

        return p;
    }
};



COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_SLOTALLOC__HEADER_FILE__

