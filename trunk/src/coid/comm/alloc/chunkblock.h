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
 * PosAm.
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#ifndef __COID_COMM_CHUNKBLOCK__HEADER_FILE__
#define __COID_COMM_CHUNKBLOCK__HEADER_FILE__

#include "../namespace.h"

#include "../commtypes.h"
#include "chunkpage.h"
#include "commalloc.h"
#include "memtrack.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
///Memory page that manages memory blocks of fixed size.
/** The structure itself is the header of the memory page, so it's got custom
    new operator and hidden constructor.
    
    For every block allocated within the page, if you take a pointer somewhere
    into the block, and unmask its lowest rPAGESIZE bits, you'll get the address
    of the \a chunkblock structure itself. At the start of the memory at that address
    is always stored the address of the page again first. This is used to verify that
    the block is valid, and to fix pointers to memory blocks after a load from
    a persistent stream.

    @param T managed element type
    @param EXTEND type that the class should be extended with, required because this
    class cannot be derived but still may want to be extended in certain manner
**/
template<class T, class EXTEND=void, unsigned PAGESIZE=8192>
class chunkblock
{
    chunkblock* _me;            ///< copy of this pointer
    uints _first;               ///< first free chunk offset or zero when none free
    uint _numfree;              ///< number of free chunks
    ints _used;                 ///< first never used chunk number or -1 when all already used
    uints _totalsize;           ///< total size of used memory, PAGESIZE * npages created
    ElementCreator<T> item;     ///< item size info, takes no space for non-void T
public:
    ExtendClassMember<EXTEND> ext;   ///< space to reserve for extended member

private:
    chunkblock() {}

    void construct( uint size, uints totalmem )
    {
        _totalsize = totalmem;
        DASSERT( size >= sizeof(uints) );   //element size too small
        DASSERT( size*16 < PAGESIZE );      //too big element or ineffective for this page size
        item.set_size(size);
        reset();
    }

    void reset()
    {
        _me = this;
        _numfree = uint((PAGESIZE - _first) / item.size);
        _used = align_value( sizeof(chunkblock), sizeof(uint64) );
        _first = 0;
    }

    void* operator new (uints size, uints total )
    {
        MEMTRACK(chunkblock, size);
        return memaligned_alloc( total, PAGESIZE );
    }

    void operator delete (void *ptr, uints )
    {   memaligned_free( ptr ); }


public:

    ///Create chunkblock pages for items of size \a itemsize
    static chunkblock* create( uint itemsize, uint pages=8 )
    {
        DASSERT( IS_2_POW_N(PAGESIZE) );    //keep PAGESIZE a power of 2
        uints totalmem = pages*PAGESIZE;
        chunkblock* p = new(totalmem) chunkblock;
        p->construct(itemsize,totalmem);
        return p;
    }

    void destroy()
    {
        delete this;
    }

    ///Get chunkblock structure where the \a p pointer would lie
    static chunkblock* get_segchunk( void* p )
    {   return (chunkblock*) ((uints)p &~ (uints)(PAGESIZE-1)); }


    T* alloc()
    {
        void* p = alloc_nocreate();
        return p ? item.create(p) : 0;
    }

    void* alloc_nocreate()
    {
        if( _numfree == 0 )
            return 0;

        void* p;
        if(!_first)    //working in direct mode
        {
            p = ((char*)this + _used);
            _used += item.size;
            if( (uints)_used > _totalsize-item.size )    //out of direct memory, switch to the list mode
                _used = -1;
            else
            {
                //skip PAGESIZE boundaries if needed
                uints m = (_used+item.size)%PAGESIZE;
                if( m <= item.size ) {
                    _used += item.size;
                    uints pb = _used & ~(PAGESIZE-1);
                    _used = pb + sizeof(uint64);
                    *(uints*)((char*)this+pb) = (uints)this;    //point to structure header on the page boundaries
                }
            }
        }
        else                //working in list mode
        {
            uint* pn = (uint*) ( (char*)this + _first );
            DASSERT( *pn >= PAGESIZE || *pn==0 || (*pn > sizeof(chunkblock) && (*pn-sizeof(chunkblock))%item.size == 0) );
            DASSERT( *pn < PAGESIZE || (*pn > sizeof(uints) && *pn < _totalsize && ((*pn%PAGESIZE)-sizeof(uints))%item.size == 0) );

            _first = *pn;
            p = pn;
        }

        --_numfree;
        return p;
    }

    void free( T* p )
    {
        ints n = check_chunk(p);
        if(n<0)  return;

        item.destroy(p);

        *(uints*)p = _first;
        _first = n;

        ++_numfree;
    }

    void free_nodestroy( void* p )
    {
        ints n = check_chunk(p);
        if(n<0)  return;

        *(uints*)p = _first;
        _first = n;

        ++_numfree;
    }

private:
    ints check_chunk( void* p ) const
    {
        ints n = (char*)p-(char*)this;
        DASSERT_RET( n >= (ints)sizeof(chunkblock)  &&  n < (ints)_totalsize, -1 );    //invalid pointer

#ifdef _DEBUG
        ints m = (n<(ints)PAGESIZE) ? (n-align_value(sizeof(chunkblock),sizeof(uint64))) : (n-sizeof(uint64));
        DASSERT_RET( m%item.size == 0, -1 );                                    //misaligned pointer
#endif
        return n;
    }
};

COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_CHUNKBLOCK__HEADER_FILE__

