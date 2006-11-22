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

#ifndef __COID_COMM_CHUNKPAGE__HEADER_FILE__
#define __COID_COMM_CHUNKPAGE__HEADER_FILE__

#include <new>
#include "../namespace.h"

#include "../commtypes.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Helper template that provides element_size() function for all types,
/// but for void type it actually allows setting custom size in bytes
template<class T>
struct ElementSizeInfo
{
    static const uints size = sizeof(T);
    void set_size( uints bytes ) {}
    static T* create( void* p )  { return new(p) T; }
};

///Specialization for void allowing custom byte size
template<>
struct ElementSizeInfo<void>
{
    uints size;
    void set_size( uints bytes ) { size = bytes; }
    static void* create( void* p )  { return p; }
};

////////////////////////////////////////////////////////////////////////////////
///Template allowing to extend allocator classes with additional members (for locking etc.)
template<class T> struct ExtendClassMember { typedef T Type; T extend; };

///Specialization for void that doesn't insert anything
template<> struct ExtendClassMember<void>  { typedef void Type; };


////////////////////////////////////////////////////////////////////////////////
///Allocator class that manages memory blocks of fixed size within one, external
/// memory page.
/// The items are referenced by void pointers. Each item has an unique id and is
/// obtainable through it.
template<class T>
class chunkpage
{
    void* _mem;
    uints _first;               ///< first free chunk offset or zero when none free
    uint _numfree;              ///< number of free chunks
    ints _mode;                 ///< contains either non-negative value pointing to the first free chunk or -1 when working as list
    uints _pagesize;

    ElementSizeInfo<T> item;

public:

    enum {
        PAGESIZE            = 0x10000,          //default page size
    };

    void reset()
    {
        _first = UMAX;
        if(item.size)
            _numfree = _pagesize/item.size;
        else
            _numfree = 0;

        _mode = 0;              //direct mode initially
    }

    chunkpage()
    {
        _pagesize = 0;
        _mem = 0;
        _numfree = 0;
        _first = UMAX;
    }

    ~chunkpage()
    {
        if(_mem)
            ::free(_mem);
    }

    static chunkpage* create( uints itemsize )
    {
        chunkpage* p = new chunkpage;
        p->init( PAGESIZE, itemsize );
        return p;
    }

    opcd init( uints pagesize, uints itemsize = sizeof(T) )
    {
        if( _mem && _pagesize != pagesize ) {
            ::free(_mem);
            _mem = 0;
        }

        if(!_mem)
            _mem = ::malloc(pagesize);
        if(!_mem)
            return ersNOT_ENOUGH_MEM;

        _pagesize = pagesize;
        item.set_size( itemsize );

        reset();
        return 0;
    }
    

    T* alloc()
    {
        if( _numfree == 0 )
            return 0;

        T* p;
        if( _mode >= 0 )    //working in direct mode
        {
            p = (T*)((char*)_mem + _mode);
            _mode += item.size;
            if( (uints)_mode > _pagesize-item.size )    //out of direct memory, switch to the list mode
                _mode = -1;
        }
        else
        {
            uints* pn = (uints*) ( (char*)_mem + _first );
            _first = *pn;
            p = (T*)pn;
        }

        --_numfree;
        return p;
    }

    void free( T* p )
    {
        ints n = (char*)p - (char*)_mem;
        DASSERT( n>=0  &&  n < _pagesize ); //out of page range
        DASSERT( n%item.size == 0 );        //misaligned pointer

        *(uints*)p = _first;
        _first = n;

        ++_numfree;
    }

    T* get_item( uints n ) const
    {
        DASSERT( n*_itemsize < _pagesize );
        return (T*)((char*)_mem + n*item.size);
    }

    uints get_item_id( const T* p ) const
    {
        uints id = (char*)p - (char*)_mem;
        DASSERT( id < _pagesize );
        return id / item.size;
    }

    uints get_itemsize() const   { return item.size; }
};


COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_CHUNKPAGE__HEADER_FILE__

