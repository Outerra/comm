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

#ifndef __COID_COMM_SEG__HEADER_FILE__
#define __COID_COMM_SEG__HEADER_FILE__

#include "../namespace.h"

#include "../binstream/binstream.h"
#include "../sync/mutex.h"
#include "../retcodes.h"
#include "../assert.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
class segpage
{
public:
    enum {
        //rSEGSIZE                = 16,   ///< rSEGSIZE must be <= 16 in this implementation
        //SEGSIZE                 = 1 << rSEGSIZE,

        rGRANULARITY            = 3,
        GRANULARITY             = 1 << rGRANULARITY,

        //SEGCHUNKS               = SEGSIZE >> rGRANULARITY,

        INITIALRESERVEDUSED      = 32,

        MINCHUNKSIZE            = 4,    //in granularity units
        MINBLOCKSTOLINSEARCH    = 8,
        FORTHALLOCBLOCKS        = 8,
    };

    struct CHUNK {
        void*      _ptr;           ///< ptr to block
        uints      _size;          ///< size of the block in bytes
    };

protected:

    struct CHUNKI {
        uint       _offs;          ///< offset of the block in chunks
        uint       _count;         ///< size of the block in chunks
    };

    struct CHUNKF {
        ushort      _counta;
        ushort      _countb;
        ushort      _prev;
        ushort      _next;

        void set_count (ushort count)       { _counta = count; _countb = ~count; }
        bool check_count () const           { return _counta == (ushort)~(uint)_countb; }
    };

    CHUNKI* _pused;
    uint    _segsize;
    ushort  _wfreebg;
    ushort  _wfreesm;
    uint    _nused;
    uint    _rsused;
    mutable comm_mutex _mutex;

    static uints get_struct_gran()           { return align_to_chunks( sizeof(segpage), GRANULARITY ); }

public:

    opcd write_to_stream( binstream& bin )
    {
        return bin.write_raw( this, _segsize );
    }

    opcd read_from_stream( binstream& bin, void** pbase, int* diffaddr )
    {
        uint segsize = _segsize;
        uchar buf[sizeof(comm_mutex)];
        xmemcpy( buf, &_mutex, sizeof(comm_mutex) );

        opcd e = bin.read_raw( this, _segsize );
        xmemcpy( &_mutex, buf, sizeof(comm_mutex) );

        if(e)  return e;
        if( _segsize != segsize )
            return ersMISMATCHED "the loaded and original segment sizes differ";

        //_pused now contains pointer whose value lies within original segment
        // address mapping
        //lets get the original base address out of it
        *pbase = (void*) ( (uint)_pused & ~(_segsize-1) );

        //rebase the used block address
        *diffaddr = (char*)this - (char*)*pbase;
        _pused = (CHUNKI*) ( (char*)_pused + *diffaddr );

        return 0;
    }

    uint get_segsize() const                { return _segsize; }

    static segpage* create( uint segsize = 65536 )
    {
        return new(segsize) segpage(segsize);
    }

    void* operator new (size_t size, uint segsize );
    void operator delete (void * ptr, uint segsize );

    uint usable_size() const                { return ((_segsize>>rGRANULARITY) - get_struct_gran() - INITIALRESERVEDUSED)<<rGRANULARITY; }
    static uint usable_size( uint segsize)  { return ((segsize>>rGRANULARITY) - get_struct_gran() - INITIALRESERVEDUSED)<<rGRANULARITY; }

    opcd get_remaining_block_size (CHUNK& block) const;

    opcd alloc (CHUNK& block)               { return alloc (block, true); }
    opcd realloc (CHUNK& block, bool keep_content = true);

    opcd free (const CHUNK& block);

    bool can_be_valid( void* p ) const      { return  p > (void*)this  &&  p < (void*)( (char*)this + _segsize ); }

    struct SEGLOCK
    {
        SEGLOCK(segpage& seg) : _seg(&seg)  { seg._mutex.lock (); }
        ~SEGLOCK()                          { unlock (); }


        SEGLOCK(segpage& seg, bool trylock, uints* size) : _seg(&seg)
        {
            if (trylock)
            {
                if (!seg._mutex.try_lock ())
                    _seg = 0;
                else
                    *size = seg._wfreebg ? (seg.ptr(seg._wfreebg)->_counta<<rGRANULARITY) : 0;
            }
            else
            {
                seg._mutex.lock ();
                *size = seg._wfreebg ? (seg.ptr(seg._wfreebg)->_counta<<rGRANULARITY) : 0;
            }
        }

        bool locked () const            { return _seg != 0; }

        void unlock ()
        {
            if (_seg) {
                _seg->_mutex.unlock ();
                _seg = 0;
            }
        }

        segpage* get () const           { return _seg; }

    private:
        segpage* _seg;
    };

    void check_state (uints used = 0) const;

private:
    segpage( uints segsize );

    friend struct SEGLOCK;
protected:
    uints get_max_size () const              { return _wfreebg ? ptr(_wfreebg)->_counta : 0; }

    opcd alloc (CHUNK& block, bool adj);
    void import_block (const CHUNK& in, CHUNKI& out) const  {
        out._offs = (CHUNKI*)in._ptr - (CHUNKI*)this;
        out._count = (in._size + GRANULARITY - 1) >> rGRANULARITY;
    }

    void export_block (const CHUNKI& in, CHUNK& out) const  {
        out._ptr = (CHUNKI*)this + in._offs;
        out._size = in._count << rGRANULARITY;
    }

    void* get_ptr (uints offs) const         { return (char*)this + (offs<<rGRANULARITY); }

    bool test_offs (uints offs) const        { return offs >= get_struct_gran()  &&  offs < (_segsize>>rGRANULARITY); }

    opcd prev (CHUNKI& block) const;
    opcd find (CHUNKI& block) const;

    CHUNKF* nextf (const CHUNKF* p) const   { return p->_next ? ((CHUNKF*)((char*)this + (p->_next<<rGRANULARITY))) : 0; }
    CHUNKF* prevf (const CHUNKF* p) const   { return p->_prev ? ((CHUNKF*)((char*)this + (p->_prev<<rGRANULARITY))) : 0; }
    ushort index (const CHUNKF* p) const    { return ((char*)p - (char*)this) >> rGRANULARITY; }
    CHUNKF* ptr (ushort w) const            { return (CHUNKF*)((char*)this + (w<<rGRANULARITY)); }

    void replace (const CHUNKF* p, CHUNKF* pn)
    {
        if (p->_next)  nextf(p)->_prev = index (pn);
        else  _wfreebg = index (pn);
        if (p->_prev)  prevf(p)->_next = index (pn);
        else  _wfreesm = index (pn);

        pn->_next = p->_next;
        pn->_prev = p->_prev;
    }

    void exclude (const CHUNKF* p)
    {
        if (p->_next)  nextf(p)->_prev = p->_prev;
        else  _wfreebg = p->_prev;
        if (p->_prev)  prevf(p)->_next = p->_next;
        else  _wfreesm = p->_next;
    }

    void insert (CHUNKF* old, CHUNKF* p)
    {
        if (old->_prev)  prevf(old)->_next = index (p);
        else  _wfreesm = index (p);

        p->_prev = old->_prev;
        p->_next = index (old);
        old->_prev = index (p);
    }

    void append (CHUNKF* p)
    {
        if (_wfreebg) {
            ptr(_wfreebg)->_next = index(p);
            p->_prev = _wfreebg;
            p->_next = 0;
            _wfreebg = index (p);
        }
        else {
            _wfreebg = _wfreesm = index(p);
            p->_next = p->_prev = 0;
        }
    }

    void move (CHUNKF* dest, CHUNKF* src)
    {
        exclude (src);
        insert (dest, src);
    }

    opcd release (uints uused);
    opcd reduce (uints uused, uints size);

    uints findu (uints uoffs) const;

    void sortu (CHUNKI& block);
    void sortf (CHUNKI& block, CHUNKF* fl, CHUNKF* fu);

    void sortf_down (CHUNKF* p);
    void sortf_up (CHUNKF* p);

    bool adjust (long add);

    static ushort align_size (uints s)       { return (ushort) ((s + GRANULARITY - 1) >> rGRANULARITY); }
};

COID_NAMESPACE_END

#endif //#ifndef __COID_COMM_SEG__HEADER_FILE__

