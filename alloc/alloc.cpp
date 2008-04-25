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

#include "alloc.h"
#include "../sync/mutex.h"
#include "../commassert.h"

#include "../dynarray.h"

#include <malloc.h>
#include <string.h>
#include <algorithm>


namespace coid {

//seg_allocator Gsega;
//seg_allocator*  comm_allocator::_alloc = 0;

////////////////////////////////////////////////////////////////////////////////
///Memory seg_allocator. Uses ssegpage class to manage chunks in segments of
/// memory. The segments are aligned to specified boundary (64k), and chunks
/// within never start at the boundary. This is used to determine whether
/// an unknown chunk comes from within a segment or it alone is an independent
/// segment.


#define REDHDRSIZE      sizeof(uints)
#define IFGUARD(m)     comm_mutex_guard<comm_mutex> __guard(1);  if(m.is_set())  __guard.inject(*m)

////////////////////////////////////////////////////////////////////////////////
void* seg_allocator::operator new (size_t size)
{
    return ::malloc(size); 
}

////////////////////////////////////////////////////////////////////////////////
void  seg_allocator::operator delete (void * ptr)
{
    ::free(ptr);
}

////////////////////////////////////////////////////////////////////////////////
seg_allocator::~seg_allocator()
{
    discard();
}

////////////////////////////////////////////////////////////////////////////////
void seg_allocator::discard()
{
    _last_succ = 0;
    IFGUARD(_pgcreatemutex);
    page* p = _last;
    _last = 0;
    for( ; p; )
    {
        delete p->spage;
        page* pp = p;
        p = p->prev;
        SINGLETON( chunk_allocator<page> ).free(pp);
    }

    _nseg = 0;
}

////////////////////////////////////////////////////////////////////////////////
opcd seg_allocator::save( binstream& bin )
{
    opcd e;
    
    bin << _segsize;
    page* p = _last;
    
    for( ; p; p=p->prev )
    {
        e = p->spage->write_to_stream(bin);
        if(e)  return e;
    }

    return e;
}

////////////////////////////////////////////////////////////////////////////////
opcd seg_allocator::load( binstream& bin, void* dynarray_ald )
{
    opcd e;
    discard();

    dynarray<load_data>& ald = *(dynarray<load_data>*)dynarray_ald;
    
    bin >> _segsize;
    
    page* p=0;
    for( ; ; )
    {
        page* n = SINGLETON( chunk_allocator<page> ).alloc();
        n->prev = p;
        n->spage = ssegpage::create( _pgcreatemutex.is_set(), _segsize );

        _last = n;
        if(!n->spage)  return ersNOT_ENOUGH_MEM "cannot create ssegpage";

        ++_nseg;

        void* base;
        int diff;

        e = n->spage->read_from_stream( bin, &base, &diff );
        if( e == ersMISMATCHED )  return e;
        else if(e)  break;

        load_data* pld = ald.add();
        pld->base = base;
        pld->diff = diff;
    }

    std::sort( ald.begin(), ald.end() );

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
seg_allocator::HEADER* seg_allocator::alloc( uints count, uints chunk )
{
    uints size = count*chunk + REDHDRSIZE;
    if( size >= _segsize/2 )
    {
        HEADER* hdr;
        hdr = (HEADER*) ssegpage::alloc_big(size);
        if(!hdr)
            throw ersEXCEPTION "not enough memory";

        hdr->_count = count;
        //hdr->_size = size - sizeof(HEADER);
        return hdr;
    }
    else
    {
//        COMM_SEGPAGE_LOCK;

        if(!_last_succ)
            _last_succ = _last;
        page* pc = _last_succ;
        if(pc)
        {
            //fast find in unlocked ssegpage
            for( page* pf = pc; ; )
            {
#ifdef _DEBUG
                long turn_on_to_check = 0;
                if( turn_on_to_check )
                {
                    ssegpage::SEGLOCK glck(*pc->spage);
                    pc->spage->check_state();
                }
#endif

                bool tst = false;

                {
                    uints avsize;
                    ssegpage::SEGLOCK glck( *pc->spage, true, &avsize );

                    if( glck.locked()  &&  avsize >= size )
                    {
                        tst = true;

                        HEADER* hdr = (HEADER*) pc->spage->alloc(size);
                        if(hdr)
                        {
                            hdr->_count = count;

                            //expecting atomic write
                            _last_succ = pc;

                            //CLAIM( _pages[i]->check_state(), 0 );
                            return hdr;
                        }
                    }
                }

                pc = pc->prev;
                if(!pc)  pc = _last;
                if( pc == pf )
                    break;
            }
        }

        page* pn = SINGLETON( chunk_allocator<page> ).alloc();
        pn->spage = ssegpage::create( _pgcreatemutex.is_set(), _segsize );

        // allocate the memory
        HEADER* hdr;
        hdr = (HEADER*) pn->spage->alloc(size);
        if(!hdr)
            throw ersEXCEPTION "not enough memory";

        hdr->_count = count;

        {
            IFGUARD(_pgcreatemutex);
            ++_nseg;

            pn->prev = _last;
            _last = pn;
            _last_succ = pn;
        }

        return hdr;
    }
}

////////////////////////////////////////////////////////////////////////////////
seg_allocator::HEADER* seg_allocator::realloc( seg_allocator::HEADER* p, uints count, uints chunk, bool keep )
{
    DASSERT( p->_count != 0xcdcdcdcd );

    if(!p)
        return alloc(count, chunk);

    uints size = count*chunk + REDHDRSIZE;
    if( !p->is_base_set() )
    {
        //was allocated using big alloc
        if( size >= _segsize/2 )
        {
            HEADER* hdr = (HEADER*) ssegpage::realloc_big( p, size, keep );
            if(!hdr)
                throw ersEXCEPTION "comm/realloc: not enough memory";

            hdr->_count = count;
            return hdr;
        }
        else
        {
            if(!keep)
                ssegpage::free(p);

            HEADER* pn = alloc( count, chunk );

            if(keep) {
                xmemcpy( pn+1, p+1, p->_count < pn->_count ? p->_count*chunk : pn->_count*chunk );
                ssegpage::free(p);
            }

            return pn;
        }
    }
    else
    {
        //was allocated using segmented alloc
        if( size >= _segsize/2 )
        {
            if(!keep)
                ssegpage::free(p);

            HEADER* hdr;
            hdr = (HEADER*) ssegpage::alloc_big(size);
            if(!hdr)
                throw ersEXCEPTION "comm/alloc: not enough memory";

            hdr->_count = count;

            if(keep) {
                xmemcpy( hdr+1, p+1, p->_count*chunk );
                ssegpage::free(p);
            }

            return hdr;
        }
        else
        {
            HEADER* hdr = (HEADER*) ssegpage::realloc( p, size, keep );
            if(hdr)
                hdr->_count = count;
            else
            {
                if(!keep)
                    ssegpage::free(p);

                HEADER* hdr = alloc( count, chunk );

                if(keep) {
                    xmemcpy( hdr+1, p+1, p->_count < count ? p->_count*chunk : count*chunk );
                    ssegpage::free(p);
                }

                return hdr;
            }

            return hdr;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
seg_allocator::HEADER* seg_allocator::allocset( uints count, uints chunk, uchar set )
{
    HEADER* h = alloc( count, chunk );
    memset( h+1, set, h->get_usable_size() );
    return h;
}

////////////////////////////////////////////////////////////////////////////////
seg_allocator::HEADER* seg_allocator::reallocset( HEADER* p, uints count, uints chunk, bool keep, uchar set )
{
    uints ocount = p->_count;

    HEADER* h = realloc( p, count, chunk, keep );
    if( keep  &&  count > ocount )
        memset( (char*)(h+1) + ocount*chunk, set, h->get_usable_size() - ocount*chunk );
    else if( !keep )
        memset( h+1, set, h->get_usable_size() );
    return h;
}

////////////////////////////////////////////////////////////////////////////////
seg_allocator::HEADER* seg_allocator::reserve (HEADER* p, uints count, uints chunk, bool keep)
{
    if(!p) {
        p = alloc( count, chunk );
        p->_count = 0;
    }
    else {
        uints ocount = p->_count;
        p = realloc( p, count, chunk, keep );
        p->_count = ocount < count ? ocount : count;
    }
    return p;
}

////////////////////////////////////////////////////////////////////////////////
seg_allocator::HEADER* seg_allocator::reserveset( HEADER* p, uints count, uints chunk, bool keep, uchar set )
{
    if(!p) {
        p = allocset( count, chunk, set );
        p->_count = 0;
    }
    else {
        uints ocount = p->_count;
        p = reallocset( p, count, chunk, keep, set );
        p->_count = ocount < count ? ocount : count;
    }
    return p;
}

////////////////////////////////////////////////////////////////////////////////
void  seg_allocator::free( seg_allocator::HEADER* p )
{
    if(!this)  return;

    ssegpage::free(p);
}


} // namespace coid
