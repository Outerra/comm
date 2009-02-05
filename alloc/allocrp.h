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

#ifndef __COID_COMM_ALLOCRP__HEADER_FILE__
#define __COID_COMM_ALLOCRP__HEADER_FILE__

#include "../namespace.h"

#include "sseg.h"
#include "chunkalloc.h"
#include "../dynarray.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
template <class T>
class restricted_page_allocator
{
public:
    ssegpage* get_page( uints n )
    {
        if( n == UMAXS )
            return 0;

        if( n < _pages.size() )
        {
            if( _pages[n].is_set() )
                return _pages[n];
        }
        else
            _pages.realloc( n+1 );

#ifdef _DEBUG
        ++_npages;
#endif
        _pages[n] = ssegpage::create( false, _segsize );
        return _pages[n];
    }

    
    void release_page( uints pg )
    {
        if( pg == UMAXS )  return;
        DASSERT( pg < _pages.size() );

#ifdef _DEBUG
        --_npages;
#endif

        _pages[pg].destroy();
    }


    void reset_page( uints pg )
    {
        ssegpage* ssp = get_page(pg);
        ssp->reset();
    }


    bool is_in_page( const dynarray<T>& a, uints pg ) const
    {
        RASSERT( pg < _pages.size() );
        ssegpage* spg = _pages[pg];
        return spg  &&  spg->can_be_valid(a._ptr);
    }


    uints get_segsize() const        { return _segsize; }

private:

#ifdef _DEBUG
    uints _npages;
#endif

    uints _segsize;
    dynarray< local<ssegpage> >  _pages;


public:

    //TODO: destroy previous content
    opcd set_page_size( uints segsize )
    {
        _segsize = nextpow2(segsize);
        _pages.alloc(32);
        return 0;
    }

    bool is_page_allocated( uints p ) const          { return p<_pages.size()  &&  _pages[p].is_set(); }
    bool is_page_empty( uints p ) const              { return p<_pages.size()  &&  _pages[p]->is_empty(); }

    bool reserve( uints n )
    {
        return _pages.reserve( n, true ) != 0;
    }

    restricted_page_allocator( uints segsize = 65536 )
    {
        _segsize = nextpow2(segsize);
        _pages.reserve(32,false);

#ifdef _DEBUG
        _npages = 0;
#endif
    }
};


////////////////////////////////////////////////////////////////////////////////
template <class T>
class limited_page_allocator
{
public:
    ssegpage* get_page( uints n ) const
    {
        if( n == UMAXS )
            return 0;
        DASSERT( n < _npg );
        return (ssegpage*) (_mem + n*_segsize);
    }

    void release_page( uints n )
    {
        if( n == UMAXS )
            return;
        ssegpage* sp = get_page(n);
        sp->reset();
    }

    bool is_in_page( const dynarray<T>& a, uints pg ) const
    {
        ssegpage* spg = get_page(pg);
        return spg && spg->can_be_valid(a.ptr());
    }
    
    uints get_segsize() const        { return _segsize; }
    uints get_npages() const         { return _npg; }

private:

    char* _mem;
    uints _npg;
    uints _segsize;

public:

    limited_page_allocator()
    {
        _npg = 0;
        _segsize = 0;
        _mem = 0;
    }

    limited_page_allocator( uints segsize, uints pagesize )
    {

        init( segsize, pagesize );
    }

    opcd init( uints segsize, uints pages )
    {
        _segsize = nextpow2(segsize);
        _mem = (char*) ::malloc( _segsize * pages );
        if(!_mem)  return ersNOT_ENOUGH_MEM;

        _npg = pages;
        char* p = _mem;
        for( uints i=0; i<pages; ++i, p+=_segsize )
            ssegpage::create( p, false, _segsize );
        return 0;
    }

    ~limited_page_allocator()
    {
        if(_mem)
        {
            char* p = _mem;
            for( uints i=0; i<_npg; ++i, p+=_segsize )
            {
                ((ssegpage*)p)->~ssegpage();
            }
        
            ::free(_mem);
        }
    }
};


////////////////////////////////////////////////////////////////////////////////
COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_ALLOCRP__HEADER_FILE__

