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
 * The Original Code is coid/comm module.
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

#ifndef __COMMON_TTREE_HEADER__
#define __COMMON_TTREE_HEADER__

#include "../namespace.h"

#include "../dynarray.h"
#include "nodeclass.h"
#include "ttreeid.h"

#include <map>

COID_NAMESPACE_BEGIN

////////////////////////////////////////////////////////////////////////////////
///Policy class for ttree, no mapping
template <class T>
class TTREE_NOMAP
{
public:
    uint find_code (const token& code) const        { return UMAX; }
    uint find_code (const T* node) const            { return UMAX; }
    bool insert_code (const T* node, uint id)       { return true; }
    bool exists_code (const T* node) const          { return false; }
    bool change_code (const T* node, token __oldcode) { return true; }
    bool change_code (const T* node, token __oldcode, uint __id) { return true; }
    token get_code (const T* node) const      { return 0; }

    bool is_enabled() const    { return false; }
    void enable (bool en=true)  { }
    bool erase (const T* node) { return true; }
};

///Policy class for ttree, hash mapping
template <class T>
class TTREE_MAP
{
    keywordmap<charstr, uint, hash<token> > _map;         ///< keyword (hash) table
    bool    _en;                            ///< enable/disable flag
public:

    ///Find node id by \a code
    uint find_code (const token& code) const        { uint* m = _map.find (code);  if (!m)  return UMAX;  return *m; }
    ///Find node id by node class
    uint find_code (const T* node) const            { uint* m = _map.find (node->get_code());  if (!m)  return UMAX;  return *m; }

    ///Insert value under code from \a node
    bool insert_code (const T* node, uint id)       { return _map.insert (node->get_code(), id); }
    ///Test if node is already listed
    bool exists_code (const T* node) const          { return find_code(node->get_code()) != UMAX; }

    ///Remove old code and insert new node
    bool change_code (const T* node, const token& __oldcode)
    {
        if (!node) return false;
        uint* m = _map.find(__oldcode);
        if (!m)  return false;

        uint v = *m;
        _map.erase (__oldcode);
        _map.insert (node->get_code(), v);
        return true;
    }

    ///Remove old code and insert new node
    bool change_code (const T* node, const token& __oldcode, uint __id)
    {
        if (!node) return false;
        token oldc = __oldcode;
        token newc = node->get_code();

        if( oldc == newc )
            return true;

        uint* m = _map.find(oldc);
        uint* n = _map.find(newc);
        
        //if already exists
        if( n && (n != m) )
            return false;

        //insert
        if(!m) {
            if (__id == UMAX) 
                return false;
            else 
                return insert_code( node, __id );
        }

        uint v = *m;
        _map.erase(oldc);
        return _map.insert( newc, v );
        //return true;
    }

    ///Retrieve code from node
    token get_code (const T* node) const        { return node->get_code(); }

    ///Return true if hash table is enabled
    bool is_enabled() const    { return _en; }
    ///Enable hash table
    void enable (bool en=true)
    {
        if (!en && _en)
            _map.discard();
        _en = en;
    }

    bool erase (const T* node) {
        return _map.erase( node->get_code() );
    }

    TTREE_MAP()
    {
        _en = false;
    }
};

////////////////////////////////////////////////////////////////////////////////
///Tree template class
/**The ttree class manages tree from a container of trees (forest). The tree consist of hierarchically bound
nodes of arbitrary classes that are derived from one common class T passed in as the argument of the ttree template
class. The nodes are organized within a singe tree, but each node can be referring to multiple other nodes. For
each node the ttree records ptr to the node object, id of the class of the node as well as the control
structures needed to provide browsing capabilities. The nodes can be queried by a class name, with parent-child
related queries and with a referring-hooked onto related queries.
*/
template <class T, class NCT, class MAP_POLICY = TTREE_NOMAP<T> >
class ttree
{
public:

    typedef T           TYPE_NODE;
    typedef MAP_POLICY  TYPE_MAP_POLICY;

    //typedef const NCT*  NodeClass;
//    typedef typename ClassRegister<NCT>::NodeClass       NodeClass;

protected:
    ///NODE structure, managed by the ttree class
    /**
        This structure keeps data required to manage a node within tree.
    */
    class NODE
    {
    public:
        COID            _superior;          ///< superior node identifier
        dynarray<COID>  _asubnodes;         ///< array of subordinated nodes

        typename ClassRegister<NCT>::NodeClass _class;  ///< node class identifier/instructor
        T*              _node;              ///< ptr to object of type T or of a derived type

        dynarray<COID>  _ahookref;          ///< the nodes this node is hooked onto
        dynarray<COID>  _ahooked;           ///< the nodes hooked onto this node

        uint            _seqid;             ///< sequential id of the node within the tree
        ushort          _wlev;              ///< level of the node in the tree
        //ttree<T>*   _base;

        NODE() { _node= NULL; }

        NODE (const NODE& n)
        {
            _superior = n._superior;
            _asubnodes = n._asubnodes;
            _class = n._class;
            _ahookref = n._ahookref;
            _ahooked = n._ahooked;
            _seqid = n._seqid;
            _wlev = n._wlev;

            _node= (T*)_class->copy(n._node);
        }

        NODE& operator = (const NODE& n)
        {
            if( n._class.is_set() )
                new (this) NODE (n);
            else
                _node= NULL;
            return *this;
        }

        ///Add specified node as child on given position in the list of children nodes
        void add (COID id, uint pos)       {
            if (pos >= _asubnodes.size())  *_asubnodes.add(1) = id;
            else  *_asubnodes.ins(pos) = id;
        }

        ///Delete specified node from the list of children nodes
        bool del( COID id )
        {
            for( uint i=0; i<_asubnodes.size(); ++i )
                if( _asubnodes[i] == id )   { _asubnodes.del(i); return true; }
            return false;
        }

        ///Returns level of the node
        uint get_level() const            { return _wlev; }

        ///Returns identifiers of nodes that are hooked onto this node
        dynarray<COID>& get_hooked (dynarray<COID>& out) const        { out = _ahooked; return out; }
        ///Returns identifiers of nodes to which this node is hooked to
        dynarray<COID>& get_referred (dynarray<COID>& out) const      { out = _ahookref; return out; }
        ///Returns identifiers of children nodes
        dynarray<COID>& get_subordinated (dynarray<COID>& out) const  { out = _asubnodes; return out; }

        ///Test if this node is hooked on to \a id_ref node
        ///@return -1 if not, position in the target list of hooked nodes otherwise
        int is_hooked_to (COID id_ref) const    { return find_in_list (_ahookref, id_ref); }
        ///Test if id_ref node is hooked on to this node
        ///@return -1 if not, position in the list of hooked nodes otherwise
        int is_referred_by (COID id_node) const { return find_in_list (_ahooked, id_node); }
        ///Test if given node is child of this node
        ///@return -1 if not, position in the list of children nodes otherwise
        int find_id (COID id) const             { return find_in_list (_asubnodes, id); }

        bool has_children() const               { return _asubnodes.size() > 0; }
        bool has_dependants() const             { return _ahooked.size() > 0; }

        friend binstream& operator << (binstream& out, const NODE& n) {
            out << n._superior << n._asubnodes << n._class
                << n._ahookref
                << n._ahooked << n._seqid << n._wlev;
            if( n._class.is_set() )
			    n._class->stream_out( out, n._node );
            return out;
        }
        friend binstream& operator >> (binstream& in, NODE& n)        {
            in >> n._superior >> n._asubnodes >> n._class
                >> n._ahookref
                >> n._ahooked >> n._seqid >> n._wlev;
            n._node = 0;
            if( n._class.is_set() )
                n._node = (T*) n._class->stream_in_new(in);
            return in;
        }

    protected:
        static ints find_in_list (const dynarray<COID>& list, COID id_ref)
        {
            uints lim = list.size();
            if (!id_ref.can_match_any_tree()) {
                for (uints i=0; i<lim; ++i)  {
                    if (list[i] == id_ref)  return i;
                }
            }
            else {
                uint id = id_ref._id & 0x00ffffff;
                for (uints i=0; i<lim; ++i)  {
                    if ((list[i]._id & 0x00ffffff) == id)  return i;
                }
            }
            return -1;
        }
    };

public:

    ///NODE identifier
    /**
    Unlike COID, this is 8B struct that keeps a pointer to the tree where the node resides, and id of the node in the tree.
    COID can be cast from ID automatically. This class has overloaded operator -> allowing access to the
    node-managed class.
    */
    class ID
    {
        friend class ttree<T,NCT,MAP_POLICY>;
        uint       _id;
        ttree<T,NCT,MAP_POLICY>*   _tree;

    public:
        operator const NODE*() const            { return &_tree->_nodes[_id]; }
        operator NODE*()                        { return &_tree->_nodes[_id]; }
        operator COID () const                  { if (_id== UMAX) return COID::get_coid(UMAX); return COID (_tree->get_tree_id(), _id); }
        operator LNID () const                  { return _id; }
        const NODE* operator ->() const         { return &_tree->_nodes[_id]; }
        NODE* operator ->()                     { return &_tree->_nodes[_id]; }

        const T& operator * () const            { return *_tree->_nodes[_id]._node; }
        T& operator * ()                        { return *_tree->_nodes[_id]._node; }

        T* ptr()                                { return _tree->_nodes[_id]._node; }
        const T* ptr() const                    { return _tree->_nodes[_id]._node; }

        ///Setup from specified COID
        ///@note the _tree member must be already initialized to point to a valid tree from the forest
        ID& operator = (const COID id)
        {
            _id = id.get_node_id();
            RASSERTX( _tree, "_tree not set" );
            _tree = (*_tree->_forest)[id.get_tree_id()];
            return *this;
        }

        uint tree_depth() const                 { return _tree->depth(); }

        bool operator == (const ID& id) const   { return _id == id._id  &&  _tree == id._tree; }
        bool operator == (const COID id) const  { return COID (_tree->get_tree_id(), _id) == id; }
        bool operator == (const LNID id) const  { return _id == id; }

        bool operator != (const ID& id) const   { return _id != id._id  ||  _tree != id._tree; }
        bool operator != (const COID id) const  { return COID (_tree->get_tree_id(), _id) != id; }
        bool operator != (const LNID id) const  { return _id != id; }

        ///Returns true if this node has specified node class
        bool is_of_class (const typename ClassRegister<NCT>::NodeClass nc) const { NODE* n = get_node();  return n->_class.is_of_type(nc); }
        bool is_of_class (const token& nc, const version* ver) const
        {
            if( nc.is_empty() ) return true;
            NODE* n = get_node();
            return n->_class.is_of_type(nc,ver);
        }

        ///Get ptr to node from the same tree, but with possibly different node id
        NODE* get_node (uint id) const          { return (NODE*)&_tree->_nodes[id]; }
        ///Get ptr to node
        NODE* get_node() const                  { return (NODE*)&_tree->_nodes[_id]; }

        token get_code() const                  { return _tree->_codemap.get_code( get_node()->_node ); }

        ///Get level of the node
        uint get_level() const                  { return _tree->_nodes[_id].get_level(); }


        bool has_children() const               { return _tree->_nodes[_id]._asubnodes.size() > 0; }
        bool has_dependants() const             { return _tree->_nodes[_id]._ahooked.size() > 0; }

        ///Returns true if this and the given node reside in the same tree
        bool is_same_tree (COID id) const       { return id.get_tree_id() == _tree->_idinforest; }
        ///Returns true if this identifier represents a tree root node
        bool is_local_tree_root() const         { return _id == 0; }
        ///Returns true if this node has no children
        bool is_lowest() const {
            const NODE* node = *this;
            return node->_asubnodes.size() == 0;
        }

        ///Set the identifier to an invalid value
        void set_invalid()                     { _tree = 0;  _id = UMAX; }
        ///Test whether the identifier is set to an invalid value
        bool is_invalid() const                { return _id == UMAX  ||  _tree == 0; }

        ///Get node number in the tree
        uint get_node_id() const               { return _id; }
        ///Get tree number in the forest
        uint get_tree_id() const               { if (_tree) return _tree->_idinforest; return UMAX;}

        void set_node_id (LNID id)              { _id = id; }

        ///Returns pointer to the tree object
        ttree<T,NCT,MAP_POLICY>* ret_tree() const  { return _tree; }

        ///Set id to point to the first child
        ///@return true if succeeded, false if no children
        bool goto_first()
        {
            const NODE* node = *this;
            if (0 == node->_asubnodes.size())  return false;
            COID id = node->_asubnodes[0];
            if (is_same_tree (id)) {
                _id = id.get_node_id();
                return true;
            }
            _tree->_get_node( id, *this );
            return true;
        }

        ///Set id to point to the next node (the next child under the parent node)
        ///@return true if succeeded, false if this was the last child of the parent node
        bool goto_next()
        {
            ID nid = *this;
            if (!nid.goto_superior())  return false;        //no superior => no neighbours

            const NODE* node = nid;
            uint pos = node->find_id (*this);

            if (++pos < node->_asubnodes.size())
            {
                COID id = node->_asubnodes[pos];
                if (is_same_tree (id))
                    _id = id.get_node_id();
                else
                    _tree->_get_node( node->_asubnodes[pos], *this );
                return true;
            }
            return false;
        }

        ///Set id to point to the previous node (the previous child under the parent node)
        ///@return true if succeeded, false if this is the first child of the parent node
        bool goto_prev()
        {
            ID nid = *this;
            if (!nid.goto_superior())  return false;        //no superior => no neighbours

            const NODE* node = nid;
            uint pos = node->find_id (*this);

            if (pos-- > 0)
            {
                COID id = node->_asubnodes[pos];
                if (is_same_tree (id))
                    _id = id.get_node_id();
                else
                    _tree->_get_node( node->_asubnodes[pos], *this );
                return true;
            }
            return false;
        }

        ///Set id to point to the first child node, that has the specified node class
        ///@return true if succeeded, false if there's no child with given class
        bool goto_first_of_class (const typename ClassRegister<NCT>::NodeClass nc)
        {
            const NODE* node = *this;
            uint lim = node->_asubnodes.size();

            for (uint i=0; i<lim; ++i)
            {
                COID id = node->_asubnodes[i];
                if (is_same_tree(id))
                {
                    uint idd = id.get_node_id();
                    NODE& n = _tree->_nodes[idd];
                    if( n._class.is_of_type(nc) )
                    {
                        _id = idd;
                        return true;
                    }
                }
                else
                {
                    NODE* n = _tree->_get_node( node->_asubnodes[i] );
                    if( n->_class.is_of_type(nc) ) {
                        *this = node->_asubnodes[i];
                        return true;
                    }
                }
            }
            return false;
        }

        ///Set id to point to the first child node, that has the specified node class
        ///@return true if succeeded, false if there's no child with given class
        bool goto_first_of_class( const token& nc, const version* ver=0 )
        {
            const NODE* node = *this;
            uint lim = node->_asubnodes.size();

            for( uint i=0; i<lim; ++i )
            {
                COID id = node->_asubnodes[i];

                if( is_same_tree(id) ) 
                {
                    uint idd = id.get_node_id();
                    NODE& n = _tree->_nodes[idd];
                    if( nc.is_empty() || n._class.is_of_type(nc,ver) ) {
                        _id = idd;
                        return true;
                    }
                }
                else {
                    NODE* n = _tree->_get_node( node->_asubnodes[i] );

                    if( nc.is_empty() || n->_class.is_of_type(nc,ver) ) {
                        *this = node->_asubnodes[i];
                        return true;
                    }
                }
            }
            return false;
        }

        ///Set id to point to the next node (the next child under the parent node) that has the specified node class
        ///@return true if succeeded, false if this was the last child of the parent node with given class
        bool goto_next_of_class (const typename ClassRegister<NCT>::NodeClass nc)
        {
            ID nid = *this;
            if (!nid.goto_superior())  return false;        //no superior => no neighbours

            const NODE* node = nid;
            uint pos = node->find_id (*this);

            while (++pos < node->_asubnodes.size())
            {
                COID id = node->_asubnodes[pos];
                if( is_same_tree(id) )
                {
                    uint idd = id.get_node_id();
                    NODE& n = _tree->_nodes[idd];
                    if( n._class.is_of_type(nc) ) {
                        _id = idd;
                        return true;
                    }
                }
                else
                {
                    NODE* n = _tree->_get_node( node->_asubnodes[pos] );
                    if( n->_class.is_of_type(nc) ) {
                        *this = node->_asubnodes[pos];
                        return true;
                    }
                }
            }
            return false;
        }

        ///Set id to point to the next node (the next child under the parent node) that has the specified node class
        ///@return true if succeeded, false if this was the last child of the parent node with given class
        bool goto_next_of_class( const token& nc, const version* ver=0 )
        {
            ID nid = *this;
            if (!nid.goto_superior())  return false;        //no superior => no neighbours

            const NODE* node = nid;
            uint pos = node->find_id (*this);

            while (++pos < node->_asubnodes.size())
            {
                if( nc.is_empty() )  return true;

                COID id = node->_asubnodes[pos];
                if (is_same_tree(id))
                {
                    uint idd = id.get_node_id();
                    NODE& n = _tree->_nodes[idd];
                    if( n._class.is_of_type(nc,ver) ) {
                        _id = idd;
                        return true;
                    }
                }
                else {
                    NODE* n = _tree->_get_node( node->_asubnodes[pos] );
                    if( n->_class.is_of_type(nc,ver) ) {
                        *this = node->_asubnodes[pos];
                        return true;
                    }
                }
            }
            return false;
        }

        ///Set id to point to the parent node
        ///@return true if succeeded, false if this was an absolute root node
        bool goto_superior()
        {
            if (is_local_tree_root()) {
                if (_tree->_foreign._id == UMAX)  return false;
                *this = _tree->_foreign;
                return true;
            }
            _id = _tree->_nodes[_id]._superior.get_node_id();
            return true;
        }

        ///Set id to point to the parent node, but do not cross to another tree
        ///@return true if succeeded, false if this was a root node
        bool goto_superior_local()
        {
            if (is_local_tree_root())
                return false;
            _id = _tree->_nodes[_id]._superior.get_node_id();
            return true;
        }

        ///Set id to point to the superior node with specified class
        ///@return true if succeeded, false if there is no such superior node
        bool goto_superior_of_class (const typename ClassRegister<NCT>::NodeClass nc)
        {
            ID nid = *this;
            if (!nid.goto_superior())  return false;
            while( !nid->_class.is_of_type(nc) )
            {
                if( !nid.goto_superior() )  return false;
            }
            *this = nid;
            return true;
        }

        ///Return true if given node is one of the superior nodes for current node
        bool has_superior (COID root) const
        {
            ID nid = *this;
            while (nid != root)
            {
                if (!nid.goto_superior())  return false;
            }
            return true;
        }

        ///Return true if given node is one of the superior nodes for current node in the current tree
        bool has_superior (LNID root) const
        {
            return has_superior (COID(get_tree_id(), root));
        }

        ///Set id to point to the last node in the linear expansion of the tree nodes
        void scanto_last()
        {
            const NODE* node = *this;
            if (0 == node->_asubnodes.size())  return;
            COID id = node->_asubnodes[node->_asubnodes.size()-1];
            if (is_same_tree (id)) {
                _id = id.get_node_id();
                scanto_last();
                return;
            }
            _tree->_get_node( id, *this );
            scanto_last();
        }

        ///Set id to point to the next node in the linear expansion of the tree nodes
        ///@return true if succeeded, false if this was the last node in the linear expansion
        bool scanto_next()
        {
            if (goto_first())  return true;
            if (goto_next())  return true;

            for (;;)
            {
                if (!goto_superior())  return false;
                if (goto_next())  return true;
            }
        }

        ///Set id to point to the next node in the linear expansion of the tree nodes, up to the specified node
        /**
            If the \a root node occurs after current node (in the linear expansion of the tree), this
            operation will succeed unless the root node is encountered. If the root node is before the
            current node, the operation will succeed unless the end of the linear expansion is found.
            @return true if succeeded, false if this was the last node in the linear expansion
        */
        bool scanto_next (const ID& root)
        {
            if (goto_first())  return true;

            for (;;)
            {
                if (*this == root) return false;
                if (goto_next())  return true;
                if (!goto_superior())  return false;
            }
        }

        ///Set id to point to the next node in the linear expansion of the tree nodes, up to the specified node, taking only nodes with levels lower than specified
        /**
            If the \a root node occurs after current node (in the linear expansion of the tree), this
            operation will succeed unless the root node is encountered. If the root node is before the
            current node, the operation will succeed unless the end of the linear expansion is found.
            Moreover, all nodes with level greater or equal than specified will be skipped.
            @return true if succeeded, false if this was the last node in the linear expansion
        */
        bool scanto_next (const ID& root, ushort lev)
        {
            if (lev > get_node()->_wlev  &&  goto_first())  return true;

            for (;;)
            {
                if (*this == root) return false;
                if (goto_next())  return true;
                if (!goto_superior())  return false;
            }
        }

        ///Set id to point to the next node in the linear expansion of the tree nodes, that has the specified class, up to the \a root node
        /**
            If the \a root node occurs after current node (in the linear expansion of the tree), this
            operation will succeed unless the root node is encountered. If the root node is before the
            current node, the operation will succeed unless the end of the linear expansion is found.
            All nodes that do not have the required class will be skipped.
            @return true if succeeded, false if this was the last node in the linear expansion
        */
        bool scanto_next_of_class (const ID& root, const typename ClassRegister<NCT>::NodeClass nc)
        {
            if (goto_first()) {
                NODE* n = get_node();
                if( n->_class.is_of_type(nc) )  return true;
                return scanto_next_of_class(root, nc);
            }

            for (;;)
            {
                if (*this == root) return false;
                if (goto_next()) {
                    NODE* n = get_node();
                    if( n->_class.is_of_type(nc) )  return true;
                    return scanto_next_of_class (root, nc);
                }
                if (!goto_superior())  return false;
            }
        }

        bool scanto_next_of_class (const ID& root, const token& nc, const version* ver=0 )
        {
            if (goto_first()) {
                if( nc.is_empty() )  return true;
                NODE* n = get_node();
                if( n->_class.is_of_type(nc,ver) )  return true;
                return scanto_next_of_class(root, nc);
            }

            for (;;)
            {
                if (*this == root) return false;
                if (goto_next()) {
                    if( nc.is_empty() )  return true;
                    NODE* n = get_node();
                    if( n->_class.is_of_type(nc,ver) )  return true;
                    return scanto_next_of_class (root, nc);
                }
                if (!goto_superior())  return false;
            }
        }

        ///Set id to point to the previous node in the linear expansion of the tree nodes
        ///@return true if succeeded, false if this is the first node in the linear expansion
        bool scanto_prev()
        {
            if (goto_prev())  { scanto_last();  return true; }
            if (goto_superior())  return true;
            return false;
        }

        ///Set id to point to the next node in the linear expansion of the tree nodes, ignoring the child nodes
        ///@return true if succeeded, false if this was the last node in the linear expansion
        bool scanto_next_sib()
        {
            if (goto_next())  return true;

            for (;;)
            {
                if (!goto_superior())  return false;
                if (goto_next())  return true;
            }
        }

        ///Set id to point to the next node in the linear expansion of the tree nodes, up to the specified node, ignoring the child nodes
        /**
            If the \a root node occurs after current node (in the linear expansion of the tree), this
            operation will succeed unless the root node is encountered. If the root node is before the
            current node, the operation will succeed unless the end of the linear expansion is found.
            Children nodes of current node aren't considered.
            @return true if succeeded, false if this was the last node in the linear expansion
        */
        bool scanto_next_sib (const ID& root)
        {
            for (;;)
            {
                if (*this == root)  return false;
                if (goto_next())  return true;
                if (!goto_superior())  return false;
            }
        }

        ///Return list of superior nodes of current node
        /**
            @param root highest node to go up to
            @param out memory where to store identifiers of the superior nodes
            @param count in: number of elements the \a out points to; out: actual count of the returned nodes
            @param toroot set to true if id's of root nodes above the given \a root node should be returned too
        */
        bool get_superiors (COID root, COID* out, uint& count, bool toroot=false) const
        {
            COID ac[256];
            ID sc = *this;

            if (root.can_match_any_tree())
                root.set_tree_id(_tree->_idinforest);

            long k= UMAX;
            long u;
            for (u=255; u>=0; --u)
            {
                ac[u] = sc;
                if (sc == root) {
                    if (toroot) {
                        bool t = sc.goto_superior();
                        k = u-1;
                        for (; k>=0 && t; --k) {
                            ac[k] = sc;
                            t = sc.goto_superior();
                        }
                        ++k;
                    }
                    break;
                }

                if (!sc.goto_superior())  return false;
            }

            if (toroot && k != UMAX)
            {
                if (256-k < (int)count)  count = 256-k;
                xmemcpy (out, ac+k, count*sizeof(COID));
            }
            else
            {
                if (256-u < (int)count)  count = 256-u;
                xmemcpy (out, ac+u, count*sizeof(COID));
            }
            return true;
        }

        bool get_superiors (LNID root, LNID* out, uint& count, bool toroot=false) const
        {
            LNID ac[256];
            ID sc = *this;

            long k,u;
            for (u=255; u>=0; --u)
            {
                ac[u] = sc;
                if (sc == root) {
                    if (toroot) {
                        bool t = sc.goto_superior_local();
                        k = u-1;
                        for (; k>=0 && t; --k) {
                            ac[k] = sc;
                            t = sc.goto_superior_local();
                        }
                        ++k;
                    }
                    break;
                }

                if (!sc.goto_superior_local())  return false;
            }

            if (toroot)
            {
                if (256-k < (int)count)  count = 256-k;
                xmemcpy (out, ac+k, count*sizeof(LNID));
            }
            else
            {
                if (256-u < (int)count)  count = 256-u;
                xmemcpy (out, ac+u, count*sizeof(LNID));
            }
            return true;
        }

        bool get_superiors_table (LNID root, LNID* out, uint* rootoffs, uint& count, bool toroot=false) const
        {
            ID sc = *this;

            uint dpt = sc.get_level();
            uint lev = dpt++;

            for (;;--lev)
            {
                if (count > lev)
                    out[lev] = sc;

                if (sc == root)
                {
                    *rootoffs = lev;
                    if (toroot)
                    {
                        for (--lev;;--lev)
                        {
                            if (!sc.goto_superior_local())
                                break;
                            if (count > lev)
                                out[lev] = sc;
                        }
                    }
                    count = dpt < count ? dpt : count;
                    return true;
                }

                if (!sc.goto_superior_local())
                    return false;
            }
        }

        ///Move the current node to specified position in its parent's children list
        void move (uint pos) const
        {
            ID nid = *this;
            if (!nid.goto_superior())  return;        //no superior => no neighbours

            NODE* node = nid;
            uint opos = node->find_id (*this);

            if (pos >= node->_asubnodes.size()  &&  pos)
                pos = node->_asubnodes.size() - 1;
            if (opos == pos)  return;

            COID key = node->_asubnodes[opos];
            node->_asubnodes.del (opos);
            *node->_asubnodes.ins (pos) = key;

            ID ko = nid;    //set _tree
            ko.set_node_id( key );
            ko.resequence();
        }

        ///Update sequence numbers after insert and/or delete operations on the tree
        void resequence() const
        {
            // compute _seqid value for node
            if (is_local_tree_root ())  return;
            uint n = nsubnodes() + 1;

            ID ip = *this;
            ip.scanto_prev ();
            uint psid = ip->_seqid;

            ID in = *this;
            if (!in.scanto_next_sib ()) {
                if (ip->_seqid >= UMAX - n)
                    _tree->shift_seqid_p (ip, n);
            }
            else
            {
                uint nsid = in->_seqid;
                if (nsid - psid == 1) {     //no space
                    _tree->insert_seqid (ip, n, in);
                }
            }

            ID id = *this;
            uint i = ip->_seqid;
            id->_seqid = i++;

            if (!id.goto_first())  return;

            for (; id != in; ++i) {
                id->_seqid = i;
                if (!id.scanto_next ()) break;
            }
        }

        ///Return number of nodes in the subtree with current node as root
        uint nsubnodes() const
        {
            if ((*this)->_asubnodes.size() == 0)  return 0;

            ID id = *this;
            ID idn = *this;

            id.goto_first ();
            idn.scanto_next_sib ();

            uint cnt;
            for (cnt=0; id != idn; ++cnt)
                if (!id.scanto_next ())  break;

            return cnt;
        }

        ///Setup from given tree and node number
        void set (const ttree<T,NCT,MAP_POLICY>& ref, LNID id)
        {
            _tree = (ttree<T,NCT,MAP_POLICY>*)&ref;
            _id = id;
        }

        bool change_code (const char* __old) {
            return _tree->change_code (_tree->_nodes[_id]._node, __old);
        }

        bool change_code (const char* __old, uint __id) {
            return _tree->change_code (_tree->_nodes[_id]._node, __old, __id);
        }

        ID (const ttree<T,NCT,MAP_POLICY>& ref, LNID id) : _id(id), _tree((ttree<T,NCT,MAP_POLICY>*)&ref)  {
            if ((id | 0xff000000) == UMAX)
                _id = UMAX;
        }
        explicit ID (int i) : _id(UMAX), _tree(0)  { }
        ID() { }
    };

    ////////////////////////////////////////////////////////////////////////////////
    ///Container for multiple potentially coupled trees
    class forest
    {
    public:
        typedef T           TYPE_NODE;
        typedef MAP_POLICY  TYPE_MAP_POLICY;
        typedef NCT         TYPE_NCT;

        typedef ttree<T,NCT,MAP_POLICY>     TREE;

        mutable comm_mutex  _mutex;
        dynarray< TREE* >   _forest;
        uint _nupd;


        friend binstream& operator << (binstream& bin, const forest& p)
        {
            bin << (uint)p._forest.size();
            uint lim = (uint)p._forest.size();
            for( uint i=0; i<lim; ++i )
            {
                bin << *p._forest[i];
            }
            return bin;
        }
        friend binstream& operator >> (binstream& bin, forest& p)
        {
            uint lim;
            bin >> lim;
            //p._forest.need_new (lim);
            for( uint i=0; i<lim; ++i )
            {
                ttree<T,NCT,MAP_POLICY>* t = p.add_tree();
                bin >> *t;
            }
            return bin;
        }


        forest() : _nupd(0) {_mutex.set_name( "forest" );}

        forest& operator = (const forest& f)
        {
            _forest.need_new (f._forest.size());
            for (uint i=0; i<f._forest.size(); ++i)
            {
                _forest[i] = new ttree<T,NCT,MAP_POLICY> (*f._forest[i]);
                _forest[i]->_forest = this;
            }
            return *this;
        }

        ///Return tree by tree id
        TREE* operator [] (uint i) const       {
            RASSERTX( i < _forest.size(), "invalid tree id" );
            return _forest[i];
        }

        ///Return tree by tree name
        TREE* operator [] (const token& name) const       {
            return get_tree(name);
        }

        ///Return tree by tree id
        TREE* get_tree (uint i) const {
            if (i >= _forest.size())  return 0;
            return _forest[i];
        }

        ///Return tree by tree name
        TREE* get_tree (const token& name) const {
            for( uint i=0; i<_forest.size(); ++i )
                if( _forest[i]->get_name() == name )  return _forest[i];
            return 0;
        }

        ///Return tree by tree name
        uint get_tree_id( const token& name ) const {
            for( uint i=0; i<_forest.size(); ++i )
                if( _forest[i]->get_name() == name )  return i;
            return UMAX;
        }

        ///Return node by COID
        const NODE* operator [] (COID id) const     { return _get_node(id); }

        ///Add specified tree to forest
        uint add( TREE* t )
        {
            _mutex.lock();
            uint id = _forest.size();
            ttree<T,NCT,MAP_POLICY>** p = _forest.add (1);
            *p = t;
            _mutex.unlock();
            return id;
        }

        ///Add new tree to forest
        TREE* add_tree()
        {
            _mutex.lock();
            uint id = _forest.size();
            ttree<T,NCT,MAP_POLICY>** p = _forest.add (1);
            *p = new ttree<T,NCT,MAP_POLICY> (*this, id);
            _mutex.unlock();
            return _forest[id];
        }

        ///Return last dimension in what element of key vector belongs hierarchically under another
        /**
            @param key vector that should be subordinated
            @param root vector that should be superior
            @param dims corresponding tree identifiers
            Number of node id's pointed to by \a key and \a root is specified by the number of
            tree identifiers contained within the \a dims dynarray. Each tree id from the array
            specifies corresponding tree for each element of \a key and \a root.
            @return -1 if failed, position of last dimension that succeeded otherwise
        */
        long belongs_under_dims (const LNID* key, const LNID* root, const dynarray<uint>& dims)
        {
            uint i;
            for (i=0; i<dims.size(); ++i)
            {
                RASSERTX( dims[i] < _forest.size(), "invalid tree id" );
                if (!_forest[dims[i]]->belongs_under (key[i], root[i]))  return i-1;
            }
            return i-1;
        }

        ///Return last dimension in what element of key vector groups under another (in expanded linear lists)
        /**
            @param key vector that should be subordinated
            @param root vector that should be superior
            @param dims corresponding tree identifiers
            Number of node id's pointed to by \a key and \a root is specified by the number of
            tree identifiers contained within the \a dims dynarray. Each tree id from the array
            specifies corresponding tree for each element of \a key and \a root.
            @return -1 if failed, position of last dimension that succeeded otherwise
        */
        long groups_under_dims (const LNID* key, const LNID* root, const dynarray<uint>& dims)
        {
            uint i;
            for (i=0; i<dims.size(); ++i)
            {
                RASSERTX( dims[i] < _forest.size(), "invalid tree id" );
                if (key[i] != root[i])  return i-1;
            }
            return i-1;
        }

        ///Discard all trees from forest
        void discard() {
            _forest.discard();
        }

        ///Get ID reference from COID
        ID get_node_ref (COID id) const				{ return ID (*_forest[id.get_tree_id()], id.get_node_id()); }
        bool get_node (COID id, ID& node) const     { return _forest[id.get_tree_id()] ? _forest[id.get_tree_id()]->get_node(id, node) : false;}
        ///Get ID reference from tree id and a node id
        bool get_node (uint tid, LNID id, ID& node) const   { return _forest[tid] ? _forest[tid]->get_node(id, node) : false; }

        ///Return ptr to node, given a node number
        T* get_node(COID id) const                  { return _forest[id.get_tree_id()]->get_node((LNID)id); }
        T* get_node( uint tid, LNID id ) const      { return _forest[tid]->get_node(id); }

        ///Return count of trees in the forest
        uint count() const            { return _forest.size(); }
    };

    //explicit ttree (FOREST& forest) : _forest (&forest)
    explicit ttree (forest& f, ID* __id= NULL, bool __hash= false) : _forest (&f)
    //explicit ttree (forest& f) : _forest (&f)
    {
        _idinforest = _forest->add (this);
        if(__id) _foreign= *__id;
        else _foreign._id = UMAX;
        enable_hash (__hash);
    }

    ttree (forest& f, uint idf, ID* __id= NULL, bool __hash= false)
    {
        _idinforest = idf;
        _forest = &f;
        if (__id)  _foreign= *__id;
        else  _foreign._id = UMAX;
        enable_hash (__hash);
    }

    explicit ttree (bool __hash= false)
    {
        _forest = new forest;
        _idinforest = _forest->add (this);
        _foreign._id = UMAX;
        enable_hash (__hash);
    }

    ttree (const ttree& t)
    {
        _nodes = t._nodes;
        _idinforest = t._idinforest;
        _forest = t._forest;
        _foreign = t._foreign;
        _levels = t._levels;
        _depth = t._depth;

        _name = t._name;
    }

    ttree& operator = (const ttree& t)
    {
        new (this) ttree<T,NCT,MAP_POLICY> (t);
        return *this;
    }

    ///Set tree name
    void set_name( const token& name )  { _name = name; }
    ///Get tree name
    token get_name() const       { return _name; }

    ///Get count of levels in the tree
    uint get_level_count() {
        return _levels.size();
    }

    ///Set name for given level
    void set_level_name (uint level, const token& name)  {
        if (level >= _levels.size())
            _levels.add( level+1-_levels.size() );
        _levels[level]._name = name;
    }

    ///Get level name
    token get_level_name (uint level) const
    {
        if (level >= _levels.size())  return 0;
        return _levels[level]._name;
    }

    forest& ret_forest() {
        return *_forest;
    }

    ~ttree()
    {
        discard();
    }

    ///Detach all nodes from the tree
    void discard()
    {
        if( is_valid(0) )
            detach(0);
    }

    ///Return true if node with given id exists in the tree
    bool check_key( LNID id ) const         { return id < _nodes.size()  &&  _nodes[id]._class.is_set(); }

    ///Attach a root node to the tree
    /**
        @param node node to attach
        @param node_class node class of the attached node
        @param foreign foreign node to attach to
    */
    ID   attach_root (T* node, const typename ClassRegister<NCT>::NodeClass node_class, COID foreign=COID(UMAX));

    ///Attach a root node to the tree
    /**
        @param superior parent node to attach to
        @param node node to attach
        @param node_class node class of the attached node
        @param pos position within children of the parent node
    */
    ID   attach (const COID superior, T* node, const typename ClassRegister<NCT>::NodeClass node_class, uint pos = UMAX)  {
        ID id;
        _get_node( superior, id );
        return attach( id, node, node_class, pos );
    }

    ID   attach (const LNID superior, T* node, const typename ClassRegister<NCT>::NodeClass node_class, uint pos = UMAX)  {
        ID id;
        _get_node( superior, id );
        return attach( id, node, node_class, pos );
    }

    opcd reattach (const LNID node, const LNID superior, uint pos)
    {
        ID id,root;
        _get_node( superior, root );
        if( root.has_superior(node) )
            throw ersUNAVAILABLE "pure insanity";
        _get_node( node, id );

        return reattach( id, superior, pos);
    }

    ///NODE identifier struct for detach operations
    struct ObjList
    {
        T*      node;
        typename ClassRegister<NCT>::NodeClass  nc;
        COID    id;
    };

    ///Detach specified node
    bool detach (COID node, dynarray<ObjList>* t = 0)  {
        ID id;
        _get_node( node, id );
        return detach (id, t);
    }

    bool detach (LNID node, dynarray<ObjList>* t = 0)  {
        ID id;
        _get_node( node, id );
        return detach (id, t);
    }

    bool detach_single( LNID node )  {
        ID id;
        _get_node( node, id );
        return detach_single(id);
    }

    bool get_detach_list( ID& id, dynarray<ObjList>& t ) const;

    ///Return root node id (COID) of the tree
    COID get_root() const           { return COID (_idinforest, 0); }
    ///Return root node reference (ID)
    void get_root (ID& id) const    { id._tree = (ttree<T,NCT,MAP_POLICY>*)this; id._id = 0; }

    ///Return reference (ID) to required node
    /**
        @param key node number
        @param id ID to set up
        @return true if node exists
    */
    bool get_node (LNID key, ID& id) const
    {
        if (!check_key(key))  {
            id= ID(0);
            return false;
        }
        id._tree = (ttree<T,NCT,MAP_POLICY>*)this;
        id._id = key;
        return true;
    }

    bool get_node (COID key, ID& id) const
    {
        id= ID(0);
        uint   idt = key.get_tree_id();
        if (idt >= _forest->count())  return false;
        if (!(*_forest)[idt]->check_key(key))  return false;
        id._tree = (*_forest)[idt];
        id._id = key.get_node_id();
        return true;
    }

    ///Return ptr to node, given a node number
    T* get_node (COID id) const
    {
        uint   idt = id.get_tree_id();
        if( idt >= _forest->count() )  return 0;
        if( !(*_forest)[idt]->check_key(id) )  return 0;
        return (*_forest)[idt]->_nodes[id.get_node_id()]._node;
    }

    T* get_node (LNID id) const
    {
        if( !check_key(id) )  return 0;
        return _nodes[id]._node;
    }

    ///Return node class of specified node
    typename ClassRegister<NCT>::NodeClass get_class (COID id) const    { const NODE* n = _get_node(id);  return n ? n->_class : ClassRegister<NCT>::NodeClass(); }
    typename ClassRegister<NCT>::NodeClass get_class (LNID id) const    { const NODE* n = _get_node(id);  return n ? n->_class : ClassRegister<NCT>::NodeClass(); }

    ///Hook \a node to \a superior
    static bool hook( ID& node, ID& ref_superior )
    {
        *node->_ahookref.add(1) = ref_superior;
        *ref_superior->_ahooked.add(1) = node;

        return true;
    }

    ///Unhook \a node from \a superior
    static bool unhook( ID& node, ID& ref_superior )
    {
        long i = node->is_hooked_to(ref_superior);
        long j = ref_superior->is_referred_by(node);
        if (i < 0  &&  j < 0)  return false;
        RASSERTX( i>=0 && j>=0, "tree hook damaged" );

        node->_ahookref.del(i);
        ref_superior->_ahooked.del(j);
        return true;
    }

    ///Return true if \a node is hooked to \a ref_superior
    static bool is_hooked_to (const ID& node, const ID& ref_superior)   { return node->is_hooked_to (ref_superior) >= 0; }

    ///Return id's of nodes to which given node is hooked to
    static dynarray<COID>& get_hooked (const ID& node, dynarray<COID>& out)   { return node->get_hooked (out); }
    ///Return id's of nodes that are hooked to given node
    static dynarray<COID>& get_referred (const ID& node, dynarray<COID>& out)   { return node->get_referred (out); }
    ///Return id's of children nodes of given node
    static dynarray<COID>& get_subordinated (const ID& node, dynarray<COID>& out) { return node->get_subordinated (out); }

    ///Move node in children list of its parent to specified position
    static void move_in_subordinated (const ID& node, uint pos);

    ///Get list of superior nodes of current node
    /**
        @param root highest node to go up to
        @param out memory where to store identifiers of the superior nodes
        @param count in: number of elements the \a out points to; out: actual count of the returned nodes
        @param toroot set to true if id's of root nodes above the given \a root node should be returned too
        @return true if key belongs under the root
    */
    bool get_superiors (COID key, COID root, COID* out, uint& count, bool toroot=false) const
    {
        ID id;
        if( !x_get_node(key, id) )  return false;
        return id.get_superiors( root, out, count, toroot );
    }

    bool get_superiors (LNID key, COID root, COID* out, uint& count, bool toroot=false) const
    {
        ID id;
        if( !x_get_node(key, id) )  return false;
        return id.get_superiors( root, out, count, toroot );
    }

    bool get_superiors (LNID key, LNID root, LNID* out, uint& count, bool toroot=false) const
    {
        ID id;
        if( !x_get_node(key, id) )  return false;
        return id.get_superiors( root, out, count, toroot );
    }

    bool get_superiors_table (LNID key, LNID root, LNID* out, uint* rootoffs, uint& count, bool toroot=false) const
    {
        ID id;
        if( !x_get_node( key, id ) )  return false;
        return id.get_superiors_table (root, out, rootoffs, count, toroot);
    }

    ///Get parent node of specified node
    bool get_superior (COID key, COID* superior) const
    {
        *superior = _get_node(key)->_superior;
        return !superior->is_invalid();
    }

    bool get_superior (LNID key, COID* superior) const
    {
        *superior = _get_node(key)->_superior;
        return !superior->is_invalid();
    }

    ///Get ptr to parent node of specified node
    T* get_superior (COID key) const
    {
        ID n;
        if (!x_get_node(key, n))  return 0;
        return n->_superior.is_invalid() ? 0 : get_node(n->_superior);
    }

    T* get_superior (LNID key) const
    {
        ID n;
        if (!x_get_node(key, n))  return 0;
        return (n->_superior.is_invalid() || n->_superior.is_local_root()) ? 0 : get_node((LNID)n->_superior);
    }

    ///Get ID of parent node of specified node
    bool get_superior (COID key, ID& superior) const        {
        _get_node( key, superior );
        return superior.goto_superior();
    }
    bool get_superior (LNID key, ID& superior) const        {
        _get_node( key, superior );
        return superior.goto_superior();
    }

    uint match_superiors (dynarray<LNID>& sup, LNID key) const
    {
        ID n;
        _get_node(key, n);

        long lev = n->get_level();

        long count = sup.size();
        if (lev >= count)
            sup.need (lev+1);

        for (;;--lev)
        {
            if (lev >= count)
                sup[lev] = n;
            else if (n == sup[lev])
                return lev;

            if (!n.goto_superior_local())
                break;
        }

        throw ersIMPROPER_STATE "should not get here";
    }

    ///Return number of levels in the tree
    uint depth() const                  { return _depth; }

    ///Return level of the node
    uint key_level (COID key) const     { return _get_node(key)->_wlev; }
    uint key_level (LNID key) const     { return _get_node(key)->_wlev; }

    ///Return true if the node has no children
    bool is_lowest (COID key) const     { return _get_node(key)->_asubnodes.size() == 0; }
    bool is_lowest (LNID key) const     { return _get_node(key)->_asubnodes.size() == 0; }

    bool is_valid (COID key) const
    {
        uint   idt = key.get_tree_id();
        RASSERTX( idt < _forest->count(), "invalid tree id" );
        return (*_forest)[idt]->check_key( (LNID)key );
    }
    bool is_valid (LNID key) const      { return check_key(key); }

    ///Return id of this tree within the forest
    uint get_tree_id() const            { return _idinforest; }

    ///Perform insert by hierarchical key
    /**
        @param hrcode hierarchical code of parent node
        @param en node to insert
        @param nc node class of the node
        @param pos position within children of the parent node
        @param out [out] ID to set to point to the inserted node
    */
    opcd insert( const token& hrcode, T* en, const typename ClassRegister<NCT>::NodeClass nc, uint pos, ID* out )
    {
        ID root;
        if (!en)  return ersINVALID_PARAMS;
        if (_codemap.exists_code (en))  return ersALREADY_EXISTS;

        if( find( hrcode, root ) )
        {
            *out = attach (root, en, nc, pos);
            return 0;
        }
        else if (hrcode.is_empty())
        {
            *out = attach_root (en, nc);
            return 0;
        }
        return ersINVALID_PARAMS;
    }

    ///replace T of node identified by \a code
    opcd replace( const token& code, T*& en, const typename ClassRegister<NCT>::NodeClass nc )
    {
        ID root;
        if (!en)  return ersINVALID_PARAMS "null pointer";

        LNID ln = _codemap.find_code (code);
        if (ln.is_invalid())  return ersNOT_FOUND "code not found";

        NODE* n = _get_node(ln);
        if( !n->_class.is_of_type(nc) )  return ersINVALID_PARAMS "node class doesn't match";
        const T* old = n->_node;

        _codemap.change_code (en, code);

        n->_node = (T*)en;
        en = old;

        return 0;
    }

    ///Get hierarchical address of specified node
    opcd get_hrkey( LNID key, charstr& out ) const
    {
        COID ac[256];
        uint cnt = 256;

        ID id;
        if( !get_node(key, id) )
            return ersOUT_OF_RANGE;

        COID root = get_root();
        id.get_superiors( root, ac, cnt );
        
        if( cnt == 1) {
            out << char('/');
            return 0;
        }

        for (uint i=1; i<cnt; ++i)
        {
            const T* no = _get_node(ac[i])->_node;
            out << char('/') << _codemap.get_code(no);
        }

        return 0;
    }

    ///Find node by hierarchic code
    bool find( const token& hrcode, ID& out, bool casecmp=true, token separator=token::empty() ) const
    {
        if( count()==0 )  return false;
        if( separator.is_empty() )
            separator = "\\/";

        token tok = hrcode;

        ID id;
        get_root(id);

        tok.skip_ingroup(separator);

        while( !tok.is_empty() )
        {
            token k = tok.cut_left(separator,-1);

            COID* pn = id->_asubnodes.ptr();
            uint i, lim = id->_asubnodes.size();

            for( i=0; i<lim; ++i )
            {
                const T* no = _get_node(pn[i])->_node;
                if( k.cmpeqc( _codemap.get_code(no), casecmp ) ) {
                    get_node( pn[i], id );
                    break;
                }
            }
            if( i>=lim )  return false;
        }
        out = id;
        return true;
    }

    ///Find node by hierarchic code
    bool find_or_create( const token& hrcode, ID& out, bool casecmp=true, token separator=token::empty() )
    {
        if( count()==0 )  return false;
        if( separator.is_empty() )
            separator = "\\/";

        token tok = hrcode;

        ID id;
        get_root(id);

        tok.skip_ingroup(separator);

        while( !tok.is_empty() )
        {
            token k = tok.cut_left(separator,-1);

            COID* pn = id->_asubnodes.ptr();
            uint i, lim = id->_asubnodes.size();

            for( i=0; i<lim; ++i )
            {
                const T* no = _get_node(pn[i])->_node;
                if( k.cmpeqc( _codemap.get_code(no), casecmp ) )
                {
                    get_node( pn[i], id );
                    break;
                }
            }
            if( i >= lim )
            {
                T* n = new T;

                //T shuld have set_code( const token& ) method
                n->set_code(k);
                id = attach( (LNID)id, n, id->_class );

                if( id.is_invalid() )
                    return false;
            }
        }
        out = id;
        return true;
    }

    ///Get level of node
    uint get_level (LNID key) const    {
        ID id;
        EASSERT( get_node(key, id) );
        if (id.is_invalid()) return 0;
        return id.get_level();
    }

    ///Compare positions of two nodes in linear expansion of the tree
    ///@return -1 if key1 is before key2, 1 if key1 is after key2, 0 if they are the same
    long compare (LNID key1, LNID key2) const
    {
        const NODE* n1 = _get_node(key1);
        const NODE* n2 = _get_node(key2);
        if (n1->_seqid < n2->_seqid)  return -1;
        if (n1->_seqid > n2->_seqid)  return 1;
        return 0;
    }

    ///Return hierarchical and linear position
    /**
        @return 0 if key2 is hierarchically below key1, 1 if key1 is hierarchically below key2
        2 if key1 occurs before key2 in linear list and 3 otherwise
    */
    long compare_hr_ord (LNID key1, LNID key2) const
    {
        //test if the keys lie on the same branch
        const NODE* n1 = _get_node(key1);
        const NODE* n2 = _get_node(key2);

        if (n1->_wlev != n2->_wlev)
        {
            if (n1->_wlev < n2->_wlev)
            {
                if (belongs_under (key2, key1))  return 0;
            }
            else if (belongs_under (key1, key2)) return 1;
        }
        if (n1->_seqid < n2->_seqid)  return 2;
        if (n1->_seqid == n2->_seqid)
            throw ersIMPROPER_STATE;
        return 3;
    }

    ///Return true if node \a key belongs hierarchically under \a root
    bool belongs_under (LNID key, COID root) const {
        ID id;
        _get_node(key, id);
        return id.has_superior (root);
    }

    bool belongs_under (LNID key, LNID root) const {
        ID id;
        if(!get_node (key, id)) return false;
        return id.has_superior (root);
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Do OR operation on given bitmap (byte) array on elements addressed by selected nodes
    /**
        @param parentkey root node to start from
        @param pbdata byte array to use
        @param vor byte mask to use as one operand of the operation
        @param level to apply to
        For level > 0 select all subordinated nodes up to the level, level < 0 select all subordinated
        nodes above |level|
    */
    opcd scan_sub_then_OR (
                         LNID parentkey,
                         uchar* pbdata,
                         uint vor,
                         long lev) const
    {
        uint i, lim;

        dynarray<COID> subs;
        ID idp;
        get_node (parentkey, idp);

        if (!get_subordinated (idp, subs))  return ersINVALID_PARAMS;

        lim = subs.size();
        for (i=0; i<lim; i++)
        {
            if (lev>0) {
                pbdata[subs[i].get_node_id()] |= (uchar)vor;
                if(lev>1)  scan_sub_then_OR (subs[i], pbdata, vor, lev-1);
            }
            else if (lev==0)
                pbdata[subs[i].get_node_id()] |= (uchar)vor;
            else if (lev<0)
                scan_sub_then_OR (subs[i], pbdata, vor, lev-1);
        }

        return 0;
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///Do AND operation on given bitmap (byte) array on elements addressed by selected nodes
    /**
        @param parentkey root node to start from
        @param pbdata byte array to use
        @param vand byte mask to use as one operand of the operation
        @param level to apply to
        For level > 0 select all subordinated nodes up to the level, level < 0 select all subordinated
        nodes above |level|
    */
    opcd scan_sub_then_AND (
                         LNID parentkey,
                         uchar *pbdata,
                         uchar vand,
                         long lev) const
    {
        uint i,lim;

        dynarray<COID> subs;
        ID idp;
        get_node (parentkey, idp);

        if (!get_subordinated (idp, subs))  return ersINVALID_PARAMS;

        lim = subs.size();
        for (i=0; i<lim; i++)
        {
            if (lev>0) {
                pbdata[subs[i].get_node_id()] &= (uchar)vand;
                if (lev>1) scan_sub_then_AND (subs[i], pbdata, vand, lev-1);
            }
            else if (lev==0)
                pbdata[subs[i].get_node_id()] &= (uchar)vand;
            else if (lev<0)
                scan_sub_then_AND (subs[i], pbdata, vand, lev-1);
        }

        return 0;
    }

    enum {
        TTREE_STREAM_VERSION    = 0xdead0007,
        TTREE_STREAM_VERSION_06 = 0xdead0006,
        TTREE_STREAM_VERSION_O5 = 0xdead0005,
    };

    friend binstream& operator << (binstream& out, const ttree<T,NCT,MAP_POLICY>& t)
    {
        RASSERTX( t._depth >= t._levels.size()  ||  t._levels[t._depth]._nitems == 0 , "invalid tree depth" );
        return out << TTREE_STREAM_VERSION << t._nodes << t._idinforest << t._foreign << t._levels << t._depth << t._name;
    }

    friend binstream& operator >> (binstream& in, ttree<T,NCT,MAP_POLICY>& t)
    {
        uint v;
        in >> v;
        if (v == TTREE_STREAM_VERSION_O5)
        {
            //convert from version 5
            dynarray<uint> lev;
            in >> t._nodes >> t._idinforest >> t._foreign >> lev >> t._name;
            t._levels.need_new (lev.size());
            for (uints i=0; i<lev.size(); ++i)
                t._levels[i]._nitems = lev[i];
            t._depth = lev.size();
        }
        else if (v == TTREE_STREAM_VERSION_06)
        {
            in >> t._nodes >> t._idinforest >> t._foreign >> t._levels >> t._depth >> t._name;
            //must correct the depth that can be in 06 version one less than the actual one
            for (uints i=t._levels.size(); i>0; ) {
                --i;
                if (t._levels[i]._nitems)   { t._depth = i+1; break; }
            }
        }
        else if (v != TTREE_STREAM_VERSION)
            throw ersINVALID_TYPE "invalid ttree stream version or damaged";
        else
            in >> t._nodes >> t._idinforest >> t._foreign >> t._levels >> t._depth >> t._name;

        RASSERTX( t._depth >= t._levels.size()  ||  t._levels[t._depth]._nitems == 0 , "invalid tree depth" );
        return in;
    }

    ///Return count of nodes in the tree
    uint count() const                { return _nodes.size(); }


friend class ID;
friend class COID;
friend class forest;

    ///Return true if index is being maintained
    bool is_hash() const               { return _codemap.is_enabled(); }
    ///Enable code index
    void enable_hash (bool hash)
    {
        if (hash  &&  !_codemap.is_enabled())
        {
            _codemap.enable (hash);
            for (uint i=0; i<_nodes.size(); ++i)
            {
                if( _nodes[i]._class.is_set() )
                    _codemap.insert_code( _nodes[i]._node, i );
            }
        }
        else
            _codemap.enable (hash);
    }

    ///Find node by code, -1 if failed
    LNID find_code( const token& code ) const
    {
        if (!is_hash()) return UMAX;
        return _codemap.find_code(code);
    }

    ///Insert code to index
    opcd insert_code (const T* node, LNID id)
    {
        if(!_codemap.is_enabled()) return ersUNAVAILABLE "hash not running";

        if(node && !_codemap.insert_code(node, id))  return ersALREADY_EXISTS "same key inserted before";
        return NOERR;
    }

    ///Replace code in index
    opcd change_code (const T* node, const token& oldcode)
    {
        if (!_codemap.is_enabled()) return ersUNAVAILABLE "hash not running";

        return _codemap.change_code( node, oldcode )  ?  0  :  ersFAILED;
    }

    opcd change_code (const T* node, const token& oldcode, uint __id)
    {
        if (!_codemap.is_enabled()) return ersUNAVAILABLE "hash not running";

        return _codemap.change_code( node, oldcode, __id)  ?  0  :  ersFAILED;
    }

    ///Return true if node exists in index
    bool exists_code (const T* node) const          { return _codemap.exists_code (node); }


    ///Return true if node referred to by \a node exists
    static bool node_exists (const ID& node)
    {
        return node._tree->check_key(node._id);
    }

    ///Return true if node referred to by \a node exists
    bool node_exists (COID node) const
    {
        uint idt = node.get_tree_id();
        RASSERTX( idt < _forest->count(), "invalid tree id" );
        ttree<T,NCT,MAP_POLICY>* t = (*_forest)[idt];
        return t->check_key( node.get_node_id() );
    }

    ///Return true if node referred to by \a node exists
    bool node_exists (LNID node) const
    {
        return check_key(node);
    }


protected:

    ///Level record
    struct LEVEL
    {
        uint   _nitems;                ///< number of items in the level
        charstr _name;                  ///< level name

        LEVEL& operator ++()   { ++_nitems;  return *this; }
        LEVEL& operator --()   { --_nitems;  return *this; }

        LEVEL()
        {
            _nitems = 0;
        }

        friend binstream& operator << (binstream& out, const LEVEL& l)  { return out << l._nitems << l._name; }
        friend binstream& operator >> (binstream& in, LEVEL& l)         { return in >> l._nitems >> l._name; }
    };

    dynarray<NODE>  _nodes;             ///< array of tree nodes
    uint            _idinforest;        ///< index of this tree within the forest
    forest*         _forest;            ///< ref.to common tree container
    COID            _foreign;           ///< a node from another tree within the forest, to what this tree is hooked
    dynarray<LEVEL> _levels;            ///< levels info
    uint            _depth;             ///< depth of tree

    MAP_POLICY      _codemap;           ///< tree map
    charstr         _name;              ///< tree name

    ///Get node ptr to required node
    NODE* _get_node( COID id )
    {
        uint idt = id.get_tree_id();
        RASSERTX( idt < _forest->count(), "invalid tree id" );
        ttree<T,NCT,MAP_POLICY>* t = (*_forest)[idt];
        RASSERTX( t->check_key(id.get_node_id()), "invalid node id" );
        return t->_nodes.ptr() + id.get_node_id();
    }
    ///Get node ptr to required node
    const NODE* _get_node (COID id) const
    {
        uint idt = id.get_tree_id();
        RASSERTX( idt < _forest->count(), "invalid tree id" );
        ttree<T,NCT,MAP_POLICY>* t = (*_forest)[idt];
        RASSERTX( t->check_key(id.get_node_id()), "invalid node id" );
        return t->_nodes.ptr() + id.get_node_id();
    }


    ///Get ID pointer to required node
    void _get_node (COID id, ID& idn) const
    {
        uint idt = id.get_tree_id();
        DASSERT( idt < _forest->count() );
        ttree<T,NCT,MAP_POLICY>* t = (*_forest)[idt];
        DASSERT( t->check_key(id.get_node_id()) );
        idn._tree = t;
        idn._id = id.get_node_id();
    }

    ///Get ID pointer to required node
    void _get_node (LNID id, ID& idn) const
    {
        DASSERT( check_key(id) );
        idn._tree = (ttree<T,NCT,MAP_POLICY>*)this;
        idn._id = id._id;
    }

    ///Get ID pointer to required node
    bool x_get_node (COID id, ID& idn) const
    {
        uint idt = id.get_tree_id();
        DASSERT( idt < _forest->count() );
        ttree<T,NCT,MAP_POLICY>* t = (*_forest)[idt];
        if(!t->check_key(id.get_node_id()))
            return false;
        idn._tree = t;
        idn._id = id.get_node_id();
        return true;
    }

    ///Get ID pointer to required node
    bool x_get_node (LNID id, ID& idn) const
    {
        if(!check_key(id))
            return false;
        idn._tree = (ttree<T,NCT,MAP_POLICY>*)this;
        idn._id = id._id;
        return true;
    }

    ///Get node ptr to required node
    NODE* _get_node (LNID id)
    {
        RASSERTX( id < count(), "invalid node id" );
        return _nodes + id._id;
    }
    ///Get node ptr to required node
    const NODE* _get_node (LNID id) const
    {
        RASSERTX( id < count(), "invalid node id" );
        return _nodes.ptr() + id._id;
    }

    ///Attach node to superior node
    ID   attach( ID& superior, T* node, const typename ClassRegister<NCT>::NodeClass node_class, uint pos );
    ///Detach node
    bool detach( ID& node, dynarray<ObjList>* t = 0, bool unhooksup = true );
    bool detach_single( ID& node );

    opcd reattach( ID& id, LNID root, uint pos );

    ///Make space in seqid numbering for \a n nodes that would be inserted between the two nodes
    bool insert_seqid (ID& idp, uint n, ID& idn);
    ///Shift seqid of \a id and following nodes by \a n
    uint shift_seqid_n (ID& id, uint n);
    ///Shift seqid of \a id and preceding nodes by \a n
    uint shift_seqid_p (ID& id, uint n);

    void hook_to_hr( ID& superior, ID& id, uint pos );
};

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline typename ttree<T,NCT,MAP_POLICY>::ID ttree<T,NCT,MAP_POLICY>::attach_root
    (
    T* node,
    const typename ClassRegister<NCT>::NodeClass node_class,
    COID foreign
    )
{
    if (_nodes.size() > 0)  return ID(0);

    ID id (*this, 0);

    NODE* no = _nodes.add(1);
    no->_class = node_class;
    no->_superior.set_invalid();
    no->_node = (T*) node;
    no->_seqid = 0;
    no->_wlev = 0;

    _foreign = foreign;
    if (!foreign.is_invalid())
    {
        ID superior;
        _get_node(foreign, superior);

        superior->add (id, UMAX);
/*        const NODE* fno = superior;
        ttree<T,NCT,MAP_POLICY>* ftree = superior._tree;

        if ((uint)fno->_wlev+1 >= ftree->_levels.size()) {
            ++*ftree->_levels.add(1);
            ftree->_depth = fno->_wlev+1;
        }
        else
            ++ftree->_levels[fno->_wlev+1];*/
    }

    ++*_levels.need_new(1);
    _depth = 1;

    //hash
    insert_code (no->_node, id);

    return id;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline typename ttree<T,NCT,MAP_POLICY>::ID ttree<T,NCT,MAP_POLICY>::attach
    (
    ID& superior,
    T* node,
    const typename ClassRegister<NCT>::NodeClass node_class,
    uint pos
    )
{
    if (!node_exists(superior))  return ID(0);          //invalid parent node
    if (this != superior._tree)  return superior._tree->attach (superior, node, node_class, pos);

    ID id (*this, _nodes.size());

    //test hash
    if (exists_code (node))  return ID(0);

    NODE* no = _nodes.add(1);
    no->_class = node_class;
    no->_superior = superior;
    no->_node = (T*) node;
    no->_wlev = superior->_wlev + 1;

    hook_to_hr( superior, id, pos );

    //hash
    insert_code (node, id);

    return id;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline void ttree<T,NCT,MAP_POLICY>::hook_to_hr( ID& superior, ID& id, uint pos )
{
    superior->add( id, pos );

    NODE* no = id;
    if (no->_wlev >= _levels.size()) {
        ++*_levels.add(1);
        _depth = no->_wlev+1;
    }
    else
        ++_levels[no->_wlev];

    // compute _seqid value for node
    ID ip = id;
    ip.scanto_prev();
    uint psid = ip->_seqid;

    ID in = id;
    if (!in.scanto_next()) {
        if (ip->_seqid > UMAX - 2)
            shift_seqid_p (ip, 1);
        no->_seqid = ip->_seqid + ((UMAX - ip->_seqid) >> 1);
    }
    else
    {
        uint nsid = in->_seqid;
        if (nsid - psid == 1) {     //no space
            insert_seqid (ip, 1, in);
            id->_seqid = ip->_seqid + ((in->_seqid - ip->_seqid) >> 1);
        }
        else
            no->_seqid = psid + ((nsid - psid) >> 1);
    }
}


////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline bool ttree<T,NCT,MAP_POLICY>::detach
    (
    ID& id,
    dynarray<typenamex ttree<T,NCT,MAP_POLICY>::ObjList>* t,
    bool unhooksup
    )
{
    if( this != id._tree )
        return id._tree->detach( id, t, unhooksup );

    if( id->_ahooked.size() )
    {
        //destroy hooked nodes
        dynarray<COID>& h = id->_ahooked;
        for( uint i=h.size(); i>0; )
        {
            --i;
            ID hid;
            EASSERT( get_node( h[i], hid ) );
            detach( hid, t, true );
        }
        h.reset();
    }

    if( id->_ahookref.size() )
    {
        //unhook
        dynarray<COID>& h = id->_ahookref;
        for( uint i=h.size(); i>0; )
        {
            --i;
            ID hid;
            EASSERT( get_node( h[i], hid ) );
            unhook( id, hid );
        }
        h.reset();
    }

    //destroy subordinates
    if( id->_asubnodes.size() )
    {
        dynarray<COID>& h = id->_asubnodes;
        for( uint i=h.size(); i>0; )
        {
            --i;
            ID hid;
            EASSERT( get_node( h[i], hid ) );
            detach( hid, t, false );
        }
        h.reset();
    }

    --_levels[id->_wlev];
    if( _levels[id->_wlev]._nitems == 0 )
        _depth = id->_wlev;

    if( id->_node ) {
        if( is_hash() && exists_code(id->_node) )
            _codemap.erase( id->_node );

        if(t)
        {
            ObjList* p = (*t).add (1);
            p->node = id->_node;
            p->nc = id->_class;
            p->id = id;
        }
        else
            delete id->_node;
        id->_node = 0;
    }
    id->_class.clear();

    if(unhooksup)
    {
        //detach from superior node
        ID sup (*this, id->_superior);
        if( !sup.is_invalid() )
            sup->del(id);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline bool ttree<T,NCT,MAP_POLICY>::detach_single
    (
    ID& id
    )
{
    if( this != id._tree )
        return id._tree->detach_single(id);

    if( id->_ahooked.size() )
    {
        //unhook hooked nodes
        dynarray<COID>& h = id->_ahooked;
        for( uint i=h.size(); i>0; )
        {
            --i;
            ID hid;
            if( !get_node( h[i], hid ) ) continue;
            unhook( hid, id );
        }
        h.reset();
    }

    if( id->_ahookref.size() )
    {
        //unhook
        dynarray<COID>& h = id->_ahookref;
        for( uint i=h.size(); i>0; )
        {
            --i;
            ID hid;
            if( !get_node( h[i], hid ) ) continue;
            unhook( id, hid );
        }
        h.reset();
    }

    id->_asubnodes.reset();

    --_levels[id->_wlev];
    if( _levels[id->_wlev]._nitems == 0 )
        _depth = id->_wlev;

    id->_class.clear();
    if( id->_node ) {
        T* n = id->_node;
        if( is_hash() && exists_code(n) )
            _codemap.erase(n);

        id->_node = 0;
        delete n;
    }

    //detach from superior node
    ID sup (*this, id->_superior);
    if( !sup.is_invalid() )
        sup->del(id);

    return true;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline bool ttree<T,NCT,MAP_POLICY>::get_detach_list
    (
    ID& id,
    dynarray<typenamex ttree<T,NCT,MAP_POLICY>::ObjList>& t
    ) const
{
    if( this != id._tree )
        return id._tree->get_detach_list( id, t );

    if( id->_ahooked.size() )
    {
        //destroy hooked nodes
        dynarray<COID>& h = id->_ahooked;
        for( uint i=0; i<h.size(); ++i )
        {
            ID hid;
            EASSERT( get_node( h[i], hid ) );
            get_detach_list( hid, t );
        }
    }

    //destroy subordinates
    if( id->_asubnodes.size() )
    {
        dynarray<COID>& h = id->_asubnodes;
        for (uint i=0; i<h.size(); ++i)
        {
            ID hid;
            EASSERT( get_node( h[i], hid ) );
            get_detach_list( hid, t );
        }
    }

    if( id->_node )
    {
        ObjList* p = t.add(1);
        p->node = id->_node;
        p->nc = id->_class;
        p->id = id;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline opcd ttree<T,NCT,MAP_POLICY>::reattach( ID& id, LNID root, uint pos )
{
    --_levels[id->_wlev];
    if (_levels[id->_wlev]._nitems == 0)
        _depth = id->_wlev;

    //detach from superior node
    ID sup( *this, id->_superior );
    if (!sup.is_invalid())
        sup->del(id);

    sup.set_node_id(root);
    hook_to_hr( sup, id, pos );

    return 0;
}

////////////////////////////////////////////////////////////////////////////////
///Make space in the _seqid sequence for new node
template <class T, class NCT, class MAP_POLICY>
inline bool ttree<T,NCT,MAP_POLICY>::insert_seqid( typename ttree::ID& idp, uint n, typename ttree::ID& idn )
{
    n -= shift_seqid_n (idn, n);
    if (n)
        n -= shift_seqid_p (idp, n);
    RASSERTX( n==0, "no more space for _seqid?" );
    return n==0;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline uint ttree<T,NCT,MAP_POLICY>::shift_seqid_n(  typename ttree::ID& idn, uint n )
{
    ID idnn = idn;

    if (!idnn.scanto_next()) {
        if (UMAX - idn->_seqid >= 2*n)
            idn->_seqid += (UMAX - idn->_seqid) >> 1;
        else if (UMAX - idn->_seqid > n)
            idn->_seqid += n;
        else
            return 0;
    }
    else
    {
        uint f = idnn->_seqid - idn->_seqid;

        if (f > 2*n)
            idn->_seqid += f >> 1;
        else if (f > n)
            idn->_seqid += n;
        else {
            uint k;
            k = shift_seqid_n (idnn, n-f+1);
            idn->_seqid += k+f-1;
            return k+f-1;
        }
    }

    return n;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline uint ttree<T,NCT,MAP_POLICY>::shift_seqid_p( typename ttree::ID& idp, uint n )
{
    ID idpp = idp;

    if (!idpp.scanto_prev()) {
        //cannot shift root
        return 0;
    }

    uint f = idp->_seqid - idpp->_seqid;

    if (f >= 2*n)
        idp->_seqid -= f >> 1;
    else if (f > n)
        idp->_seqid -= n;
    else {
        uint k;
        k = shift_seqid_p (idpp, n-f+1);
        idp->_seqid -= k+f-1;

        return k+f-1;
    }

    return n;
}

////////////////////////////////////////////////////////////////////////////////
template <class T, class NCT, class MAP_POLICY>
inline void ttree<T,NCT,MAP_POLICY>::move_in_subordinated (const ID& node, uint pos)
{
    node.move (pos);
}

COID_NAMESPACE_END

#endif //__COMMON_TTREE_HEADER__
