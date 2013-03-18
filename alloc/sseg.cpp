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

#include "sseg.h"
//#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "memtrack.h"

#include "../comm.h"
#include "../commassert.h"
#include "../dynarray.h"

#include <algorithm>
#include <vector>

namespace coid {

//enable checking
#if 0
#define SEG_CHECK           check_state()
#define SEG_CHECK_(n)       check_state(n)
#else
#define SEG_CHECK
#define SEG_CHECK_(n)
#endif


#define IFGUARD     comm_mutex_guard<comm_mutex> __guard(1);  if(_mutex.is_set())  __guard.inject(*_mutex)
//#define CHECK_OVERLAP

////////////////////////////////////////////////////////////////////////////////
void* ssegpage::operator new ( size_t, uints segsize, bool )
{
    segsize = nextpow2(segsize);
    uchar r = ssegpage::block::get_granularity_shift_from_pagesize(segsize);

    MEMTRACK(ssegpage, segsize);

    return memaligned_alloc( segsize, 1<<r );
}

////////////////////////////////////////////////////////////////////////////////
void ssegpage::operator delete (void * ptr, uints segsize, bool )
{
    memaligned_free( ptr );
}

////////////////////////////////////////////////////////////////////////////////
ssegpage::ssegpage( bool mutex, uints segsize )
{
    _me = this;
    _used = 0;

    //segsize = nextpow2(segsize);
    _rsegsize = int_high_pow2( segsize );
    _ralign = ssegpage::block::get_granularity_shift_from_rpagesize((uchar)_rsegsize);

    if(mutex)
        initialize_mutex();

    reset();
}

////////////////////////////////////////////////////////////////////////////////
void ssegpage::reset()
{
    fblock* pf = (fblock*) get_first_ptr();
    pf->set( (1<<_rsegsize) - (uint)align_size(sizeof(*this)), this, pf );
    pf->set_base(0);
    pf->_bigger = 0;
    pf->_smaller = 0;

    _used = 0;
    _freesm = _freebg = pf;

    SEG_CHECK;

#ifdef _DEBUG
    _last_op = -1;
    _last_alloc = _last_realloc = _last_free = 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
opcd ssegpage::write_to_stream( binstream& bin )
{
    IFGUARD;
    uints len = uints(1)<<_rsegsize;
    return bin.write_raw( this, len );
}

////////////////////////////////////////////////////////////////////////////////
opcd ssegpage::read_from_stream( binstream& bin, void** pbase, int* diffaddr )
{
    IFGUARD;
    uint rsegsize = _rsegsize;
    comm_mutex* p = _mutex.eject();

    uints len = uints(1)<<_rsegsize;
    opcd e = bin.read_raw( this, len );

    new(&_mutex) local<comm_mutex>;   //clean initialization
    _mutex = p;

    if(e)  return e;
    if( _rsegsize != rsegsize )
        return ersMISMATCHED "the loaded and original segment sizes differ";

    //original base
    *pbase = _me;
    _me = this;

    //rebase the used block address
    ints diff = (char*)this - (char*)*pbase;
    *diffaddr = (int)diff;

    if( diff == 0  ||  _freesm == 0 )  return 0;

    _freesm = (fblock*) ((char*)_freesm + diff);
    _freebg = (fblock*) ((char*)_freebg + diff);

    fblock* bg = _freesm;
    for( ; bg; )
    {
        if( bg->_smaller )
            bg->_smaller = (fblock*) ((char*)bg->_smaller + diff);
        if( bg->_bigger )
            bg->_bigger = (fblock*) ((char*)bg->_bigger + diff);
        bg = bg->_bigger;
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
ssegpage::block* ssegpage::alloc_big( uints size )
{
    size = (uint)align_value_to_power2( size + sizeof(fblock), 12 );

    void* base = memaligned_alloc( size, 4096 );
    if(!base)  return 0;

    block* p = (block*) ( (char*)base + sizeof(int) );

    p->set_big( size, 12, base );
    return p;
}

////////////////////////////////////////////////////////////////////////////////
ssegpage::block* ssegpage::realloc_big( block* b, uints size, bool keep_content )
{
    uchar ra = b->get_ralign();

    size = align_value_to_power2( size + sizeof(fblock), 12 );
    if( ra >= 12 )
    {
        void* base = memaligned_alloc( size, 4096 );
        if(!base)  return 0;

        ::memcpy( base, b->get_base(), b->get_size() );
        memaligned_free(b->get_base());

        block* p = (block*) ( (char*)base + sizeof(int) );

        p->set_big( size, 12, base );
        return p;
    }
    else
    {
        ssegpage* sseg = b->get_base();
        uints os = b->get_usable_size();
        
        if( !keep_content )
            sseg->_free(b);

        void* base = memaligned_alloc( size, 4096 );
        if(!base)  return 0;

        block* p = (block*) ( (char*)base + sizeof(int) );

        if( keep_content )
        {
            xmemcpy( p+1, b+1, os );
            sseg->_free(b);
        }

        p->set_big( size, 12, base );
        return p;
    }
}

////////////////////////////////////////////////////////////////////////////////
///Alloc block of required size.
ssegpage::block* ssegpage::alloc (
        uints size,
        bool dbgfill
        )
{
    IFGUARD;

    SEG_CHECK;

    size = align_size( size + sizeof(fblock) );
    if( !_freebg  ||  size > _freebg->get_size() )
        return 0;

    //take the smallest satisfying block from the free chain
    fblock* pblockf = _freesm;
    for( ; pblockf  &&  pblockf->get_size() < size; pblockf = pblockf->_bigger );

    DASSERTX( pblockf, "internal ssegpage structures damaged" );
    DASSERT( !pblockf->is_base_set() );

    //exclude from the free chain
    uints os = pblockf->get_size();
    bool split = os - size >= uint(MINBLOCKSIZEGRAN << _ralign);
    
    if(split)
    {
        fblock* pnf = (fblock*) ( (char*)pblockf + size );
        pnf->set( os - size, this, pblockf );
        pnf->set_base(0);
        
        replace( pblockf, pnf ); 

        pblockf->set_size( size, this );
        pblockf->set_base( this );

        sortf_down(pnf);  //new size always smaller
    }
    else
    {
        exclude(pblockf);
        pblockf->set_base( this );
    }

#ifdef _DEBUG
    if(dbgfill) memset( (block*)pblockf+1, 0xcd, pblockf->get_usable_size() );
#endif

    DASSERT( pblockf->get_base() == this  &&  !pblockf->is_free_block() );

    SEG_CHECK;

    DASSERT( _used + pblockf->get_size() <= (uints(1)<<_rsegsize) - align_size(sizeof(*this)) );
    _used += pblockf->get_size();

#ifdef _DEBUG
    _last_op = 0;
    _last_alloc = pblockf;
#endif

    return pblockf;
}

////////////////////////////////////////////////////////////////////////////////
///Realloc block if possible, that means when space immediatelly following the block is enough, fail otherwise
ssegpage::block* ssegpage::realloc (
        block* bi,           ///<structure specifying offset and page of the block, gets new block's size
        uints size,
        bool keep_content
        )
{
    DASSERT(bi);

    if( bi->get_ralign() >= 12 )
        return realloc_big( bi, size, keep_content );
    else
	{
        DASSERT( bi->is_base_set() );

        ssegpage* p = bi->get_base();
        return p->_realloc( bi, size, keep_content );
	}
}

////////////////////////////////////////////////////////////////////////////////
ssegpage::block* ssegpage::_realloc (
        block* bi,           ///<structure specifying offset and page of the block, gets new block's size
        uints rqsize,
        bool keep_content
        )
{
	DASSERT( _me == this );

    //RASSERT( bi->check_tail() );
    DASSERT( test_offs(bi) );

    IFGUARD;

    uints size = align_size( rqsize + sizeof(fblock) );
    if( !_freebg  ||  size > _freebg->get_size() )
        return 0;

    SEG_CHECK;

    uints uoldbs = bi->get_size();
    if( uoldbs >= size )
    {
        if( uoldbs >= 2*size )
        {
            if( 0 == reduce(bi, size) )
                return bi;
        }

#ifdef _DEBUG
    _last_op = 0x00020001;
    _last_realloc = bi;
#endif

        return bi;
    }

    uints unbs = uoldbs;

    fblock *fl=0, *fu=0;

    uints sn, sp;
    sn = bi->get_size( &sp );

    if( sn  &&  (char*)bi + sn < (char*)this + (uints(1)<<_rsegsize) )
    {
        fu = (fblock*) ((char*)bi + sn);
        if( !fu->is_free_block() )
            fu = 0;
        else
            unbs += fu->get_size();
    }

    if( unbs < size  &&  sp > 0 )
    {
        fl = (fblock*) ((char*)bi - sp);
        if( !fl->is_free_block() )
            fl = 0;
        else
            unbs += fl->get_size();
    }

    // surrounding free blocks are not enough
    if( unbs < size )
    {
        if(!keep_content)
            _free(bi);

        block* pa = alloc( rqsize, false );
        if(!pa)
            return 0;

        if(keep_content)
        {
            uints os = uoldbs - sizeof(block) - sizeof(block::tail);
            xmemcpy( pa+1, bi+1, os );
            _free(bi);
#ifdef _DEBUG
            memset( (char*)(pa+1) + os, 0xcd, pa->get_size() - uoldbs );
#endif
        }

        SEG_CHECK;

#ifdef _DEBUG
    _last_op = 0x00020002;
    _last_realloc = pa;
#endif

        return pa;
    }

    //RASSERT( fl ? fl->check_size() : 1 );
    //RASSERT( fu ? fu->check_size() : 1 );

    _used -= sn;

    block* pn;
    if(fl)
        pn = (block*)fl;
    else 
        pn = bi;

    if(fl) exclude(fl);
    if(fu) exclude(fu);

    pn->set_size( unbs, this );
    pn->set_base(this);

    if( keep_content )
    {
        //unbs > uoldbs here
        if( pn != bi )
            memmove( pn+1, bi+1, uoldbs );
    }

    if( unbs - size >= uint(MINBLOCKSIZEGRAN<<_ralign) )  //include remainder in free chain?
    {
        pn->set_size( size, this );

        fblock* pnf = (fblock*) ( (char*)pn + size );
        pnf->set_size( unbs - size, this );
        pnf->set_base(0);

        sortf( pnf, unbs - size, 0, 0 );
    }

    SEG_CHECK;

    if(keep_content)
    {
#ifdef _DEBUG
        memset( (char*)pn + uoldbs - sizeof(uint), 0xcd, pn->get_size() - uoldbs );
#endif
    }
    else {
#ifdef _DEBUG
        memset( pn+1, 0xcd, pn->get_size() - sizeof(fblock) );
#endif
    }

    DASSERT( pn->is_base_set()  &&  pn->get_base() == this );

    SEG_CHECK;

    DASSERT( _used + pn->get_size() <= (uints(1)<<_rsegsize) - align_size(sizeof(*this)) );
    _used += pn->get_size();

#ifdef _DEBUG
    _last_op = 0x00020003;
    _last_realloc = pn;
#endif

    return pn;
}

////////////////////////////////////////////////////////////////////////////////
opcd ssegpage::free( block* b )
{
    if( b->get_ralign() >= 12 )
    {
        memaligned_free( b->get_base() );
        return 0;
    }
    else
	{
	    ssegpage* p = b->get_base();
        return p->_free(b);
	}
}

////////////////////////////////////////////////////////////////////////////////
///Free block
opcd ssegpage::_free(
        block* bi     ///<structure with page and offset of the block, gets its size
        )
{
	DASSERT( _me == this );

    IFGUARD;
    SEG_CHECK;

    RASSERTX( test_offs(bi), "invalid block" );

#ifdef _DEBUG
    _last_op = 0x00010000;
    _last_free = bi;
#endif

    //exclude from used chain
    fblock *fl=0, *fu=0;

    uints sn, sp;
    sn = bi->get_size( &sp );
    _used -= sn;

    if( sn  &&  (char*)bi + sn < (char*)this + (uints(1)<<_rsegsize) )
    {
        fu = (fblock*) ((char*)bi + sn);
        if( !fu->is_free_block() )
            fu = 0;
    }

    if( sp )
    {
        fl = (fblock*) ((char*)bi - sp);
        if( !fl->is_free_block() )
            fl = 0;
    }

    sortf( bi, bi->get_size(), fl, fu );

    SEG_CHECK;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
///Sort block in the free blocks chain
void ssegpage::sortf (
        void* b,                ///<block to sort
        uints size,
        fblock* fl,             ///<offset to free block immediatelly under the sorted block
        fblock* fu              ///<offset to free block immediatelly above the sorted block
        )
{
    //DASSERT( fl ? ( fl->check_size()  &&  (char*)fl + fl->_sizea == (char*)b )  : 1 );
    //DASSERT( fu ? ( fu->check_size()  &&  (char*)b + size == (char*)fu ) : 1 );

    if(fl)
    {
        if(fu)
        {
            exclude(fu);
            fl->set_size( fl->get_size() + size + fu->get_size(), this );
        }
        else
            fl->set_size( fl->get_size() + size, this );

        sortf_up(fl);
    }
    else if(fu)
    {
        fblock* p = (fblock*)b;
        p->set_size( fu->get_size() + size, this );
        p->set_base(0);
        replace( fu, p );

        sortf_up(p);    //always larger
    }
    else
    {
        fblock* pfree = _freesm ? _freesm : 0;
        for( ; pfree  &&  pfree->get_size() < size; pfree = pfree->_bigger );

        fblock* p = (fblock*)b;
        p->set_base(0);

        insert( pfree, p );

        //DASSERT( p ? p->check_size() : 1 );
        //DASSERT( pfree ? pfree->check_size() : 1 );
    }
}

////////////////////////////////////////////////////////////////////////////////
///Sort in free blocks chain by size, sorting in the lower part of array from given position
void ssegpage::sortf_down (
        fblock* p
        )
{
    if(!p->_smaller)  return;

    fblock* px = p->_smaller;
    for( ; px  &&  px->get_size() >= p->get_size(); px = px->_smaller );

    if(px)
    {
        if( px->_bigger == p )  return;
        move( px->_bigger, p );
    }
    else
        move( _freesm, p );
/*
    fblock* pfree = _freesm;
    for( ; pfree != p  &&  pfree->_sizea < p->_sizea; pfree = pfree->_bigger );

    if( pfree != p )
        move( pfree, p );
*/
//    DASSERT( p ? p->check_size() : 1 );
//    DASSERT( pfree ? pfree->check_size() : 1 );
}

////////////////////////////////////////////////////////////////////////////////
///Sort in free blocks chain by size, sorting in the upper part of array from given position
void ssegpage::sortf_up (
        fblock* p
        )
{
    if(!p->_bigger)  return;

    fblock* pfree = p->_bigger;
    for( ; pfree  &&  pfree->get_size() < p->get_size(); pfree = pfree->_bigger );

    if(!pfree) {
        exclude(p);
        append(p);
    }
    else if( pfree != p )
        move( pfree, p );

//    RASSERT( p ? p->check_size() : 1 );
//    RASSERT( pfree ? pfree->check_size() : 1 );
}

////////////////////////////////////////////////////////////////////////////////
opcd ssegpage::reduce( block* bi, uints size )
{
    fblock* fu=0;
    uints sn = bi->get_size();

    _used -= sn;

    fu = (fblock*) ((char*)bi + sn);
    if( (char*)fu < (char*)this + (uints(1)<<_rsegsize)  &&  !fu->is_free_block() )
        fu = 0;

    fblock* pf = (fblock*) ((char*)bi + size);
    bi->set_base(0);

    if( fu )
    {
        pf->set( bi->get_size() - size + fu->get_size(), this, bi );
        replace( pf, fu );

        sortf_up(pf);
    }
    else
    {
        pf->set( bi->get_size() - size, this, bi );
        sortf( pf, pf->get_size(), 0, fu );
    }

    bi->set_size( size, this );
    _used += size;

    SEG_CHECK;

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
void ssegpage::check_state( uints used ) const
{
	DASSERT( _me == this );
    IFGUARD;

    fblock* p = _freesm;
    fblock* pfree = p ? p->_bigger : 0;

    for( ; pfree  &&  pfree->get_size() >= p->get_size(); p=pfree, pfree=pfree->_bigger )
        RASSERTX( !pfree->is_base_set(), "not a free block in the free blocks chain" );

    RASSERTX( !_freesm  ||  !pfree, "the free blocks sort order broken" );
    RASSERT( !_freesm  ||  p == _freebg );

    bool isfreesm=0, isfreebg=0;

    block* bo = get_first_ptr();
    block* bi;
    for( ; !is_last(bo) ; )
    {
        bi = bo->get_next();

        RASSERT( (char*)bi == (char*)bo + bo->get_size() );

        if( bi->is_base_set() )
        {
            //used block should have correct base set
            RASSERT( this == bi->get_base() );
        }
        else
        {
            //two free blocks cannot be together
            RASSERT( bo->is_base_set() );
        }

        if( bo == _freesm )  isfreesm = true;
        if( bo == _freebg )  isfreebg = true;

        bo = bi;
    }

    RASSERT( (char*)bo + bo->get_size() == (char*)this + (uints(1)<<_rsegsize) );

    RASSERT( isfreesm  ||  _freesm == 0  ||  bo == _freesm );
    RASSERT( isfreebg  ||  _freebg == 0  ||  bo == _freebg );
    RASSERT( 0 == ((_freebg == 0)  ^  (_freesm == 0)) );
}

} // namespace coid
