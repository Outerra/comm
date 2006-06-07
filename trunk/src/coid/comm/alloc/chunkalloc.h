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

#include "../commtypes.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
class chunkalloc
{
    uints _first;
    uints _itemsize;
    uints _pagesize;
    uints _numfree;

    void* _mem;

public:

    void reset()
    {
        _first = 0;
        if(_itemsize)
            _numfree = _pagesize/_itemsize;
        else
            _numfree = 0;

		if( _mem ) {
			uchar* p = (uchar*)_mem;
			uints a;
			for( a=0; a<_pagesize; a+=_itemsize )
			{
				uints* pn = (uints*)(p+a);
				*pn = a + _itemsize;
			}

			a -= _itemsize;
			*(uints*)(p+a) = UMAX;
		}
    }

    chunkalloc()
    {
        _pagesize = _itemsize = 0;
        _mem = 0;
        _first = _numfree = 0;
    }

    ~chunkalloc()
    {
        if(_mem)
            ::free(_mem);
    }

    opcd init( uints itemsize, uints pagesize )
    {
        if(_mem)
            ::free(_mem);

        _mem = ::malloc(pagesize);
        if(!_mem)
            return ersNOT_ENOUGH_MEM;

        _pagesize = pagesize;
        _itemsize = itemsize;
        _first = 0;

        reset();
        return 0;
    }
    

    void* alloc()
    {
        if( _numfree == 0 )
            return 0;

        uints* pn = (uints*) ( (char*)_mem + _first );

        _first = *pn;
        --_numfree;

        return pn;
    }

    void free( void* p )
    {
        uints n = (char*)p - (char*)_mem;
        RASSERT( n < _pagesize );
        RASSERTX( ( n % _itemsize ) == 0, "invalid pointer" );

        *(uints*)p = _first;
        _first = n;

        ++_numfree;
    }

    void* get_item( uints n ) const
    {
        DASSERT( n*_itemsize < _pagesize );
        return ((char*)_mem + n*_itemsize);
    }

    uints get_item_id( const void* p ) const
    {
        uints id = (char*)p - (char*)_mem;
        DASSERT( id < _pagesize );
        return id / _itemsize;
    }

    uints get_itemsize() const   { return _itemsize; }
};


COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_CHUNKALLOC__HEADER_FILE__

