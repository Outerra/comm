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
class slotring : protected slotalloc<T, POOL, ATOMIC>
{
    typedef slotalloc<T, POOL, ATOMIC>
        slotalloc_t;

    typedef uint16 changeset_t;

public:

    slotring( int ring_size )
        : _frame(0)
    {
        _ring.alloc(ring_size - 1);
    }

    ///Reset content. Destructors aren't invoked in the pool mode, as the objects may still be reused.
    void reset()
    {
        mark_all_modified();
    }

    ///Discard content. Also destroys pooled objects and frees memory
    void discard()

    ///Return an item given id
    //@param id id of the item
    const T* get_item( uints id ) const
    {
        DASSERT( id < _array.size() && get_bit(id) );
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    ///Return an item given id
    //@param id id of the item
    //@note non-const access assumes the data will be modified, sets the modification flag
    T* get_item( uints id )
    {
        DASSERT( id < _array.size() && get_bit(id) );

        //mark write access as potentially modifying
        set_modified(id);
        return id < _array.size() ? _array.ptr() + id : 0;
    }

    const T& operator [] (uints idx) const {
        return *get_item(idx);
    }

    T& operator [] (uints idx) {
        return *get_item(idx);
    }


protected:

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

    ///mark all used objects as modified
    void mark_all_modified()
    {
        uints const* b = _allocated.ptr();
        uints const* e = _allocated.ptre();
        changeset_t* d = _changeset.ptr();
        constexpr int NBITS = 8*sizeof(uints);

        for(uints const* p=b; p!=e; ++p, d+=NBITS) {
            uints m = *p;
            if(!m) continue;

            uints s = (p - b) * NBITS;

            for(int i=0; m && i<NBITS; ++i, m>>=1) {
                if(m&1)
                    *d |= 1;
            }
        }
    }

private:

    dynarray<slotalloc_t> _ring;        //< ring of currently inactive arrays 

    dynarray<changeset_t> _changeset;   //< bit set per item that marks whether the item possibly changed
    uint _frame;                        //< current frame number

    dynarray<binstring> _ops;           //< ringbuffer with create/delete operations for each tracked frame
};

COID_NAMESPACE_END
