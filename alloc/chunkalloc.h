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

#ifndef __COID_COMM_CHUNKALLOC__HEADER_FILE__
#define __COID_COMM_CHUNKALLOC__HEADER_FILE__

#include "../namespace.h"

#include "../sync/mutex.h"
#include "../commtypes.h"
#include "chunkblock.h"

COID_NAMESPACE_BEGIN



////////////////////////////////////////////////////////////////////////////////
///Allocator of typed chunks. Size of blocks to be managed is obtained from sizeof(T)
template<class T>
class chunk_allocator
{
    typedef chunkblock<T,comm_mutex>    block;

    struct page
    {
        block*  chunk;
        page*   prev;
    };

    page* _last;

    ElementCreator<T> item;
    mutable comm_mutex _mutex;

public:

    chunk_allocator()
    {
        _last = 0;
		_mutex.set_name( "chunk_allocator" );
    }

    T* alloc()
    {
        GUARDME;

        void* pv;
        for( page* p=_last; p; p=p->prev )
        {
            {GUARDTHIS(p->chunk->ext.member); pv = p->chunk->alloc_nocreate();}
            if(pv)  return item.create(pv,true);
        }

        page* pn = new page;
        pn->prev = _last;
        pn->chunk = block::create((uint)item.size);
        _last = pn;

        {GUARDTHIS(pn->chunk->ext.member); pv = pn->chunk->alloc_nocreate();}
        return item.create(pv,true);
    }

    static void free( T* p )
    {
        ASSERT_RETVOID(p);
        block* page = block::get_segchunk(p);
        p->~T();

        GUARDTHIS(page->ext.member);
        page->free_nodestroy(p);
    }

    bool check( T* p ) const
    {
        block* seg = block::get_segchunk(p);
        for( page* pg=_last; pg; pg=pg->prev )
            if( pg->chunk == seg )  return true;
        return false;
    }
};

COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_CHUNKALLOC__HEADER_FILE__
