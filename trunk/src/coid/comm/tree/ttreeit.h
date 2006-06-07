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

#ifndef __TREE_ITERATOR_H__
#define __TREE_ITERATOR_H__

#include "../namespace.h"

#include "ttree.h"
#include "../segarray.h"
//#include "alib/node.h"
#include "ttreebm.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
template <class NC>
struct ttreeITEM
{
    COID        _coid;
    ulong       _flgs;
    charstr     _text;
    NC          _nc;
    COID        _par;
};

template <class NC>
inline binstream& operator << (binstream& out, const ttreeITEM<NC>& r) {
    return out << r._coid << r._flgs << r._text << r._nc << r._par;
}

template <class NC>
inline binstream& operator >> (binstream& in, ttreeITEM<NC>& r) {
    return in >> r._coid >> r._flgs >> r._text >> r._nc >> r._par;
}


////////////////////////////////////////////////////////////////////////////////
template <class NC>
struct ttreePANEL
{
    ulong                       _selected;
    ulong                       _count;
    dynarray<ttreeITEM<NC> >    _itms;

    enum VERSION {
        v1 = '1'
    };
};

template <class NC>
inline binstream& operator << (binstream& out, const ttreePANEL<NC>& r) {
    return out << (char) ttreePANEL<NC>::v1 << r._selected << r._count << r._itms;
}

template <class NC>
inline binstream& operator >> (binstream& in, ttreePANEL<NC>& r) {
    char version;
    in >> version;
    switch (version) {
    case ttreePANEL<NC>::v1:
        in >> r._selected >> r._count >> r._itms;
        break;
    }
    return in;
}


////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY = TTREE_NOMAP<T> >
class ttreeITERATOR
{
    struct ttreeNODES
    {
        ulong               _flags;         ///<node flags - expand, has children ...
        COID                _coid;          ///<ttree node id

        ttreeNODES () {
            _flags= 0;
            _coid.set_invalid();
        }
    };

    segarray<ttreeNODES>    _nodes;         ///<array of visible tree nodes
    typedef segarray<ttreeNODES>::ptr   ttreeNODES_PTR;

    typedef ttree<T, NCT, MAP_POLICY>            TTREE;
    typedef ttree<T, NCT, MAP_POLICY>::forest    FOREST;
    typedef ttree<T, NCT, MAP_POLICY>::ID        ID;

    typedef ttree_bitmap<T, NCT, MAP_POLICY>     BITMAP;


    TTREE*                  _tree;          ///<reference tree
    LNID                    _root;          ///<start id from tree

    ulong                   _selected;      ///<selected id
    ulong                   _pos;           ///<start id in list
    ulong                   _size;          ///<size od last returned list

//	typedef ttreePANEL< ClassRegister<NCT>::NodeClass >		ttreePANELint;
	typedef ClassRegister<NCT>::NodeClass		ttreePANELarg;

    ttreePANEL<ttreePANELarg>           _panel;         ///<list of shown items
    ttreePANEL<ttreePANELarg>           _panupd;        ///<list of unfolded items
    ulong                   _mc;            ///<count of moves
    BITMAP                  _bmp;           ///<selection bitmap
    BITMAP                  _accbmp;        ///<access bitmap

    LNID                    _to;            ///<restrict key to
    ulong                   _lfrom;         ///<restrict level from
    ulong                   _lto;           ///<restrict level to

    bool                    _forestit;      ///<forest iterator - use multiple trees

    /// 00=0 nothing vis. 01=1 not vis but child 10=2 vis but not all 11=3 all vis
    long is_visible (LNID key) //const
    {
        if (_forestit) return 3;        // multiple trees

        if( _accbmp.get_bmp_modify().size() ) {		/// what if size of valid bitmap is zero ?
			return (_accbmp.get_bit(key) << 1) | _accbmp.get_bit_sub(key);
        }

        ulong lev = _tree->get_level (key);
        if (lev < _lfrom)
            return 1;
        if (lev > _lto)
            return 0;
        if (_to == UMAX)
        {
            if (!_tree->belongs_under (key, _root))
                return 0;
            return _lto >= _tree->get_level_count()  ?  3  :  2;
        }
        if (_tree->compare (key, _root) < 0)
            return 1;
        if (_tree->compare (key, _to) > 0)
            return 0;
        return 2;
    }

    //opcd _unfold (LNID key, ulong pos, ulong __flags);
    opcd _unfold (COID key, ulong pos, ulong __flags);

public:
    enum {
        fEXPANDED           = 0x0002,       ///<node is expanded
        fSELECTED           = 0x0004,       ///<node is selected in list
        fEXPANDABLE         = 0x0010,       ///<node has children

        oEXPAND             = 0x01,
        oCOLLAPSE           = 0x02,
        oSWITCH             = oEXPAND | oCOLLAPSE,

        rLEVEL              = 16,           ///<unfold depth: bitpos in flags
        xLEVEL              = 0xffff0000,   ///<unfold depth: bitmask in flags
    };

    ttreeITERATOR (bool __forest= false);
    ~ttreeITERATOR();

    opcd init (ttree<T,NCT,MAP_POLICY>::forest* __forest, COID __root= COID(0,0), LNID to=UMAX, ulong lfrom=0, ulong lto=UMAX, bool __mt= false);

    opcd unfold (ulong __item, ulong __flags= oSWITCH);

    opcd load_list (ulong __position, ulong __size, const ttreePANEL<ttreePANELarg> ** p);
	opcd load_vector_list(ulong __position, ulong __size, dynarray<dynarray<COID> >& lst, uint* total);

    opcd get_expanded (dynarray<COID>& __exp) {
        ttreeNODES_PTR ptr;
        _nodes.get_ptr (0, ptr);
        for (ulong i= 0;; ++i) {
            if (ptr.is_valid()) {
                if (ptr->_flags & fEXPANDED) {
                    *(__exp.add (1))= ptr->_coid;
                }
            }
            else break;
            ++ptr;
        }

        return NOERR;
    }

    opcd refresh() {
        dynarray<COID> expanded;
        get_expanded (expanded);
        COID sel;
        get_selected (sel);

        _nodes.reset();
        unfold (UMAX);

        reload (expanded);
        select (sel);

        return NOERR;
    }

    opcd reload(dynarray<COID>& __exp) {
        for (ulong i= 0; i < __exp.size(); ++i) {
            ulong f= find (__exp[i]);
            if (f != UMAX) unfold (f, oEXPAND);
        }

        return NOERR;
    }

    ulong find (COID __id) {
        ttreeNODES_PTR ptr;
        _nodes.get_ptr (0, ptr);
        for (ulong i= 0;; ++i) {
            if (!ptr.is_valid()) break;
            if (ptr->_coid == __id) return i;
            ++ptr;
        }

        return UMAX;
    }

    opcd select (COID __id) {
        ID id;
        if (__id.is_invalid()) return ersSPECIFIC "couldn't select";
        if (!_tree->get_node(__id, id)) return ersSPECIFIC "invalid node id";

        uint count= 256;
        COID out[256];
        id.get_superiors (COID(0, 0), out, count, true);

        ulong item= UMAX;
        for (ulong i= 0; i < count -1; ++i) {
            item= find (out[i]);
            if (item != UMAX) unfold (item, oEXPAND);
        }
        item= find (__id);
        if (item != UMAX) select (item);

        return NOERR;
    }

    opcd select (ulong __position, ulong __fromlev, ulong __tolev)
    {
        if (__position >= _nodes.size())  return ersOUT_OF_RANGE;
        ID id;
		_tree->get_node (_nodes[__position]._coid, id);
        _bmp.set_liberating_rule (id, UMAX, __fromlev, __tolev);
        return 0;
    }

    opcd deselect (ulong __position, ulong __fromlev, ulong __tolev)
    {
        if (__position >= _nodes.size())  return ersOUT_OF_RANGE;
        ID id;
		_tree->get_node (_nodes[__position]._coid, id);
        _bmp.set_restricting_rule (id, UMAX, __fromlev, __tolev);
        return 0;
    }

    const BITMAP* get_bitmap () const       { return &_bmp; }
    void set_bitmap (const BITMAP* bmp)     { _bmp = *bmp; }
    const BITMAP* get_access_bitmap () const       { return &_accbmp; }
    void set_access_bitmap (const BITMAP* bmp)     {
		_accbmp = *bmp; _nodes.reset(); clear_selection( false );
		//unfold( UMAX, oEXPAND );
	}

    bool has_subordinated (ulong __position) {
        ttreeNODES_PTR ptr;
        _nodes.get_ptr(__position, ptr);
        if (ptr.is_valid()) return (ptr->_flags & fEXPANDABLE);
        return false;
    }

    ulong get_count () { return _nodes.size(); }

    opcd deselect () { _selected= UMAX; }

    opcd select (ulong __position) {
        if (__position >= _nodes.size())  return ersOUT_OF_RANGE;
        if (_selected != UMAX)
            _bmp.clr_bit (_nodes[_selected]._coid);
        _selected= __position;
        if (__position != UMAX)
            _bmp.set_bit (_nodes[__position]._coid);

        return NOERR;
    }

    opcd invert_selection ()
    {
        _bmp.invert_all ();
        return 0;
    }

    opcd clear_selection (bool toones)
    {
        _bmp.clear_all (toones);
        return 0;
    }

    COID get_item_coid (ulong __item) {
        COID coid;
        coid.set_invalid();
        ttreeNODES_PTR ptr;
        _nodes.get_ptr(__item, ptr);
        if (ptr.is_valid()) return ptr->_coid;

        return coid;
    }

    ulong get_selected () { return _selected; }

    opcd get_selected (COID& coid) {
        ttreeNODES_PTR ptr;
        _nodes.get_ptr(get_selected(), ptr);
        if (ptr.is_valid()) coid= ptr->_coid;
        else coid.set_invalid();

        return NOERR;
    }
};

///////////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
ttreeITERATOR<T,NCT,MAP_POLICY>::ttreeITERATOR (bool __forest /*= false */)
{
    _pos= 0;
    _size= 0;
    _selected= UMAX;

    _to = UMAX;
    _lfrom = 0;
    _lto = UMAX;

    _forestit= __forest;
}

///////////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
ttreeITERATOR<T,NCT,MAP_POLICY>::~ttreeITERATOR()
{
    _root.set_invalid ();
}

///////////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
opcd ttreeITERATOR<T,NCT,MAP_POLICY>::init(ttree<T,NCT,MAP_POLICY>::forest* __forest, COID __root,
    LNID to, ulong lfrom, ulong lto, bool __mt)
{
    if (!__forest) return ersINVALID_PARAMS "NULL pointer";

    if (__root.can_match_any_tree ())
        __root.set_tree_id (0);

    _tree = __forest->get_tree (__root.get_tree_id ());

	if( _tree == NULL )
		return ersINVALID_PARAMS "invalid root - tree id";

	if( ! _tree->check_key(__root) )
		return ersINVALID_PARAMS "invalid root - node";

    _root = __root;

    _forestit= __mt;

    _nodes.reset();
    clear_selection (false);

    return unfold (UMAX);
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
opcd ttreeITERATOR<T,NCT,MAP_POLICY>::unfold (ulong pos, ulong __flags/* = oSWITCH */)
{
    ttreeNODES_PTR  ptr;
    ulong flg;

    if (pos == UMAX)
        flg = fEXPANDABLE;
    else
    {
        _nodes.get_ptr (pos, ptr);
        if (!ptr.is_valid()) return ersINVALID_PARAMS "invalid node pointer";
        flg = ptr->_flags;
    }

    if (!(flg & fEXPANDABLE))
        return ersUNAVAILABLE "no children";

    if ((flg & fEXPANDED) && (__flags & oCOLLAPSE))
    { //collapse
        COID root = ptr->_coid;
        ptr->_flags &= ~fEXPANDED;
        ++ptr;

        ulong count = 0;
        while (ptr.is_valid()) {
            ID id;
            _tree->get_node(ptr->_coid, id);
            if (!id.has_superior(root)) break;
            ++count;
            if (_selected != UMAX && (pos+count == _selected))
                _selected= pos;
            ++ptr;
        }
        if (_selected!= UMAX && _selected > pos)
            _selected -= count;
        _nodes.del(pos+1, count);
    }
    else if (!(flg & fEXPANDED) && (__flags & oEXPAND))
    {
        if (pos == UMAX) {
            //_unfold (UMAX, 0, __flags);
            COID coid;
            coid.set_invalid();
            _unfold (coid, 0, __flags);
        }
        else
        {
            ptr->_flags |= fEXPANDED;
            _unfold (ptr->_coid, pos+1, __flags);
        }
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//template <class T, class MAP_POLICY>
//opcd ttreeITERATOR<T,MAP_POLICY>::_unfold (LNID key, ulong pos, ulong __flags/* = oSWITCH */) // LNID doesn't work with multiple trees
template <class T, class NCT, class MAP_POLICY>
opcd ttreeITERATOR<T,NCT,MAP_POLICY>::_unfold (COID key, ulong pos, ulong __flags/* = oSWITCH */)
{
    {  //expand
        ID id;

        dynarray<COID> out;
        //if (key == UMAX)
        if (key._id == UMAX)
                *out.add (1) = COID (_tree->get_tree_id(), _root);
        else
        {
            if (!_tree->get_node(key, id))
                return ersUNAVAILABLE;
            id->get_subordinated (out);
        }

        ulong item = pos;
        ttreeNODES_PTR  ptr;
        _nodes(pos, ptr);

        ulong npot = 0;

        {for (ulong i=0; i < out.size(); ++i)
        {
            long v = is_visible (out[i]);
            if (v == 1)
            {
                ++npot;
            }
            else if (v >= 2)
            {
                _tree->get_node (out[i], id);
                if (!id.is_invalid()) {
                    //ptr.ins (1);
					_nodes.ins( ptr.index(), ptr );
                    ptr->_flags = 0;//_bmp.get_bit (out[i], false) ? fSELECTED : 0;
                    ptr->_coid = out[i];
                    if (id->_asubnodes.size())
                        ptr->_flags |= fEXPANDABLE;
                    ++ptr;
                    ++item;
                    ++npot;
                }
            }
        }}

        ulong flgn = __flags;
        ulong dpth = __flags & xLEVEL;
        bool unf;
        if (dpth)
        {
            flgn -= 1 << rLEVEL;
            unf = true;
        }
        else
        {
            unf = false;
        }

        for (ulong i=out.size(); npot>0 && i>0;)
        {
            --i;

            long v = is_visible (out[i]);
            if (v >= 2  &&  !unf)
            {
                --item;
                continue;
            }
            else if (v == 0)
                continue;

            _unfold (out[i], item, flgn);
            --npot;
            if (v >= 2)
                --item;
        }
    }

    return NOERR;
}

///////////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
opcd ttreeITERATOR<T, NCT, MAP_POLICY>::load_list (ulong __position, ulong __size,  const ttreePANEL<ttreePANELarg> ** p)
{
    if (__position >= get_count()) return ersOUT_OF_RANGE;

    ulong count= (get_count() -__position) < __size ? get_count() -__position : __size;
    _panel._count= get_count();
    _panel._itms.discard();

    ttreeNODES_PTR ptr;
    _nodes.get_ptr (__position, ptr);
    if (ptr.is_valid() && count) {
        ttreeITEM<ttreePANELarg> * pitem= _panel._itms.need_new (count);
        for (ulong i= 0; i < count; ++i) {
            pitem->_coid= ptr->_coid;
            pitem->_flgs= ptr->_flags | (_bmp.get_bit (pitem->_coid) ? fSELECTED : 0);
            //if ((__position + i) == _selected) pitem->_flgs |= fSELECTED;
            ++pitem;
            ++ptr;
        }
        _pos= __position;
        _size= count;
    }
    else return ersSPECIFIC "invalid position";

    _panel._selected= _selected;
    *p= &_panel;

    return NOERR;
}

template <class T, class NCT, class MAP_POLICY>
opcd ttreeITERATOR<T, NCT, MAP_POLICY>::load_vector_list(ulong __position, ulong __size, dynarray<dynarray<COID> >& lst, uint* total)
{
    if (__position >= get_count()) return ersOUT_OF_RANGE;

    ulong count= (get_count() -__position) < __size ? get_count() -__position : __size;
    _panel._count= get_count();
    _panel._itms.discard();

    ttreeNODES_PTR ptr;
    _nodes.get_ptr (__position, ptr);
    if (ptr.is_valid() && count)
	{
		lst.need_new(count);
        for (ulong i= 0; i < count; ++i)
		{
            *lst[i].add(1) = ptr->_coid;
            ++ptr;
        }
        _pos= __position;
        _size= count;
    } else
		return ersSPECIFIC "invalid position";
    *total = _nodes.size();
    _panel._selected= _selected;
	return NOERR;
}

COID_NAMESPACE_END

#endif // __TREE_ITERATOR_H__
