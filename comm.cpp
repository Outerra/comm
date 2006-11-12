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

#include "comm.h"

#ifdef USE_COMM_ALLOC

COID_NAMESPACE_BEGIN
/*
void * operator new(size_t size) NEWTHROWTYPE {
    comm_allocator::instance ();
    void* p = (comm_allocator::_alloc->alloc(size, 1) + 1);
    return p;
}

void operator delete(void * ptr) DELTHROWTYPE {
    if(ptr)
        comm_allocator::_alloc->free( (seg_allocator::HEADER*)ptr - 1 );
}

void * operator new[] (size_t size) NEWTHROWTYPE {
    comm_allocator::instance();
    void* p = (comm_allocator::_alloc->alloc(size, 1) + 1);
    return p;
}

void operator delete[] (void *ptr) DELTHROWTYPE {
    if (ptr)
        comm_allocator::_alloc->free( (seg_allocator::HEADER*)ptr - 1 );
}
*/
COID_NAMESPACE_END

#else
#include <malloc.h>

COID_NAMESPACE_BEGIN
/*
void * operator new(size_t size) NEWTHROWTYPE {
    return malloc (size);
}

void operator delete(void * ptr) DELTHROWTYPE {
    free (ptr);
}

void * operator new[] (size_t size) NEWTHROWTYPE {
    return malloc (size);
}

void operator delete[] (void *ptr) DELTHROWTYPE {
    free (ptr);
}
*/
COID_NAMESPACE_END

#endif // USE_COMM_ALLOC




#include "dynarray.h"

COID_NAMESPACE_BEGIN

uints _Gmemused = 0;


////////////////////////////////////////////////////////////////////////////////
void *_xmemcpy( void *dest, const void *src, size_t count ) {
    return ::memcpy( dest, src, count );
}

////////////////////////////////////////////////////////////////////////////////
bool cdcd_memcheck( const uchar* a, const uchar* ae, const uchar* b, const uchar* be )
{
    const uchar* p = (const uchar*)::memchr( a, 0xcd, ae-a );
    if(!p)
    {
        if( be == b )  return true;
        return cdcd_memcheck( b, be, 0, 0 );
    }
    else if( p == ae-1 )
    {
        if( be == b )  return true;
        if( *b == 0xcd )  return false;
        return cdcd_memcheck( b+1, be, 0, 0 );
    }
    else if( p[1] == 0xcd )
        return false;
    else
        return cdcd_memcheck( a+2, ae, b, be );
}

////////////////////////////////////////////////////////////////////////////////
uints memaligned_used()
{
    return _Gmemused;
}

////////////////////////////////////////////////////////////////////////////////
void * memaligned_alloc( size_t size, size_t alignment )
{
	size_t ptr, r_ptr;
	size_t *reptr;

    if( size >= (size_t)-(int)(alignment+sizeof(void*)) )
        return 0;

	if( !IS_2_POW_N(alignment) ) {
		RASSERTX( 0, "alignment must be a power of 2" );
		return 0;
	}

	alignment = (alignment > sizeof(void *) ? alignment : sizeof(void *));

	if ((ptr = (size_t)malloc(size + alignment + sizeof(void *))) == (size_t)NULL)
		return NULL;

#ifdef SYSTYPE_MSVC
	_Gmemused += _msize( (void*)ptr );
#else
	_Gmemused += malloc_usable_size( (void*)ptr );
#endif

	r_ptr = (ptr + alignment + sizeof(void *)) & ~(alignment -1);
	reptr = (size_t *)(r_ptr - sizeof(void *));
	*reptr = ptr;

	return (void *)r_ptr;
}

////////////////////////////////////////////////////////////////////////////////
void * memaligned_realloc( void * memblock, size_t size, size_t alignment )
{
	size_t ptr, r_ptr, s_ptr;
	size_t *reptr;
	size_t mov_sz;

	if (memblock == NULL)
		return NULL;

    if( size >= (size_t)-(int)(alignment+sizeof(void*)) )
        return 0;

	s_ptr = (size_t)memblock;

	/* ptr points to the pointer to starting of the memory block */
	s_ptr = (s_ptr & ~(sizeof(void *) -1)) - sizeof(void *);

	/* ptr is the pointer to the start of memory block*/
	s_ptr = *((size_t *)s_ptr);

	if (!IS_2_POW_N(alignment))
	{
		RASSERTX( 0, "alignment must be a power of 2" );
		return NULL;
	}

	alignment = (alignment > sizeof(void *) ? alignment : sizeof(void *));

	// Calculate the size that is needed to move
#ifdef SYSTYPE_MSVC
	mov_sz = _msize( (void *)s_ptr) - ((size_t)memblock - s_ptr );
#else
	mov_sz = malloc_usable_size( (void *)s_ptr) - ((size_t)memblock - s_ptr );
#endif

    _Gmemused -= mov_sz;

	if ((ptr = (size_t)malloc(size + alignment + sizeof(void *))) == (size_t)NULL)
		return NULL;

#ifdef SYSTYPE_MSVC
	_Gmemused += _msize( (void*)ptr );
#else
	_Gmemused += malloc_usable_size( (void*)ptr );
#endif

	r_ptr = (ptr + alignment + sizeof(void *)) & ~(alignment -1);
	reptr = (size_t *)(r_ptr - sizeof(void *));
	*reptr = ptr;

	/* copy the content from older memory location to newer one */
	xmemcpy((void *)r_ptr, memblock, mov_sz > size ? size: mov_sz);
	free((void *)s_ptr);

	return (void *)r_ptr;
}

////////////////////////////////////////////////////////////////////////////////
void memaligned_free( void * memblock )
{
	size_t ptr;

	if (memblock == NULL)
		return;

	ptr = (size_t)memblock;

	/* ptr points to the pointer to start of the memory block */
	ptr = (ptr & ~(sizeof(void *) -1)) - sizeof(void *);

	/* ptr is the pointer to the start of memory block*/
	ptr = *((size_t *)ptr);

#ifdef SYSTYPE_MSVC
	_Gmemused -= _msize( (void*)ptr );
#else
	_Gmemused -= malloc_usable_size( (void*)ptr );
#endif

	free((void *)ptr);
}

////////////////////////////////////////////////////////////////////////////////
///Get fresh array with \a nitems of elements
/** Adjusts the array to the required size
    @note only trivial types can be allocated this way (no constructor)
    @param nitems number of items to resize to
    @param itemsize size of item in bytes
    @param ralign granularity (exponent of two) of reserved items
    @return pointer to the first element of array */
void* dynarray_new( void* p, uints nitems, uints itemsize, uints ralign )
{
    uints nalloc;
    if( ralign > 0 )
        nalloc = get_aligned_size(nitems, ralign);
    else
        nalloc = nitems;

    uints size = p ? ((seg_allocator::HEADER*)p-1)->get_usable_size() : 0;

    if( nalloc*itemsize > size )
        p = SINGLETON(seg_allocator).reserve( p ? (seg_allocator::HEADER*)p-1 : (seg_allocator::HEADER*)p,
            nalloc, itemsize, true ) + 1;

    if(p)
        comm_allocator<uchar>::set_count( (uchar*)p, nitems );

    if(nitems) {
#ifdef _DEBUG
        if (nalloc*itemsize > size)
            ::memset( (uchar*)p+size, 0xcd, nalloc*itemsize - size );
#endif
    }

    return p;
};

COID_NAMESPACE_END
