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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#include "metastream.h"
#include "../hash/hashkeyset.h"

COID_NAMESPACE_BEGIN


struct SMReg {
    typedef hash_keyset<token,metastream::DESC,_Select_Copy<metastream::DESC,token> >  MAP;
    MAP _map;
    dynarray< local<metastream::DESC> > _arrays;
    //comm_mutex _mutex;
};

////////////////////////////////////////////////////////////////////////////////
void metastream::StructureMap::get_all_types( dynarray<const metastream::DESC*>& dst ) const
{
    SMReg& smr = *(SMReg*)pimpl;

    SMReg::MAP::const_iterator b,e;
    b = smr._map.begin();
    e = smr._map.end();

    dst.reserve( smr._map.size(), false );
    for(; b!=e; ++b )
        *dst.add() = &(*b);
}

////////////////////////////////////////////////////////////////////////////////
metastream::StructureMap::StructureMap()
{
    pimpl = new SMReg;
}

////////////////////////////////////////////////////////////////////////////////
metastream::StructureMap::~StructureMap()
{
    delete (SMReg*)pimpl;
    pimpl = 0;
}

////////////////////////////////////////////////////////////////////////////////
metastream::DESC* metastream::StructureMap::insert( const metastream::DESC& v )
{
    //SMReg& smr = SINGLETON(SMReg);
    //GUARDTHIS(smr._mutex);
    SMReg& smr = *(SMReg*)pimpl;

    return (DESC*) smr._map.insert_value(v);
}

////////////////////////////////////////////////////////////////////////////////
metastream::DESC* metastream::StructureMap::find( const token& k ) const
{
    //SMReg& smr = SINGLETON(SMReg);
    //GUARDTHIS(smr._mutex);
    const SMReg& smr = *(const SMReg*)pimpl;

    return (DESC*) smr._map.find_value(k);
}

////////////////////////////////////////////////////////////////////////////////
metastream::DESC* metastream::StructureMap::create_array_desc( uints size )
{
    //SMReg& smr = SINGLETON(SMReg);
    //GUARDTHIS(smr._mutex);
    SMReg& smr = *(SMReg*)pimpl;

    DESC* d = *smr._arrays.add() = new DESC;
    d->array_size = size;
    return d;
}


COID_NAMESPACE_END
