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

#ifndef __COMM_TREE_BITMAP_H__
#define __COMM_TREE_BITMAP_H__

#include "../namespace.h"

#include "ttree.h"
#include "../binstream/binstream.h"

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
struct dim_access
{
    COID    root;
    uint    level_from;
    uint    level_to;

    friend binstream& operator << (binstream& out, const dim_access& d)
    {
        return out << d.root << d.level_from << d.level_to;
    }

    friend binstream& operator >> (binstream& in, dim_access& d)
    {
        return in >> d.root >> d.level_from >> d.level_to;
    }
};

/*
////////////////////////////////////////////////////////////////////////////////
struct KEY_LEVEL {
	ushort lev;
	ushort flags;

	KEY_LEVEL() {}
	KEY_LEVEL(ushort l, ushort f) : lev(l), flags(f) {}

	KEY_LEVEL & operator = (uint level) {*(uint *) this = level; return *this;}
	operator uint() {return *(uint *) this;}

	enum {flgALL=1, flgLOWEST=2, flgRELATIVE=4,};

	bool is_full() {return (flags & flgALL) != 0;}
	bool is_relative() {return (flags & flgRELATIVE) != 0;}
	bool is_lowest() {return (flags & flgLOWEST) != 0;}
};
*/


////////////////////////////////////////////////////////////////////////////////
struct U32PAIR
{
    uint _acc, _sub;

    void clear ()                           { _acc = _sub = 0; }
    void init ()                            { _acc = _sub = UMAX; }
    void set (uchar shf, const U32PAIR& v)
    {
        _acc &= ~(((v._acc+1)&1)<<shf);
        _sub &= ~(((v._sub+1)&1)<<shf);
        //if (!v._acc)  _acc &= ~(1<<shf);
        //if (!v._sub)  _sub &= ~(1<<shf);
    }

    void merge (const U32PAIR& p)
    {
        // minimize merging of two acc,sub pairs:
        //      00  01  10  11
        //      --------------
        // 00 | 00  01  10  11              00 - no node in subtree is accessible
        // 01 | 01  01  10  11              01 - root inaccessible, some subnodes accessible
        // 10 | 10  10  10  11              10 - root accessible, not all subnodes accessible
        // 11 | 11  11  11  11              11 - whole subtree accessible
        //
        // acc = a.acc | b.acc
        // sub = (a.acc & a.sub) | (b.acc & b.sub) | (b.sub & !a.acc) | (a.sub & !b.acc)
        _acc |= p._acc;
        _sub = (_acc & _sub) | (p._acc & p._sub) | (p._sub & ~_acc) | (_sub & ~p._acc);
    }

	uint merge_and( const U32PAIR& in1, const U32PAIR& in2 )
	{
		/*
			acc     sub            meaning
			 0       0          node not accessible, and neither any node below it
			 0       1          node not accessible, but some nodes under it are accessible
			 1       0          node accessible, but not all of the subordinated nodes are accessible
			 1       1          node accessible, and all of its subordinated nodes are accessible


                  00  01  10  11
                  --------------
             00 | 00  00  00  00
             01 | 00  01 *01* 01
             10 | 00 *01* 10  10
             11 | 00  01  10  11
		*/
		_acc = in1._acc & in2._acc;
/*
		_sub = (in1._sub & in2._sub) |	(~in1._acc & in1._sub & in2._acc & ~in2._sub) |
										(in1._acc & ~in1._sub & ~in2._acc & in2._sub);
			- or: (_sub1 & _sub2)    |  ((01&10  ||  10&01)   ==> 2*XOR, and one cross XOR (to avoid 01&01 or 10&10))
*/
		_sub = (in1._sub & in2._sub) |	(in1._acc ^ in1._sub) & (in2._acc ^ in2._sub) & (in1._sub ^ in2._sub);

		return _acc;// | _sub;			// if nothing is accessible, return zero
	}

    U32PAIR operator ~ () const         { return U32PAIR (_acc, 0); }

    U32PAIR () { }
    U32PAIR (uint acc, uint sub)      { _acc = ~acc;  _sub = sub; }

	friend binstream& operator << (binstream& out, const U32PAIR& u) {
		return out << u._acc;
	}

	friend binstream& operator >> (binstream& in, U32PAIR& u) {
		return in >> u._acc;
	}
};


////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY = TTREE_NOMAP<T> >
struct ttree_bitmap
{
private:
    dynarray<U32PAIR>       _bm;          ///< bitmap array
    uint   _flags;

public:
    enum {
        fDEFAULT_SELECTED       = 1,
        fSTREAM_BITMAP          = 2,
    };

    typedef ttree<T, NCT, MAP_POLICY>    TYPE_TTREE;
    typedef TYPE_TTREE::forest           TYPE_FOREST;

    ttree_bitmap ()
    {
        _flags = 0;
    }

		/// if you want the bitmap for modifying
	dynarray<U32PAIR> & get_bmp_modify() {return _bm;}

    void set_default_select (bool set=true)
    {
        if (set)
            _flags |= fDEFAULT_SELECTED;
        else
            _flags &= ~fDEFAULT_SELECTED;
    }

    void set_stream_flag (bool set=true)
    {
        if (set)
            _flags |= fSTREAM_BITMAP;
        else
            _flags &= ~fSTREAM_BITMAP;
    }

    bool def_sel () const       { return (_flags & fDEFAULT_SELECTED) != 0; }
    bool stream_bmp () const    { return (_flags & fSTREAM_BITMAP) != 0; }

    uint get_flags () const    { return _flags; }
    uint set_flags (uint f)   { return _flags = f; }

    uint adjust_size (ttree_bitmap& t)
    {
	    uint size = _bm.size();
	    int diff = size - t._bm.size();

		/// make arrays the same size
	    if (diff > 0)
		    t._bm.addc (diff, t.def_sel());
	    else if (diff < 0)
        {
		    _bm.addc (-diff, def_sel());
		    size = t._bm.size();
	    }
        return size;
    }

    ///Set specified key as accessible
    void set_bit (LNID n)      {
        uint w = n>>5;
        if (w >= _bm.size())    _bm.addc (w+1-_bm.size(), def_sel());
        _bm[w]._acc |= 1 << (n&0x1f);
    }

    ///Set specified key as inaccessible
    void clr_bit (LNID n)      {
        uint w = n>>5;
        if (w >= _bm.size())    _bm.addc (w+1-_bm.size(), def_sel());
        _bm[w]._acc &= ~(1 << (n&0x1f));
    }

    ///Set sub flags of specified key
    void set_bit_sub (LNID n)      {
        uint w = n>>5;
        if (w >= _bm.size())    _bm.addc (w+1-_bm.size(), def_sel());
        _bm[w]._sub |= 1 << (n&0x1f);
    }

    ///Clear sub flags of specified key
    void clr_bit_sub (LNID n)      {
        uint w = n>>5;
        if (w >= _bm.size())    _bm.addc (w+1-_bm.size(), def_sel());
        _bm[w]._sub &= ~(1 << (n&0x1f));
    }

    ///Get accessibility of specified key
    uint get_bit (LNID n) const    {
        uint w = n>>5;
        if (w >= _bm.size())  return def_sel() ? 1 : 0;
        return (_bm[w]._acc >> (n&0x1f)) & 1;
    }

    ///Get sub flag of specified key
    uint get_bit_sub (LNID n) const    {
        uint w = n>>5;
        if (w >= _bm.size())  return ! def_sel();		/// def_sel() == get_bit() ('w' is out of range) ==> invert second bit (==> 10 || 01, 11 && 00 could be wrong)
        return (_bm[w]._sub >> (n&0x1f)) & 1;
    }

    void get_pair (LNID n, U32PAIR& pair) const {
        uint w = n>>5;
        if (w >= _bm.size())  { pair._acc = pair._sub = def_sel() ? 1 : 0; return; }
        pair._acc = (_bm[w]._acc >> (n&0x1f)) & 1;
        pair._sub = (_bm[w]._sub >> (n&0x1f)) & 1;
    }

    ///Check whether subordinated keys can be accessible
    uint can_access_sub (LNID n) const {
        uint w = n>>5;
        if (w >= _bm.size())  return def_sel() ? 1 : 0;
        return (_bm[w]._sub >> (n&0x1f)) & 1;
    }

    ///If all keys under the \a n and the \a n are accessible
    uint all_accessible (LNID n) const {
        uint w = n>>5;
        if (w >= _bm.size())  return def_sel() ? 1 : 0;
        return _bm[w]._sub  &  _bm[w]._acc  &  (1 << (n&0x1f));
    }

    ///If any of keys under the \a n or the \a n are accessible
    uint any_accessible (LNID n) const {
        uint w = n>>5;
        if (w >= _bm.size())  return def_sel() ? 1 : 0;
        return (_bm[w]._sub  |  _bm[w]._acc)  &  (1 << (n&0x1f));
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Compute sub flags
    void compute_flags (const TYPE_FOREST& f, uint tid, dim_access& minacc)
    {
        TYPE_TTREE::ID root (*f.get_tree (tid), 0);
        uint full=1, min=UMAX, max=0;
        LNID vis=UMAX;

        compute_flags (root, full, min, max, vis);

        minacc.root = COID(tid,vis);
        minacc.level_from = min;
        minacc.level_to = max;
    }

    void invert_all ()
    {
        for (uint i=0; i<_bm.size (); ++i)
            _bm[i] = ~_bm[i];
        _flags ^= fDEFAULT_SELECTED;
    }

    void clear_all (bool toones=false)
    {
        _bm.reset();
        if (toones)
            _flags |= fDEFAULT_SELECTED;
        else
            _flags &= ~fDEFAULT_SELECTED;
    }

    void reset ()
    {
        _bm.reset();
    }

    void need (uint size, bool set=false)
    {
        _bm.need_newc (size, set);
    }

	bool merge_or( dynarray<U32PAIR> & out, bool defsel=false )
	{
	    uint size = _bm.size();
	    int diff = size - out.size();

		/// make arrays the same size
	    if( diff > 0 ) out.addc (diff, defsel );
	    else if( diff < 0 ) {
		    _bm.addc (-diff, def_sel());
		    size = out.size();
	    }

		for( uint i=0; i<size; i++ )
			out[i].merge( _bm[i] );

		return size != 0;
	}

	bool merge_or( dynarray<ulong> & out )
	{
	    uint size = _bm.size();
	    int diff = size - out.size();

		/// make arrays the same size
	    if (diff > 0)
		    out.addc (diff, false);
	    else if (diff < 0) {
		    _bm.addc (-diff, def_sel());
		    size = out.size();
	    }

		for( uint i=0; i<size; i++ )
			out[i] = out[i] | _bm[i]._acc;

		return size != 0;
	}

    bool merge_and (ttree_bitmap& t, ttree_bitmap& out)
    {
        uint size = adjust_size (t);
        out.need (size, false);

        uint ret = 0;
	    for (uint i=0; i<size; ++i)
		    ret |= out._bm[i].merge_and (_bm[i], t._bm[i]);

        return ret != 0;
    }

	const dynarray<U32PAIR> * get_bitmap() { return &_bm; }


    ////////////////////////////////////////////////////////////////////////////////
    ///Apply rule that adds accessible keys
    /**
        @param key root key of rule
        @param fromdepth relative depth from which the operation applies
        @param todepth relative depth to which the operation applies
        Adds subset of accessible nodes, and sets flag that subordinated nodes are accessible to all parents
        of the added nodes. The method with two depth arguments can be broken to several calls to simpler method
        with one depth argument (todepth), with root computed from given key and the \a fromdepth argument.
    */
#define LOWEST_LEVEL(x)						(x == UMAX-1)

    uint set_liberating_rule (const TYPE_TTREE::ID& root, LNID to, uint fromdepth, uint todepth)
    {
        TYPE_TTREE::ID id = root;

		if( fromdepth >= id.tree_depth() && ! LOWEST_LEVEL(fromdepth) )
			fromdepth = id.tree_depth();
        if( todepth >= id.tree_depth() )
            todepth = id.tree_depth();

        if( fromdepth == 0 || (fromdepth==UMAX-1 && id.is_lowest()) )
            set_liberating_rule( id, to, todepth );
        else if( id == to )
            return -1;
        else if( id.goto_first() )
        {
		    if( LOWEST_LEVEL(fromdepth) )
				fromdepth = UMAX;
			for (;;)
            {
				set_liberating_rule (id, to, fromdepth-1, todepth-1);
                if( id == to )  break;
				if( !id.goto_next())  break;
			}
        }

	    return -1;
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Apply rule that removes accessible keys
    /**
        @param key root key of rule
        @param fromdepth relative depth from which the operation applies
        @param todepth relative depth to which the operation applies
        Removes subset of accessible nodes, and sets flag that subordinated nodes aren't accessible to all parents
        of the removed nodes. The method with two depth arguments can be broken to several calls to simpler method
        with one depth argument (todepth), with root computed from given key and the \a fromdepth argument.
    */
    uint set_restricting_rule (const TYPE_TTREE::ID& root, LNID to, uint fromdepth, uint todepth)
    {
        TYPE_TTREE::ID id = root;

		if( fromdepth >= id.tree_depth() && ! LOWEST_LEVEL(fromdepth) )
			fromdepth = id.tree_depth();
        if( todepth >= id.tree_depth() )
            todepth = id.tree_depth();

        if( fromdepth == 0 || (fromdepth==UMAX-1 && id.is_lowest()) )
            set_restricting_rule( id, to, todepth );
        else if( id == to )
            return -1;
        else if( id.goto_first() )
        {
		    if( LOWEST_LEVEL(fromdepth) )
				fromdepth = UMAX;
			for (;;) {
				set_restricting_rule (id, to, fromdepth-1, todepth-1);
                if( id == to )  break;
				if( !id.goto_next() )  break;
			}
        }

	    return -1;
    }

    friend binstream& operator << (binstream& out, const ttree_bitmap<T,NCT,MAP_POLICY>& tb)
    {
        out << tb._flags;
        if (tb._flags & tb.fSTREAM_BITMAP)
            out << tb._bm;
        return out;
    }

    friend binstream& operator >> (binstream& in, ttree_bitmap<T,NCT,MAP_POLICY>& tb)
    {
        in >> tb._flags;
        if (tb._flags & tb.fSTREAM_BITMAP) {
            in >> tb._bm;
			for( ulong i=0; i<tb._bm.size(); i++ )		/// _subs should be reset (there are structs with many of these)
				tb._bm[i]._sub = 0xFF;
        }
        else
            tb._bm.reset();
        return in;
    }

protected:
    ////////////////////////////////////////////////////////////////////////////////
    ///Apply rule that adds accessible keys
    void set_liberating_rule (const TYPE_TTREE::ID& root, LNID to, uint todepth)
    {
        TYPE_TTREE::ID id = root;
        TYPE_TTREE::ID rt = root;

        uint lev = root.get_level() + todepth;

        if (to != UMAX)
        {
            rt.set_node_id(to);
            uint levto = rt.get_level();
            while( levto > lev )
            {
                rt.goto_superior_local();
                --levto;
            }
        }

        set_bit (id);
        for (;;)
        {
            if (!id.scanto_next (rt, (ushort) lev))  break;
            set_bit (id);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Apply rule that removes accessible keys
    void set_restricting_rule (const TYPE_TTREE::ID& root, LNID to, uint todepth)
    {
        TYPE_TTREE::ID id = root;
        TYPE_TTREE::ID rt = root;

        uint lev = root.get_level() + todepth;

        if (to != UMAX)
        {
            rt.set_node_id(to);
            uint levto = rt.get_level();
            while( levto > lev )
            {
                rt.goto_superior_local();
                --levto;
            }
        }

        clr_bit (id);
        for (;;)
        {
            if (!id.scanto_next (rt, (ushort) lev))  break;
            clr_bit (id);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Compute sub flags recursively
    /**
        @param n gets number of nodes traversed
        @param a gets number of nodes accessible
    */
    uint compute_flags (const TYPE_TTREE::ID& root, uint& full, uint& min, uint& max, LNID& vis)
    {
        TYPE_TTREE::ID id = root;

        if (id.goto_first())
        {
            uint fulli=1;
            uint mini=UMAX, maxi=0;
            uint ct=0;
            for (;;)
            {
                ct += compute_flags (id, fulli, mini, maxi, vis);
                if (!id.goto_next())  break;
            }

            if (mini!=UMAX) ++mini;
            if (maxi>0) ++maxi;

            if (get_bit(root)) {
                if (fulli)      set_bit_sub (root);
                else            clr_bit_sub (root);
                mini = 0;
                if (maxi==0)  maxi = 1;
                ++ct;
                vis = root;
            }
            else {
                if (ct)         set_bit_sub (root);
                else            clr_bit_sub (root);
                fulli = 0;

                // if more than one child subtrees are visible, this should be visibility root
                if (ct>1)
                    vis = root;
            }

            if (!fulli)
                full = 0;

            if (mini < min)
                min = mini;
            if (maxi > max)
                max = maxi;

            return ct ? 1 : 0;
        }
        else
        {
            if (get_bit(root))
            {
                set_bit_sub (root);
                min = 0;
                if (max==0) max=1;
                return 1;
            }
            else
            {
                clr_bit_sub (root);
                full = 0;
            }

            return 0;
        }
    }

};

COID_NAMESPACE_END

#endif //__COMM_TREE_BITMAP_H__
