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

#ifndef __COID_MEMTRACK__HEADER_FILE__
#define __COID_MEMTRACK__HEADER_FILE__

#include "../namespace.h"
#include "../commtypes.h"

#include <algorithm>


#if defined(_DEBUG) || COID_USE_MEMTRACK

#define MEMTRACK_ALLOC(name, size) coid::memtrack_alloc(name, size)
#define MEMTRACK_FREE(name, size) coid::memtrack_free(name, size)

#define MEMTRACK_ENABLED

#else

#define MEMTRACK_ALLOC(name, size)
#define MEMTRACK_FREE(name, size)

#endif


COID_NAMESPACE_BEGIN

struct memtrack {
    uints size;                         //< size allocated since the last memtrack_list call
    uints total;                        //< total allocated size
    uint nallocs;                       //< number of allocations since the last memtrack_list call
    uint ntotalallocs;
    const char* name;                   //< class identifier

    void swap(memtrack& m) {
        std::swap(size, m.size);
        std::swap(total, m.total);
        std::swap(nallocs, m.nallocs);
        std::swap(ntotalallocs, m.ntotalallocs);
        std::swap(name, m.name);
    }

    memtrack() : size(0), total(0), nallocs(0), ntotalallocs(0), name(0) {}
};


///Track allocation request for name
void memtrack_alloc( const char* name, uints size );

///Track allocation request for name
void memtrack_free( const char* name, uints size );

///List allocation request statistics from last call
uint memtrack_list( memtrack* dst, uint nmax );

uint memtrack_count();

///Dump info into file
void memtrack_dump( const char* file );

///Reset tracking info
void memtrack_reset();

void memtrack_shutdown();

COID_NAMESPACE_END

#endif //#ifndef __COID_MEMTRACK__HEADER_FILE__
