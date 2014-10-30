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
#include "../retcodes.h"
#include "../commassert.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Helper template that provides size info for all types,
/// but for void type it actually allows setting custom size in bytes
template<class T>
struct ElementCreator
{
    static const uints size = sizeof(T);

    void set_size( uints bytes )                    { }
    static T* create( void* p, bool firsttime )     { return new(p) T; }
    static void destroy( void* p, bool final )      { delete (T*)p; }
};

///Specialization for void allowing custom byte size
template<>
struct ElementCreator<void>
{
    uints size;

    void set_size( uints bytes )                    { size = bytes; }
    static void* create( void* p, bool firsttime )  { return p; }
    static void destroy( void* p, bool final )      { }
};

////////////////////////////////////////////////////////////////////////////////
///Template allowing to extend allocator classes with additional members (for locking etc.)
template<class T> struct ExtendClassMember { typedef T Type; T member; };

///Specialization for void that doesn't insert anything
template<> struct ExtendClassMember<void>  { typedef void Type; };


////////////////////////////////////////////////////////////////////////////////
///Allocator class that manages memory blocks of fixed size within one, external
/// memory page.
/// The items are referenced by void pointers. Each item has an unique id and is
/// obtainable through it.
template<class T, class E = ElementCreator<T> >
class chunkpage
{
    void* _mem;
    ints _first;                //< first free chunk offset or zero when none free
    uint _numfree;              //< number of free chunks
    uints _used;                //< contains either non-negative value pointing to the first free chunk or -1 when working as list
    uints _pagesize;

    E item;

public:

    enum {
        PAGESIZE            = 0x10000,          //default page size
    };

    void reset()
    {
        _first = -1;
        if(item.size)
            _numfree = uint(_pagesize/item.size);
        else
            _numfree = 0;

        _used = 0;              //direct mode initially
    }

    chunkpage()
    {
        _pagesize = 0;
        _mem = 0;
        _numfree = 0;
        _first = -1;
    }

    chunkpage( uints pagesize, uints itemsize = sizeof(T) )
    {
        opcd e = init( pagesize, itemsize );
        if(e)  throw e;
    }

    ~chunkpage()
    {
        if(_mem)
            ::dlfree(_mem);
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
            ::dlfree(_mem);
            _mem = 0;
        }

        if(!_mem)
            _mem = ::dlmalloc(pagesize);
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
        bool firsttime = _first < 0;
        if(firsttime)    //working in direct mode
        {
            p = (T*)((char*)_mem + _used);
            _used += item.size;
        }
        else
        {
            uints* pn = (uints*) ( (char*)_mem + _first );
            _first = *pn;
            p = (T*)pn;
        }

        --_numfree;
        return item.create(p,firsttime);
    }

    void free( T* p )
    {
        ints n = (char*)p - (char*)_mem;
        DASSERT( n>=0  &&  n < (ints)_pagesize ); //out of page range
        DASSERT( n%item.size == 0 );        //misaligned pointer

        item.destroy(p,false);

        *(uints*)p = _first;
        _first = n;

        ++_numfree;
    }

    T* get_item( uints n ) const
    {
        DASSERT( n < _used );
        return (T*)((char*)_mem + n*item.size);
    }

    ///Return item pointer or NULL if it never was used
    //@note the method doesn't check if the item wasn't freed already, it returns the item only if
    //@ it was allocated some time from last reset()
    T* get_item_safe( uints n ) const
    {
        if( n >= _used )  return 0;
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




////////////////////////////////////////////////////////////////////////////////
template<class T>
class chunklist
{
    struct element
    {
        T value;
        element* next;
    };

    chunkpage<element>  page;
    element* first;
    element* last;

public:

    void push_back( const T& v )
    {
        element* el = page.alloc();
        el->value = v;
        el->next = 0;
        if(last)
            last->next = el;
        else
            first = el;
        last = el;
    }

    void push_front( const T& v )
    {
        element* el = page.alloc();
        el->value = v;
        el->next = 0;
        if(first)
            el->next = first;
        else
            last = el;
        first = el;
    }

    bool pop_front()
    {
        if(!first)  return false;

        element* el = first->next;
        page.free(first);
        first = el;
        if(!first)
            last = first;
        return true;
    }


    T* get_item( uints n ) const
    {
        const element* el = page.get_item(n);
        return &el->value;
    }

    uints get_item_id( const T* p ) const
    {
        return page.get_item_id( (const element*)p );
    }

};


COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_CHUNKPAGE__HEADER_FILE__

