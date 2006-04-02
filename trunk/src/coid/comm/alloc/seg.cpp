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

#include "seg.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "../comm.h"
#include "../assert.h"

namespace coid {

//enable checking
#if 0
#define SEG_CHECK           check_state()
#define SEG_CHECK_(n)       check_state(n)
#else
#define SEG_CHECK
#define SEG_CHECK_(n)
#endif

////////////////////////////////////////////////////////////////////////////////
void* segpage::operator new ( size_t, uints segsize )
{
    segsize = nextpow2(segsize);
    return memaligned_alloc( segsize, segsize );
}

////////////////////////////////////////////////////////////////////////////////
void segpage::operator delete (void * ptr, uints segsize )
{
    memaligned_free( ptr );
}

////////////////////////////////////////////////////////////////////////////////
segpage::segpage( uints segsize )
{
    _segsize = nextpow2(segsize);
    _nused = 1;
    _rsused = INITIALRESERVEDUSED;
    _pused = (CHUNKI*) get_ptr( get_struct_gran() );
    _wfreebg = _wfreesm = (ushort) (get_struct_gran() + _rsused);

    _pused[0]._offs = get_struct_gran();
    _pused[0]._count = _rsused;

    CHUNKF* p = ptr(_wfreebg);
    p->_next = p->_prev = 0;
    p->set_count( (_segsize>>rGRANULARITY) - _wfreebg );
}

////////////////////////////////////////////////////////////////////////////////
///Return remaining block size, given pointer could point on or in a block allocated herein.
opcd segpage::get_remaining_block_size (
        CHUNK& block     ///<pointer to structure which specifies an offset, and gets filled the size with the remaining size
        ) const
{
    uints j, ul, uu, uoffs;
    CHUNKI *pblocku;
    CHUNKI b;

    import_block( block, b );
    if( !test_offs (b._offs) )  return ersINVALID_PARAMS;

    uoffs = b._offs;
    pblocku = _pused;

    ul=0;  uu = _nused;
    for(; uu-ul > MINBLOCKSTOLINSEARCH;)
    {
        j = (uu+ul)>>1;
        if( pblocku[j]._offs > uoffs )  uu=j;
        else if( pblocku[j]._offs < uoffs )  ul=j;
        else
        {
            block._size = pblocku[j]._count<<rGRANULARITY;
            return 0;
        }
    }

    for( j=ul; j < uu; j++ )
    {
        if (pblocku[j]._offs <= uoffs  &&  pblocku[j]._offs + pblocku[j]._count > uoffs)
        {
            b._offs = pblocku[j]._offs;
            b._count = pblocku[j]._count - uoffs + pblocku[j]._offs;

            export_block (b, block);
            return 0;
        }
    }

    return ersINVALID_PARAMS;
}

////////////////////////////////////////////////////////////////////////////////
///Alloc block of required size.
opcd segpage::alloc (
        CHUNK& block,        ///<structure specifying requred size, gets the offset and actual size
        bool adj
        )
{
    ushort size = align_size(block._size);
    if( size > get_max_size() )  return ersNOT_ENOUGH_MEM;

    //if failed, alloc won't be able to allocate more control structs
    if( _nused == _rsused )  return ersNOT_ENOUGH_MEM;
    if( adj  &&  _nused+1 >= _rsused )
    {
        //try to realloc _pused control array
        if( !adjust(_nused+1) )  return ersNOT_ENOUGH_MEM;
        SEG_CHECK;
        if( size > get_max_size() )  return ersNOT_ENOUGH_MEM;
    }

    SEG_CHECK;

    CHUNKF* pblockf;
    CHUNKI b;

    //take the smallest satisfying block from the free chain
    pblockf = ptr(_wfreesm);
    for( ; pblockf  &&  pblockf->_counta < size; pblockf=nextf(pblockf) );

    RASSERTX( pblockf, "internal structures damaged" );

    //exclude from free chain
    if (pblockf->_counta - size > MINCHUNKSIZE)
    {
        //split
        b._offs = index (pblockf);
        b._count = size;

        CHUNKF* pnf = ptr (index(pblockf) + (ushort)size);
        pnf->set_count (pblockf->_counta - size);
        replace (pblockf, pnf);

        sortf_down (pnf);  //new size always smaller
    }
    else
    {
        b._offs = index (pblockf);
        b._count = pblockf->_counta;

        exclude (pblockf);
    }

    //sort in used
    SEG_CHECK_( b._count );
    sortu (b);
    SEG_CHECK;

    export_block (b, block);

#ifdef _DEBUG
    memset (block._ptr, 0xcd, block._size);
#endif

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
///Realloc block if possible, that means when space immediatelly following the block is enough, fail otherwise
opcd segpage::realloc (
        CHUNK& block,           ///<structure specifying offset and page of the block, gets new block's size
        bool   keep_content
        )
{
    uints i, uns, unof, uolds;
    CHUNKI *pblocku, eblock, b;

    if( block._ptr == 0 )
        return alloc(block);

    import_block( block, b );
    RASSERTX( b._count < 0x10000, "too big segment page" );

    if( !test_offs(b._offs) )
        return ersINVALID_PARAMS;

    uints size = b._count;
    if( size > get_max_size() )
        return ersOUT_OF_RANGE "value";

    i = findu(b._offs);
    if(i == UMAX)  return ersOUT_OF_RANGE "value";

    pblocku = _pused;
    uolds = pblocku[i]._count;

    if( uolds >= size )
    {
        if( uolds >= 2*size )
        {
            if( 0 == reduce(i, size) )
            {
                export_block( pblocku[i], block );
                return 0;
            }
        }

        b._count = uolds;
        export_block( b, block );
        return 0;
    }

    uns = uolds;

    CHUNKF *fl, *fu;
    if( i>0  &&  pblocku[i-1]._offs + pblocku[i-1]._count < b._offs )
    {
        fl = ptr( (ushort)(pblocku[i-1]._offs + pblocku[i-1]._count) );
        uns += fl->_counta;
    }
    else if( i==0  &&  b._offs > get_struct_gran() )
    {
        fl = ptr( (ushort)(get_struct_gran()) );
        uns += fl->_counta;
    }
    else
        fl = 0;

    if( i<_nused-1  &&  pblocku[i]._offs + pblocku[i]._count < pblocku[i+1]._offs )
    {
        fu = ptr ((ushort)(pblocku[i]._offs + pblocku[i]._count));
        uns += fu->_counta;
    }
    else if( i == _nused -1  &&  pblocku[i]._offs + pblocku[i]._count < (_segsize>>rGRANULARITY) )
    {
        fu = ptr( (ushort)(pblocku[i]._offs + pblocku[i]._count) );
        uns += fu->_counta;
    }
    else
        fu = 0;

    // surrounding free blocks are not enough
    if( uns < b._count )
    {
        if(!keep_content)
            release(i);

        if( 0 != alloc(block) )
            return ersNOT_ENOUGH_MEM;

        if(keep_content)
        {
            xmemcpy( block._ptr, get_ptr(b._offs), uolds<<rGRANULARITY );
            i = findu(b._offs);
            release(i);
#ifdef _DEBUG
            memset( (char*)block._ptr + (uolds<<rGRANULARITY), 0xcd, block._size - (uolds<<rGRANULARITY) );
#endif
        }

        return 0;
    }

    RASSERT( fl ? fl->check_count() : 1 );
    RASSERT( fu ? fu->check_count() : 1 );

    if(fl)  unof = index(fl);  else  unof = pblocku[i]._offs;

    if(fl)  exclude(fl);
    if(fu)  exclude(fu);

    if(keep_content)
    {
        memmove( get_ptr(unof), block._ptr, uolds<<rGRANULARITY );
    }

    if( uns - size > MINCHUNKSIZE )  //include remainder in free chain
    {
        eblock._offs = unof + size;
        eblock._count = uns - size;

        sortf( eblock, 0, 0 );

        pblocku[i]._offs = unof;
        pblocku[i]._count = size;

        b._offs = unof;
        SEG_CHECK;
    }
    else
    {
        pblocku[i]._offs = b._offs = unof;
        pblocku[i]._count = b._count = uns;
        SEG_CHECK;
    }

    export_block(b, block);
    if(keep_content)
    {
#ifdef _DEBUG
        memset( (char*)block._ptr + (uolds<<rGRANULARITY), 0xcd, block._size - (uolds<<rGRANULARITY) );
#endif
    }
    else {
#ifdef _DEBUG
        memset( block._ptr, 0xcd, block._size );
#endif
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
///Free block
opcd segpage::free(
        const CHUNK& block     ///<structure with page and offset of the block, gets its size
        )
{
    uints uused;

    SEG_CHECK;

    CHUNKI b;
    import_block( block, b );

    RASSERTX( test_offs( b._offs ), "invalid block" );

    uused = findu(b._offs);
    if (uused == UMAX)  return ersINVALID_PARAMS;

    return release (uused);
}

////////////////////////////////////////////////////////////////////////////////
///Return block previous to the given block, within the used chain
opcd segpage::prev(CHUNKI& block) const
{
    uints j, uu;
    CHUNKI *pblocku;

    pblocku = _pused;
    uu = _nused;
    for (j=0; j<uu; ++j)
    {
        if (pblocku[j]._offs + pblocku[j]._count == block._offs) {
            block._offs = pblocku[j]._offs;
            block._count = pblocku[j]._count;
            return 0;
        }
    }

    return ersNOT_FOUND;
}

////////////////////////////////////////////////////////////////////////////////
///Find out if given block is valid one within the used blocks chain
opcd segpage::find(CHUNKI& block) const
{
    uints uused;

    uused = findu (block._offs);
    if (uused == UMAX)  return ersINVALID_PARAMS;

    block._offs = _pused[uused]._offs;
    block._count = _pused[uused]._count;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
///Release block from page
opcd segpage::release(
        uints uused         ///<number of the block in used blocks chain
        )
{
    CHUNKF *fl, *fu;
    uints uoffs;
    CHUNKI *pblocku;

    //exclude from used chain

    pblocku = _pused;
    uoffs = pblocku[uused]._offs;
    //*pusize = pblocku[uused]._size;

    if (uused>0  &&  pblocku[uused-1]._offs + pblocku[uused-1]._count < uoffs)
        fl = ptr ((ushort)(pblocku[uused-1]._offs + pblocku[uused-1]._count));
    else if (uused == 0  &&  uoffs > get_struct_gran ())
        fl = ptr ((ushort)get_struct_gran ());
    else
        fl = 0;

    if (uused < _nused-1  &&  pblocku[uused]._offs + pblocku[uused]._count < pblocku[uused+1]._offs)
        fu = ptr ((ushort)(pblocku[uused]._offs + pblocku[uused]._count));
    else if (uused == _nused -1  &&  pblocku[uused]._offs + pblocku[uused]._count < (_segsize>>rGRANULARITY))
        fu = ptr ((ushort)(pblocku[uused]._offs + pblocku[uused]._count));
    else
        fu = 0;

    sortf( pblocku[uused], fl, fu );

    --_nused;
    if (uused < _nused)
        memmove (pblocku +uused, pblocku +uused +1, (_nused-uused)*sizeof(CHUNKI));

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
///Find block with given offset in used blocks chain
uints segpage::findu(
        uints uoffs         ///<offset to find
        ) const
{
    uints j, ul, uu;
    CHUNKI *pblocku;

    pblocku = _pused;
    ul=0;  uu = _nused;
    for(; uu -ul > MINBLOCKSTOLINSEARCH;)
    {
        j = (uu+ul)>>1;
        if (pblocku[j]._offs > uoffs)  uu = j;
        else if (pblocku[j]._offs < uoffs)  ul = j+1;
        else  return j;
    }

    for (j=ul; j < uu; ++j)
    {
        if (pblocku[j]._offs == uoffs)  return j;
    }

    return UMAX;
}

////////////////////////////////////////////////////////////////////////////////
///Sort block in the used blocks chain by its offset
void segpage::sortu (
        CHUNKI& block        ///<block to sort
        )
{
    uints j, ul, uu;
    CHUNKI *pblocku;

    pblocku = _pused;

    ul=0;  uu = _nused;
    for(; uu -ul > MINBLOCKSTOLINSEARCH;)
    {
        j = (uu +ul) >> 1;
        if (pblocku[j]._offs > block._offs)  uu=j;
        else  ul = j+1;
    }

    for (j=ul; j < uu; ++j)
    {
        if (pblocku[j]._offs > block._offs)  break;
    }

    if (j < _nused)
        memmove (pblocku +j +1, pblocku +j, (_nused-j)*sizeof(CHUNKI));

    pblocku[j]._offs = block._offs;
    pblocku[j]._count = block._count;

    ++_nused;
}

////////////////////////////////////////////////////////////////////////////////
///Sort block in free blocks chain
void segpage::sortf (
        CHUNKI& block,       ///<block to sort
        CHUNKF* fl,            ///<offset to free block immediatelly under the sorted block
        CHUNKF* fu             ///<offset to free block immediatelly above the sorted block
        )
{
    RASSERT( fl ? fl->check_count() : 1 );
    RASSERT( fu ? fu->check_count() : 1 );

    if (fl)
    {
        fl->set_count (fl->_counta + (ushort)block._count);
        if (fu)
        {
            fl->set_count (fl->_counta + fu->_counta);
            exclude (fu);
        }

        sortf_up (fl);
    }
    else if (fu)
    {
        CHUNKF* p = ptr ((ushort)block._offs);
        p->set_count (fu->_counta + (ushort)block._count);
        replace (fu, p);

        sortf_up (p);    //always larger
    }
    else
    {
        CHUNKF* pfree = _wfreesm ? ptr (_wfreesm) : 0;
        for (; pfree  &&  pfree->_counta < block._count; pfree = nextf (pfree));

        CHUNKF* p = ptr ((ushort)block._offs);
        p->set_count ((ushort)block._count);

        if (pfree)
            insert (pfree, p);
        else
            append (p);

        RASSERT( p ? p->check_count() : 1 );
        RASSERT( pfree ? pfree->check_count() : 1 );
    }
}

////////////////////////////////////////////////////////////////////////////////
///Sort in free blocks chain by size, sorting in the lower part of array from given position
void segpage::sortf_down (
        CHUNKF* p
        )
{
    if (!p->_prev)  return;

    CHUNKF* pfree = ptr (_wfreesm);
    for (; pfree != p  &&  pfree->_counta < p->_counta; pfree = nextf (pfree));

    if (pfree != p)
        move (pfree, p);

    RASSERT( p ? p->check_count() : 1 );
    RASSERT( pfree ? pfree->check_count() : 1 );
}

////////////////////////////////////////////////////////////////////////////////
///Sort in free blocks chain by size, sorting in the upper part of array from given position
void segpage::sortf_up (
        CHUNKF* p
        )
{
    if (!p->_next)  return;

    CHUNKF* pfree = nextf(p);
    for (; pfree  &&  pfree->_counta < p->_counta; pfree = nextf (pfree));

    if (!pfree) {
        exclude (p);
        append (p);
    }
    else if (pfree != p)
        move (pfree, p);

    RASSERT( p ? p->check_count() : 1 );
    RASSERT( pfree ? pfree->check_count() : 1 );
}

////////////////////////////////////////////////////////////////////////////////
opcd segpage::reduce (uints uused, uints size)
{
    CHUNKI* pchunk = _pused + uused;
    CHUNKI fch;

    fch._offs = pchunk->_offs + size;
    fch._count = pchunk->_count - size;

    CHUNKF* fu;
    if (uused < _nused-1  &&  _pused[uused]._offs + _pused[uused]._count < _pused[uused+1]._offs)
        fu = ptr ((ushort)(_pused[uused]._offs + _pused[uused]._count));
    else if (uused == _nused -1  &&  _pused[uused]._offs + _pused[uused]._count < (_segsize>>rGRANULARITY))
        fu = ptr ((ushort)(_pused[uused]._offs + _pused[uused]._count));
    else
        fu = 0;

    sortf (fch, 0, fu);

    pchunk->_count = size;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
bool segpage::adjust (
    long add
    )
{
    //enlarging requires one free slot to hold new control block
    RASSERT( _nused+1 <= _rsused );

    if (add > 0)
    {
        if (_nused + add < _rsused)  return true;        //already ok

        CHUNK a;
        a._size = (_nused+FORTHALLOCBLOCKS)*sizeof(CHUNKI);
        if (0 == alloc (a, false))
        {
            CHUNKI* pused = _pused;
            xmemcpy (a._ptr, pused, _nused*sizeof(CHUNKI));
            _pused = (CHUNKI*) a._ptr;
            _rsused = a._size / sizeof(CHUNKI);
            a._ptr = pused;
            free (a);
            SEG_CHECK;
        }
        else  return false;
    }
    else if (add < 0)
    {
        if (_nused + add + 1 > _rsused/2)  return true;  //do only if it's worth

        uints uoff = findu ((CHUNKI*)_pused - (CHUNKI*)this);

        reduce (uoff, _nused + add + 1);
        SEG_CHECK;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
void segpage::check_state (uints used) const
{
    CHUNKF* p = _wfreesm ? ptr (_wfreesm) : 0;
    CHUNKF* pfree = _wfreesm ? nextf(p) : 0;

    for (; pfree  &&  pfree->_counta >= p->_counta; p=pfree, pfree=nextf(pfree));

    RASSERTXN( !pfree, "free blocks sort order broken" );
    RASSERTN(!pfree);

    uints size = 0;
    for (uints i=0; i<_nused; ++i)
    {
        size += _pused[i]._count;
    }

    size += used;

    if (_wfreesm) {
        CHUNKF* p = ptr (_wfreesm);
        for (; p ; p=nextf(p))
        {
            RASSERTX( p->check_count(), "the block header is damaged, possibly by an operation preceding this check" );
            size += p->_counta;
        }
    }

    RASSERTXN( size == (_segsize>>rGRANULARITY) - get_struct_gran(), "computed size of free blocks doesn't match the declared one" );
    RASSERTN( size == (_segsize>>rGRANULARITY) - get_struct_gran() );
}

} // namespace coid
