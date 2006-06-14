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

#include "segchunk.h"
#include "../retcodes.h"
//#include "seg.h"

namespace coid {

////////////////////////////////////////////////////////////////////////////////
void* segchunk::operator new (size_t)
{
    return memaligned_alloc( SEGSIZE, PAGESIZE );
}

////////////////////////////////////////////////////////////////////////////////
void segchunk::operator delete (void * ptr)
{
    memaligned_free( ptr );
}

////////////////////////////////////////////////////////////////////////////////
segchunk* segchunk::create( uint rsize )
{
    return new segchunk(rsize);
}

////////////////////////////////////////////////////////////////////////////////
segchunk::segchunk( uint rsize )
{
    RASSERTX( (1UL<<rsize) >= sizeof(int), "element size too small" );
    RASSERTX( rsize < rPAGESIZE, "too big element" );
    _rsize = rsize;
    reset();
	_mx.set_name( "segchunk" );
}

////////////////////////////////////////////////////////////////////////////////
void segchunk::reset()
{
    _me = this;
    _first = align_value_to_power2( sizeof(segchunk), _rsize );
    _numfree = (SEGSIZE - _first) >> _rsize;

    uint size = 1<<_rsize;
    uint alim = SEGSIZE - size;

    for( uint a=_first; a<alim; a+=size )
    {
        uint* pn = (uint*) ( (char*)this + a );

        if( ((a+size) & (PAGESIZE-1)) == 0 )
            *pn = a + 2*size;
        else if( (a & (PAGESIZE-1)) == 0 )
        {
            *pn = (uint)this;       //make islands
            --_numfree;
        }
        else
            *pn = a + size;
    }

    uint* pn = (uint*) ( (char*)this + alim );
    *pn = 0;
}

////////////////////////////////////////////////////////////////////////////////
void* segchunk::alloc()
{
    MXGUARD(_mx);

    if( _numfree == 0 )
        return 0;

    uint* pn = (uint*) ( (char*)this + _first );
    DASSERTX( (_numfree == 1  &&  *pn == 0)  ||  *pn >= align_value_to_power2( sizeof(segchunk), _rsize )  &&  *pn < SEGSIZE, "segment structure damaged" );

    _first = *pn;
    --_numfree;

    return pn;
}

////////////////////////////////////////////////////////////////////////////////
void segchunk::free( void* p )
{
    MXGUARD(_mx);

    RASSERTX( p >= (char*)this + align_value_to_power2( sizeof(segchunk), _rsize )  &&  p < (char*)this + SEGSIZE, "invalid pointer" );

    uint n = (char*)p - (char*)this;
    RASSERTX( (n & ((1<<_rsize)-1)) == 0, "misaligned pointer" );

    *(uint*)p = _first;
    _first = n;

    ++_numfree;
}


} // namespace coid
