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

#ifndef __COID_COMM_SEGS__HEADER_FILE__
#define __COID_COMM_SEGS__HEADER_FILE__

#include "../namespace.h"

#include "../binstream/binstream.h"
#include "../sync/mutex.h"
#include "../local.h"
#include "../retcodes.h"
#include "../commassert.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Memory allocator for blocks of varying sizes.
/// Memory overhead is one uint per used block. Free blocks form double-linked list
/// sorted by size.
class ssegpage
{
public:

    struct block;
    friend struct block;

    ///
    struct block
    {
        enum {
            fBIG_CHUNK          = 0x80000000,   //< a big block, containing just the size shifted
            xBIG_SIZE_SHIFTED   = ~fBIG_CHUNK,
            MAXPAGESHIFT        = 12
        };

        /**
            Contains shift to base and size of the block.
            Kind of 'floating point' format for holding position and size of 
            wide range of block sizes (with adjusted granularities).
             1 bit 0, 5 bits shift, 13b offset and 13b size
            shift=0   =>    offset=13b<<3   size=13b<<3
            shift=1   =>    offset=13b<<4   size=13b<<4
            ...
            shift=9   =>    offset=13b<<12  size=13b<<12    (4k -> malloc)
        **/
        uint _shftsize;

        ///
        struct tail
        {
            uint _offbase;
        };

        ssegpage* get_base() const  { return (ssegpage*) ((char*)this - ((tail*)this)[-1]._offbase); }
        bool is_base_set() const    { return ((tail*)this)[-1]._offbase != 0; }

        bool is_big_chunk() const   { return (_shftsize & fBIG_CHUNK) != 0; }

        bool operator == (const block b) const      { return _shftsize == b._shftsize; }
        //bool operator > (const block b) const       { return (_shftsize & 0x03ffe000) > (b._shftsize & 0x03ffe000); }
        //bool operator < (const block b) const       { return (_shftsize & 0x03ffe000) < (b._shftsize & 0x03ffe000); }


        uchar get_ralign() const    { return 3 + (_shftsize >> (32-6)); }

        uints get_size() const
        {
            if( _shftsize & fBIG_CHUNK )
                return uints(_shftsize & xBIG_SIZE_SHIFTED) << MAXPAGESHIFT;

            uchar s = _shftsize >> (32-6);
            return (_shftsize << (6+13)) >> (6+13-3-s);
        }

        uints get_size( uints* prev ) const
        {
            DASSERT( !is_big_chunk() );
            uchar s = _shftsize >> (32-6);
            *prev = ((_shftsize<<6) & 0xfff80000) >> (13+6-3-s);
            return (_shftsize << (6+13)) >> (6+13-3-s);
        }



        uints get_usable_size() const   { return get_size() - sizeof(fblock); }

        block* get_next() const         { return (block*) ((char*)this + get_size()); }
        void get_next_prev( block*& prev, block*& next ) const
        {
            DASSERT( !is_big_chunk() );
            uchar s = _shftsize >> (32-6);
            prev = (block*) ((char*)this - (((_shftsize<<6) & 0xfff80000) >> (13+6-3-s)));
            next = (block*) ((char*)this + ((_shftsize << (6+13)) >> (6+13-3-s)));
        }

        uint get_granularity() const
        {
            DASSERT( !is_big_chunk() );
            uchar s = _shftsize >> (32-6);
            return 1<<(3+s);
        }

        uchar get_granularity_shift() const
        {
            DASSERT( !is_big_chunk() );
            uchar s = _shftsize >> (32-6);
            return 3+s;
        }

        static uchar get_granularity_shift_from_pagesize( uints pgs )
        {
            pgs = nextpow2(pgs);

            uchar i;
            for( i=0; pgs; ++i,pgs>>=1 );

            return get_granularity_shift_from_rpagesize(i-1);
        }

        static uchar get_granularity_shift_from_rpagesize( uchar rpgs )
        {
            if( rpgs <= 16 )  return 3;
            return 3 + rpgs-16;
        }

        void set_size( uints size, ssegpage* base )
        {
            uchar rgran = (uchar)base->_ralign;
            DASSERT( (size & ((1<<rgran)-1)) == 0 );
            _shftsize &= ~0xfc001fff;
            _shftsize |= (rgran-3) << (32-6);
            _shftsize |= uint(size>>rgran);

            if( (char*)this + size < (char*)base + (uints(1)<<base->_rsegsize) )
            {
                block* nx = (block*) ((char*)this + size);
                nx->_shftsize &= ~0x03ffe000;
                nx->_shftsize |= uint(size>>rgran) << 13;
            }
        }

        void set( uints size, ssegpage* base, block* prev )
        {
            uchar rgran = (uchar)base->_ralign;
            DASSERT( (size & ((1<<rgran)-1)) == 0 );
            int poff = int((char*)this - (char*)prev);
            _shftsize = (rgran-3) << (32-6);
            _shftsize += poff << (13-rgran);
            _shftsize += uint(size>>rgran);

            if( !base->is_last(this) )
            {
                block* nx = (block*) ((char*)this + size);
                nx->_shftsize &= ~0x03ffe000;
                nx->_shftsize |= uint(size>>rgran) << 13;
            }
        }

        void set( uints size, ssegpage* base, block prev )
        {
            uchar rgran = (uchar)base->_ralign;
            DASSERT( (size & ((1<<rgran)-1)) == 0 );
            _shftsize = (rgran-3) << (32-6);
            _shftsize += prev._shftsize & 0x03ffe000;
            _shftsize += uint(size>>rgran);

            if( !base->is_last(this) )
            {
                block* nx = (block*) ((char*)this + size);
                nx->_shftsize &= ~0x03ffe000;
                nx->_shftsize |= uint(size>>rgran) << 13;
            }
        }

        void set_big( uints size, uchar rgran, void* base )
        {
            _shftsize = fBIG_CHUNK | uint(size>>MAXPAGESHIFT);

            ((tail*)this)[-1]._offbase = uint((char*)this - (char*)base);
        }

        void set_base( ssegpage* base )
        {
            if(!base)
                ((tail*)this)[-1]._offbase = 0;
            else
                ((tail*)this)[-1]._offbase = uint((char*)this - (char*)base);
        }

    };

protected:

    enum {
        INITALRESERVEDUSEDN     = 32,

        MINBLOCKSIZEGRAN        = 4,

        MINBLOCKSTOLINSEARCH    = 8,
        FORTHALLOCBLOCKS        = 8,
    };


    struct fblock : block
    {
        fblock* _bigger;
        fblock* _smaller;

        ushort is_free_block() const    { return !is_base_set(); }
    };

    ssegpage* _me;
    uints _used;

    fblock* _freebg;
    fblock* _freesm;

    ushort  _rsegsize;
    ushort  _ralign;
    local<comm_mutex> _mutex;


#ifdef _DEBUG
    void* _last_alloc;
    void* _last_free;
    void* _last_realloc;
    int   _last_op;
#endif


    //this must be last
    block::tail _dummy;


    bool is_behind_page( block* b ) const           { return (char*)b >= (char*)this + (uints(1)<<_rsegsize); }
    bool is_last( block* b ) const                  { return (char*)b + b->get_size() >= (char*)this + (uints(1)<<_rsegsize); }

/*
    block* get_block( const block& ch ) const       { return (block*) ((char*)this + ch.get_offs()); }
    block* get_block_after( const block& ch ) const { return (block*) ((char*)this + ch.get_offs_after()); }

    fblock* get_fblock( ushort w ) const            { return (fblock*) ((char*)this + ((uint)w<<_ralign)); }
    fblock* get_fblock_prev( const fblock& c) const { return (fblock*) ((char*)this + ((uint)(c._smaller&0x7fff)<<_ralign)); }

    void set_fblock_prev( fblock* f, void* p ) const{ f->_smaller = ((char*)p - (char*)this) >> _ralign; }
*/



    block* get_first_ptr() const        { return (block*) ((char*)this + align_size(sizeof(*this))); }

    //uint get_roffs_first() const        { return align_size(sizeof(*this)) << (13-_ralign); }
    //uint get_roffs_last() const         { return 1<<(13+_rsegsize-_ralign); }

public:

    void initialize_mutex()
    {
        if( !_mutex.is_set() ) {
            _mutex = new comm_mutex(100, false);
            _mutex->set_name( "ssegpage" );
		}
    }

    opcd write_to_stream( binstream& bin );
    opcd read_from_stream( binstream& bin, void** pbase, int* diffaddr );

    uints get_segsize() const               { return uints(1)<<_rsegsize; }
    uints get_used_size() const             { return _used; }

    void* operator new (size_t size, uints segsize, bool );
    void operator delete (void * ptr, uints segsize, bool );


    static ssegpage* create( bool mutex, uints segsize = 65536 )
    {
        return new(segsize, true) ssegpage(mutex, segsize);
    }

    static ssegpage* create( void* addr, bool mutex, uints segsize )
    {
        return ::new(addr) ssegpage(mutex, segsize);
    }

    

    uints usable_size() const
    {
        return (uints(1)<<_rsegsize) - align_size(sizeof(ssegpage))
            - align_size(INITALRESERVEDUSEDN*sizeof(block*) + sizeof(block));
    }
    
    static uints usable_size_r( uint rsegsize )
    {
        uchar r = ssegpage::block::get_granularity_shift_from_rpagesize(rsegsize);
        return (uints(1)<<rsegsize)
            - align_value_to_power2(sizeof(ssegpage),r)
            - align_value_to_power2(INITALRESERVEDUSEDN*sizeof(block*) + sizeof(block),r);
    }

    //opcd get_remaining_block_size(CHUNK& block) const;

    block* alloc( uints s )                 { return alloc( s, true); }
    static block* realloc( block* b, uints size, bool keep_content = true );

    static block* alloc_big( uints s );
    static block* realloc_big( block* b, uints size, bool keep_content = true );

    static opcd free( block* b );

    void reset();

    bool is_empty() const                   { return _used == 0;  }

    bool can_be_valid( const void* p ) const {
        return  p > (void*)this
            &&  p < (void*)( (char*)this + (uints(1)<<_rsegsize) );
    }

    ////////////////////////////////////////////////////////////////////////////////
    struct SEGLOCK
    {
        SEGLOCK(ssegpage& seg) : _seg(&seg) { seg._mutex->lock(); }
        ~SEGLOCK()                          { unlock(); }


        SEGLOCK( ssegpage& seg, bool trylock, uints* size ) : _seg(&seg)
        {
            if(trylock)
            {
                if( !seg._mutex->try_lock() )
                    _seg = 0;
                else
                {
                    if( seg._freebg == 0 )
                        *size = 0;
                    else
                        *size = seg._freebg->get_usable_size();
                }
            }
            else
            {
                seg._mutex->lock();
                *size = seg._freebg ? seg._freebg->get_usable_size() : 0;
            }
        }

        bool locked() const                 { return _seg != 0; }

        void unlock()
        {
            if(_seg) {
                _seg->_mutex->unlock ();
                _seg = 0;
            }
        }

        ssegpage* get() const               { return _seg; }

    private:
        ssegpage* _seg;
    };
    ////////////////////////////////////////////////////////////////////////////////
    

    

    void check_state( uints used = 0 ) const;

private:
    ssegpage( bool mutex, uints segsize );

    friend struct SEGLOCK;
protected:


    opcd _free( block* b );
    block* _realloc( block* b, uints size, bool keep_content = true );
    
    block* alloc( uints size, bool adj );

    bool test_offs( block* b ) const        { return b >= get_first_ptr()  &&  (char*)b < (char*)this + (uints(1)<<_rsegsize); }

    //block* blockptr( const void* p ) const  { return (block*)p; }

    void replace( const fblock* p, fblock* pn )
    {
        if (p->_bigger)  p->_bigger->_smaller = pn;
        else  _freebg = pn;
        if (p->_smaller)  p->_smaller->_bigger = pn;
        else  _freesm = pn;

        pn->_bigger = p->_bigger;
        pn->_smaller = p->_smaller;
    }

    void exclude( const fblock* p )
    {
        if (p->_bigger)  p->_bigger->_smaller = p->_smaller;
        else  _freebg = p->_smaller;
        if (p->_smaller)  p->_smaller->_bigger = p->_bigger;
        else  _freesm = p->_bigger;
    }

    void insert( fblock* old, fblock* p )
    {
        if(!old)
        {
            append(p);
            return;
        }

        if (old->_smaller)  old->_smaller->_bigger = p;
        else  _freesm = p;

        p->_smaller = old->_smaller;
        p->_bigger = old;
        old->_smaller = p;
    }

    void append( fblock* p )
    {
        if (_freebg) {
            _freebg->_bigger = p;
            p->_smaller = _freebg;
            p->_bigger = 0;
            _freebg = p;
        }
        else {
            _freebg = _freesm = p;
            p->_bigger = p->_smaller = 0;
        }
    }

    void move( fblock* dest, fblock* src )
    {
        exclude(src);
        insert(dest, src);
    }

    opcd reduce( block* bi, uints size );

    void sortf( void* b, uints size, fblock* fl, fblock* fu);
    void sortf_down(fblock* p);
    void sortf_up(fblock* p);

    uints align_size(uints s) const         { return align_value_to_power2(s,(uchar)_ralign); }
};

COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_SEGS__HEADER_FILE__

