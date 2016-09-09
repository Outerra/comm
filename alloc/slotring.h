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
* Portions created by the Initial Developer are Copyright (C) 2016
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

#include "slotalloc.h"
#include "../binstring.h"

COID_NAMESPACE_BEGIN

/**
@brief Slot allocator with internal ring structure allowing to keep several layers and track changes.

For the active layer it records whether the individual items were created, deleted or possibly modified.
**/

template<class T, bool POOL=false, bool ATOMIC=false>
class slotring : public slotalloc<T, POOL, ATOMIC>
{
    typedef slotalloc<T, POOL, ATOMIC>
        slotalloc_t;

    typedef uint16 changeset_t;

public:

    slotring( int ring_size )
        : _frame(0)
    {
        _ring.alloc(ring_size);
    }

    ///Reset content. Destructors aren't invoked in the pool mode, as the objects may still be reused.
    void reset()
    {
        mark_all_modified();
        slotalloc_t::reset();
    }

    ///Discard content. Also destroys pooled objects and frees memory
    void discard() {
        slotalloc_t::discard();
        _ring.discard();
        _changeset.discard();
    }

    ///Return an item given id
    //@param id id of the item
    const T* get_item( uints id ) const
    {
        DASSERT( id < slotalloc_t::_array.size() && slotalloc_t::get_bit(id) );
        return id < slotalloc_t::_array.size() ? slotalloc_t::_array.ptr() + id : 0;
    }

    ///Return an item given id
    //@param id id of the item
    //@note non-const access assumes the data will be modified, sets the modification flag
    T* get_item( uints id )
    {
        DASSERT( id < slotalloc_t::_array.size() && slotalloc_t::get_bit(id) );

        //mark write access as potentially modifying
        set_modified(id);
        return id < slotalloc_t::_array.size() ? slotalloc_t::_array.ptr() + id : 0;
    }

    const T& operator [] (uints idx) const {
        return *get_item(idx);
    }

    T& operator [] (uints idx) {
        return *get_item(idx);
    }

    //@note always marks element as modified
    T* get_or_create( uints id, bool* is_new=0 )
    {
        T* p = slotalloc_t::get_or_create(id, &is_new);

        set_modified(id);
        return p;
    }

    ///Add new object initialized with default constructor
    T* add() {
        T* p = slotalloc_t::add();
        uints id = get_item_id(p);
        set_modified(id);
        return p;
    }

    ///Add new object, uninitialized (no constructor invoked on the object)
    //@param newitem optional variable that receives whether the object slot was newly created (true) or reused from the pool (false)
    //@note if newitem == 0 within the pool mode and thus no way to indicate the item has been reused, the reused objects have destructors called
    T* add_uninit( bool* newitem = 0 ) {
        T* p = slotalloc_t::add_uninit(newitem);
        uints id = get_item_id(p);
        set_modified(id);
        return p;
    }

    ///Delete object in the container
    void del( T* p ) {
        uints id = get_item_id(p);
        set_modified(id);

        slotalloc_t::del(p);
    }

    ///Invoke a functor on each item that was modified between two frames
    //@note const version doesn't handle array insertions/deletions during iteration
    //@param rel_frame relative frame from which to count the modifications, should be <= 0
    template<typename Func>
    void for_each_modified( int rel_frame, Func f ) const
    {
        uint mask = modified_mask(rel_frame);
        if(!mask)
            return;

        if(mask == UMAX32)
            return for_each(f);     //too old, everything needs to be considered as modified

        T const* d = slotalloc_t::_array.ptr();
        uints const* b = slotalloc_t::_allocated.ptr();
        uints const* e = slotalloc_t::_allocated.ptre();
        changeset_t const* ch = _changeset.ptr();

        for(uints const* p=b; p!=e; ++p, ++ch) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * 8 * sizeof(uints);

            for(int i=0; m && i<8*sizeof(uints); ++i, m>>=1) {
                if((m&1) != 0 && (*ch & mask) != 0)
                    f(d[s+i]);
            }
        }
    }

    //@return current frame number
    uint current_frame() const { return _frame; }

    ///Advance to the next frame
    //@return new frame number
    uint advance_frame( bool copy )
    {
        update_changeset();
        
        //push back to the ring
        int slot = _frame % _ring.size();
        slotalloc_t& prev = _ring[slot];
        slotalloc_t::swap(prev);

        ++_frame;
        if(++slot >= _ring.size())
            slot = 0;

        slotalloc_t& next = _ring[slot];
        if(copy) {
            next = prev;
        }

        //make active
        slotalloc_t::swap(next);

        return _frame;
    }

protected:

    static uint modified_mask( int rel_frame )
    {
        if(rel_frame > 0)
            return 0;

        if(rel_frame <= -5*8)
            return UMAX32;     //too old, everything needs to be considered as modified

        //frame changes aggregated like this: 8844222211111111 (MSb to LSb)
        // compute the bit plane of the relative frame
        int r = -rel_frame;
        int bitplane = 0;

        for(int g=0; r>0 && g<4; ++g) {
            int b = r >= 8 ? 8 : r;
            r -= 8;

            bitplane += b >> g;
        }

        if(r > 0)
            ++bitplane;

        return uint(1 << bitplane) - 1;
    }

    void set_modified( uints k )
    {
        //current frame is always at bit position 0
        _changeset[k] |= 1;
    }

    void update_changeset()
    {
        //the changeset keeps n bits per each element, marking if there was a change in data
        // half of the bits correspond to the given number of most recent frames
        // older frames will be coalesced, containing flags that tell if there was a change in any of the
        // coalesced frames
        //frame aggregation:
        //      8844222211111111 (MSb to LSb)

        changeset_t* chb = _changeset.ptr();
        changeset_t* che = _changeset.ptre();

        //make space for a new frame
        

        bool b8 = (_frame & 7) == 0;
        bool b4 = (_frame & 3) == 0;
        bool b2 = (_frame & 1) == 0;

        for(changeset_t* ch = chb; ch < che; ++ch) {
            changeset_t v = *ch;
            changeset_t vs = (v << 1) & 0xaeff;                 //shifted bits
            changeset_t va = ((v << 1) | (v << 2)) & 0x5100;    //aggregated bits
            changeset_t vx = vs | va;

            changeset_t xc000 = (b8 ? vx : v) & (3<<14);
            changeset_t x3000 = (b4 ? vx : v) & (3<<12);
            changeset_t x0f00 = (b2 ? vx : v) & (15<<8);
            changeset_t x00ff = vs & 0xff;

            *ch = xc000 | x3000 | x0f00 | x00ff;
        }
    }

    ///mark all used objects as modified in current frame
    void mark_all_modified()
    {
        uints const* b = slotalloc_t::_allocated.ptr();
        uints const* e = slotalloc_t::_allocated.ptre();
        changeset_t* d = _changeset.ptr();
        constexpr int NBITS = 8*sizeof(uints);

        for(uints const* p=b; p!=e; ++p, d+=NBITS) {
            uints m = *p;
            if(!m) continue;

            for(int i=0; m && i<NBITS; ++i, m>>=1) {
                if(m&1)
                    d[i] |= 1;
            }
        }
    }

private:

    dynarray<slotalloc_t> _ring;        //< ring of currently inactive arrays 

    dynarray<changeset_t> _changeset;   //< bit set per item that marks whether the item possibly changed
    uint _frame;                        //< current frame number

    //dynarray<binstring> _ops;           //< ringbuffer with create/delete operations for each tracked frame
};

COID_NAMESPACE_END
