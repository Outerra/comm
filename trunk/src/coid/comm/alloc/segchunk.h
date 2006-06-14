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

#ifndef __COID_COMM_SEGCHUNK__HEADER_FILE__
#define __COID_COMM_SEGCHUNK__HEADER_FILE__

#include <new>

#include "../namespace.h"

#include "../sync/mutex.h"
#include "../commtypes.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
///Memory page that manages memory blocks of fixed size of 2^n bytes.
/// For every block allocated within the page, if you take a pointer somewhere
/// into the block, and unmask its lowest rPAGESIZE bits, you'll get the address
/// of the \a segchunk structure itself. At the start of the memory at that address
/// is always stored the address of the page again. This is used to verify that
/// the block is valid, and to fix pointers to memory blocks after a load from
/// a persistent stream
///The access to blocks is multi-thread safe.
class segchunk
{
    segchunk* _me;
    uint _first;
    ushort _numfree;
    ushort _rsize;
    comm_mutex _mx;

    enum {
        rPAGESIZE               = 12,
        PAGESIZE                = 1 << rPAGESIZE,

        rSEGSIZE                = 16,
        SEGSIZE                 = 1 << rSEGSIZE,
    };

    void reset();

    segchunk( uint rsize );

public:

    static segchunk* create( uint rsize );

    void* operator new (size_t size);
    void operator delete (void * ptr);

    static segchunk* get_segchunk( void* p )
    {
        return (segchunk*) ((uints)p &~ (uints)(PAGESIZE-1));
    }

    void* alloc();
    void free( void* p );
};


////////////////////////////////////////////////////////////////////////////////
///Allocator of raw chunks. Size of blocks to be managed is given in the constructor.
class segchunker_raw
{
    struct page
    {
        segchunk* chunk;
        page*     prev;
    };

    page* _last;

    uint _rsize;
    mutable comm_mutex _mutex;

public:

    segchunker_raw( uint size )
    {
        uint as = (uint)nextpow2(size);
        for( _rsize=0; as>1; ++_rsize,as>>=1 );
        _last = 0;
		_mutex.set_name( "segchunker_raw" );
    }

    void* alloc()
    {
        GUARDME;

        for( page* p=_last; p; p=p->prev )
        {
            void* pv = p->chunk->alloc();
            if(pv)  return pv;
        }

        page* pn = new page;
        pn->prev = _last;
        pn->chunk = segchunk::create(_rsize);
        _last = pn;

        return pn->chunk->alloc();
    }

    void free( void* p )
    {
        segchunk* seg = segchunk::get_segchunk(p);
        seg->free(p);
    }

    bool check( void* p ) const
    {
        segchunk* seg = segchunk::get_segchunk(p);
        for( page* pg=_last; pg; pg=pg->prev )
            if( pg->chunk == seg )  return true;
        return false;
    }
};


////////////////////////////////////////////////////////////////////////////////
///Allocator of typed chunks. Size of blocks to be managed is obtained from sizeof(T)
template <class T>
class segchunker
{
    struct page
    {
        segchunk* chunk;
        page*     prev;
    };

    page* _last;

    uint _rsize;
    mutable comm_mutex _mutex;

public:

    segchunker()
    {
        uint as = (uint)nextpow2( sizeof(T) );
        for( _rsize=0; as>1; ++_rsize,as>>=1 );
        _last = 0;
		_mutex.set_name( "segchunker" );
    }

    T* alloc()
    {
        GUARDME;

        for( page* p=_last; p; p=p->prev )
        {
            void* pv = p->chunk->alloc();
            if(pv)  return new(pv) T;
        }

        page* pn = new page;
        pn->prev = _last;
        pn->chunk = segchunk::create(_rsize);
        _last = pn;

        void* t = pn->chunk->alloc();
        return new(t) T;
    }

    static void free( T* p )
    {
        RASSERT(p);
        segchunk* seg = segchunk::get_segchunk(p);
        p->~T();
        seg->free(p);
    }

    bool check( T* p ) const
    {
        segchunk* seg = segchunk::get_segchunk(p);
        for( page* pg=_last; pg; pg=pg->prev )
            if( pg->chunk == seg )  return true;
        return false;
    }
};

COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_SEGCHUNK__HEADER_FILE__

