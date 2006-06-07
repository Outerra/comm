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

#ifndef __COMM_TTREEID__HEADER_FILE__
#define __COMM_TTREEID__HEADER_FILE__

#include "../namespace.h"

#include "../commtypes.h"
#include "../binstream/binstream.h"

COID_NAMESPACE_BEGIN

///Local node identifier, just the node identifier without tree id
/**
    This is used as id of node to given tree. Does not contain id of tree within the forest.
*/
class LNID
{
public:
    uint _id;          ///< id of node

    ///Test if the id is invalid
    bool is_invalid () const                    { return _id == UMAX; }
    ///Set the id to an invalid value
    void set_invalid ()                         { _id = UMAX; }

    ///Conversion to unsigned long
    operator uint () const                     { return _id; }

    LNID() {}
    LNID(uint id) : _id(id) {}
};

///Common identifier
/**4B identifier, storing node id and tree id in one long-variable. To physically access the node, a
reference to the forest it's related to is required. COID may specify wildcard tree id (0xff) that would
match any tree, or rather the same tree as another COID
*/
class COID
{
public:
    uint  _id;

    ///Get id of the tree in the forest
    uchar get_tree_id () const                  { return (uchar) (_id >> 24); }
    ///Get id of the node in the tree
    uint get_node_id () const                  { return _id & 0x00ffffff; }

    ///Returns true if specified node belongs to the same tree as this node
    bool is_same_tree (COID n) const            { return (_id & 0xff000000) == (n._id & 0xff000000); }

    void set( uint tid, uint nodeid )
    {
        _id = (tid << 24) + nodeid;
    }

    ///Set id to an invalid value
    void set_invalid ()                         { _id = UMAX; }
    ///Test if the id is set to an invalid value
    bool is_invalid () const                    { return _id == UMAX; }

    ///Set id to mean a root of a general tree
    void set_local_root ()                      { _id = 0xff000000; }
    ///Test whether the id specifies root of a general tree
    bool is_local_root () const                 { return (_id & 0x00ffffff) == 0; }

    ///Set identifier of the tree to specific one
    void set_tree_id (uchar id)                 { _id = (id << 24) + (_id & 0x00ffffff); }
    ///Set identifier of the tree to match the tree id of another node
    void set_tree_id (COID id)                  { _id = (id._id & 0xff000000) + (_id & 0x00ffffff); }

    void set_node_id (LNID id)                  { _id = (_id & 0xff000000) + id; }

    ///Returns true if the id can match node from any tree, not just a particular one
    bool can_match_any_tree () const            { return ((long)_id >> 24)+1 == 0; }

    ///Construct id from unsigned long variable with equal layout (1B tid, 3B nid)
    static COID get_coid(uint id)               { COID t; t._id = id; return t; }

    uint get_uint () const                      { return _id; }
    ulong get_ulong () const                    { return _id; }

    bool operator == (const COID id) const
    {
        if (id._id == _id)  return true;
        if ((id._id & 0x00ffffff) == (_id & 0x00ffffff)
            &&  (can_match_any_tree()  ||  id.can_match_any_tree()) )
            return true;
        return false;
    }
    bool operator != (const COID id) const      { return ! operator == (id); }

    bool operator == (const LNID id) const      { return id._id == (_id & 0x00ffffff); }
    bool operator != (const LNID id) const      { return id._id != (_id & 0x00ffffff); }

    ///Conversion to LNID
    operator LNID () const                       { return (_id & 0x00ffffff) == 0x00ffffff  ?  UMAX  : (_id & 0x00ffffff); }
    //operator uint () const                     { return _id & 0x00ffffff; }

    COID() { set_invalid(); }
    ///Construct from unsigned long node id, with general tree id
    explicit COID (uint k)                       { _id = k; }
    ///Construct from LNID, with general tree id
    explicit COID (LNID k)                      { _id = 0xff000000 | k; }   //any node k
    ///Construct from tree id and a node id
    COID (uint tid, uint node)                  { _id = (tid << 24) | (node & 0x00ffffff); }
};


////////////////////////////////////////////////////////////////////////////////
inline binstream& operator << (binstream& out, const LNID h)
{
    return out << h._id;
}

inline binstream& operator >> (binstream& in, LNID& h)
{
    return in >> h._id;
}

////////////////////////////////////////////////////////////////////////////////
inline binstream& operator << (binstream& out, const COID id)
{
    return out << id._id;
}

inline binstream& operator >> (binstream& in, COID& id)
{
    return in >> id._id;
}

COID_NAMESPACE_END

#endif //__COMM_TTREEID__HEADER_FILE__
