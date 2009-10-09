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
 * Brano Kemen
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "memtrack.h"
#include "../hash/hashkeyset.h"
#include "../singleton.h"
#include "../atomic/atomic.h"
#include "../sync/mutex.h"

namespace coid {

struct memtrack_imp : memtrack {
    bool operator == (uints k) const {
        return (uints)name == k;
    }

    operator uints() const {
        return (uints)name;
    }

    //don't track this
    void* operator new( size_t size ) { return ::dlmalloc(size); }
    void operator delete(void* ptr)   { ::dlfree(ptr); } \
};

typedef hash_keyset<memtrack_imp, _Select_Copy<memtrack_imp,uints> > memtrack_hash_t;

struct memtrack_registrar {
    memtrack_hash_t hash;
    comm_mutex mux;
};


////////////////////////////////////////////////////////////////////////////////
#ifdef _DEBUG

static int reg=0;

////////////////////////////////////////////////////////////////////////////////
memtrack_registrar* memtrack_register()
{
    //avoid stack overflow
    static memtrack_registrar* mtr=0;
    if(reg < 0)
        return 0;
    if(!reg) {
        reg = -1;
        mtr = new memtrack_registrar;
        reg = 1;
    }

    return mtr;
}

////////////////////////////////////////////////////////////////////////////////
void memtrack_shutdown()
{
    memtrack_registrar* mtr = memtrack_register();
    reg = -2;

    delete mtr;
}

////////////////////////////////////////////////////////////////////////////////
void memtrack_alloc( const char* name, uints size )
{
    memtrack_registrar* mtr = memtrack_register();
    if(!mtr) return;

    GUARDTHIS(mtr->mux);
    memtrack* val = mtr->hash.find_or_insert_value_slot((uints)name);
    val->name = name;

    ++val->nallocs;
    val->size += size;
    val->total += size;
}

////////////////////////////////////////////////////////////////////////////////
uint memtrack_list( memtrack* dst, uint nmax )
{
    memtrack_registrar* mtr = memtrack_register();

    GUARDTHIS(mtr->mux);
    memtrack_hash_t::iterator ib = mtr->hash.begin();
    memtrack_hash_t::iterator ie = mtr->hash.end();

    uint i=0;
    for( ; ib!=ie && i<nmax; ++ib ) {
        memtrack& p = *ib;
        if(p.nallocs == 0)
            continue;

        dst[i++] = p;
        p.nallocs = 0;
        p.size = 0;
    }

    return i;
}

void memtrack_reset()
{
    memtrack_registrar* mtr = memtrack_register();

    GUARDTHIS(mtr->mux);
    memtrack_hash_t::iterator ib = mtr->hash.begin();
    memtrack_hash_t::iterator ie = mtr->hash.end();

    for( ; ib!=ie; ++ib ) {
        memtrack& p = *ib;
        p.nallocs = 0;
        p.size = 0;
    }
}

#else
void memtrack_shutdown() {}
#endif //_DEBUG

} //namespace coid
