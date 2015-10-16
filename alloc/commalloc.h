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

#ifndef __COID_COMMALLOC__HEADER_FILE__
#define __COID_COMMALLOC__HEADER_FILE__

#include "../namespace.h"
#include "../singleton.h"
#include "memtrack.h"
#include <exception>

#define USE_DL_PREFIX
#define MSPACES 1
#include "./_malloc.h"

#define COIDNEWDELETE(name) \
    void* operator new( size_t size ) { \
        void* p=::dlmalloc(size); \
        if(p==0) throw std::bad_alloc(); \
        MEMTRACK_ALLOC(name, dlmalloc_usable_size(p)); \
        return p; } \
    void* operator new( size_t, void* p ) { return p; } \
    void operator delete(void* p) { \
        MEMTRACK_FREE(name, dlmalloc_usable_size(p)); \
        ::dlfree(p); } \
    void operator delete(void*, void*)  { }

#define COIDNEWDELETE_ALIGN(name,alignment) \
    void* operator new( size_t size ) { \
        void* p=::dlmemalign(alignment,size); \
        if(p==0) throw std::bad_alloc(); \
        MEMTRACK_ALLOC(name, dlmalloc_usable_size(p)); \
        return p; } \
    void* operator new( size_t, void* p ) { return p; } \
    void operator delete(void* p) { \
        MEMTRACK_FREE(name, dlmalloc_usable_size(p)); \
        ::dlfree(p); } \
    void operator delete(void*, void*)  { }

#define COIDNEWDELETE_NOTRACK \
    void* operator new( size_t size ) { \
        void* p=::dlmalloc(size); \
        if(p==0) throw std::bad_alloc(); \
        return p; } \
    void* operator new( size_t, void* p ) { return p; } \
    void operator delete(void* ptr)     { ::dlfree(ptr); } \
    void operator delete(void*, void*)  { }


COID_NAMESPACE_BEGIN

void* memaligned_alloc( size_t size, size_t alignment );
void memaligned_free( void* p );
uints memaligned_used();

////////////////////////////////////////////////////////////////////////////////
template<class T>
struct comm_allocator
{
    static T* alloc()       { return (T*)::dlmalloc( sizeof(T) ); }
    static void free(T* p)  { ::dlfree(p); }
};

////////////////////////////////////////////////////////////////////////////////
struct comm_array_mspace
{
    comm_array_mspace() {
        msp = ::create_mspace(0, true, 16-sizeof(uints));
    }

    ~comm_array_mspace() {
        ::destroy_mspace(msp);
    }

    void singleton_initialize_module() {
        dlmalloc_ensure_initialization();
    }

    ::mspace msp;
};

static_assert(
    has_singleton_initialize_module_method<comm_array_mspace>::value != 0,
    "error" );

////////////////////////////////////////////////////////////////////////////////
struct comm_array_allocator
{
    COIDNEWDELETE("comm_array_allocator");


    static void instance() {
        SINGLETON(comm_array_mspace);
    }

    ///Typed array alloc
    template<class T>
    static T* alloc( uints n ) { return (T*)alloc(n, sizeof(T), typeid(T).name()); }

    ///Typed array realloc
    template<class T>
    static T* realloc( const T* p, uints n ) { return (T*)realloc(p, n, sizeof(T), typeid(T).name()); }

    ///Typed array add
    template<class T>
    static T* add( const T* p, uints n ) { return (T*)add(p, n, sizeof(T), typeid(T).name()); }

    ///Typed array free
    template<class T>
    static void free( const T* p ) { return free(p, typeid(T).name()); }



    ///Untyped array alloc
    static void* alloc( uints n, uints elemsize, const char* trackname = "comm_array_allocator.untyped" ) {
        uints* p = (uints*)::mspace_malloc(SINGLETON(comm_array_mspace).msp,
            sizeof(uints) + n * elemsize);

        MEMTRACK_ALLOC(trackname, ::mspace_usable_size(p));

        if(!p) throw std::bad_alloc();
        p[0] = n;
        return p + 1;
    }

    ///Untyped array realloc
    static void* realloc( const void* p, uints n, uints elemsize, const char* trackname = "comm_array_allocator.untyped" ) {

        if(!p)
            return alloc(n, elemsize, trackname);

        MEMTRACK_FREE(trackname, ::mspace_usable_size((uints*)p - 1));

        uints* pn = (uints*)::mspace_realloc(SINGLETON(comm_array_mspace).msp,
            p ? (uints*)p - 1 : 0,
            sizeof(uints) + n * elemsize);
        if(!pn) throw std::bad_alloc();

        MEMTRACK_ALLOC(trackname, ::mspace_usable_size(pn));

        pn[0] = n;
        return pn + 1;
    }

    ///Untyped array free
    static void free( const void* p, const char* trackname = "comm_array_allocator.untyped" ) {
        if(!p)  return;

        MEMTRACK_FREE(trackname, ::mspace_usable_size((uints*)p - 1));
        ::mspace_free(SINGLETON(comm_array_mspace).msp, (uints*)p - 1);
    }

    ///Untyped uninitialized add
    //@return pointer to array
    static void* add( const void* p, uints nitems, uints elemsize, const char* trackname = "comm_array_allocator.untyped" )
    {
        uints n = count(p);
        DASSERT( n+nitems <= UMAXS );

        if(!nitems)
            return const_cast<void*>(p);

        uints nto = nitems + n;
        uints nalloc = nto;
        uints s = size(p);

        void* np = const_cast<void*>(p);

        if(nalloc*elemsize > s) {
            if( nalloc < 2 * n )
                nalloc = 2 * n;

            np = realloc(p, nalloc, elemsize, trackname);
        }

        set_count(np, nto);
        return np;
    };


    static uints size( const void* p ) {
        return p
            ? (dlmalloc_usable_size( (const uints*)p - 1 ) - sizeof(uints))
            : 0;
    }

    static uints count( const void* p ) {
        return p
            ? *((const uints*)p - 1)
            : 0;
    }

    static uints set_count( const void* p, uints n )
    {
        *((uints*)p - 1) = n;
        return n;
    }
};

////////////////////////////////////////////////////////////////////////////////
COID_NAMESPACE_END

#endif //#ifndef __COID_COMMALLOC__HEADER_FILE__
