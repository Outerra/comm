#pragma once
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

#ifndef __COID_COMM_HASHKEYSET_META__HEADER_FILE__
#define __COID_COMM_HASHKEYSET_META__HEADER_FILE__


#include "../namespace.h"
#include "hashkeyset.h"
#include "../metastream/metastream.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, template<class> class ALLOC>
inline metastream& operator || (metastream& m, hash_keyset<VAL, EXTRACTKEY, HASHFUNC, EQFUNC, ALLOC>& a)
{
    typedef hash_keyset<VAL, EXTRACTKEY, HASHFUNC, EQFUNC, ALLOC> _HT;

    if (m.stream_reading()) {
        a.clear();
        typename _HT::hashtable_binstream_container bc(a);
        m.read_container(bc);
        return m;
    }
    else if (m.stream_writing()) {
        typename _HT::hashtable_binstream_container bc(a);
        m.write_container(bc);
        return m;
    }
    else {
        if (m.meta_decl_array(
            typeid(a).name(),
            -1,
            sizeof(a),
            false,
            0,  //not a linear array
            [](const void* a) -> uints { return static_cast<const _HT*>(a)->size(); },
            0,//[](void* a, uints& i) -> void* {},
            0 //[](const void* a, uints& i) -> const void* {}
        ))
            m || *(VAL*)0;
    }

    return m;
}

template <class VAL, class EXTRACTKEY, class HASHFUNC, class EQFUNC, template<class> class ALLOC>
inline metastream& operator || (metastream& m, hash_multikeyset<VAL, EXTRACTKEY, HASHFUNC, EQFUNC, ALLOC>& a)
{
    typedef hash_keyset<VAL, EXTRACTKEY, HASHFUNC, EQFUNC, ALLOC> _HT;

    if (m.stream_reading()) {
        a.clear();
        typename _HT::hashtable_binstream_container bc(a);
        return m.read_container(bc);
    }
    else if (m.stream_writing()) {
        typename _HT::hashtable_binstream_container bc(a);
        return m.write_container(bc);
    }
    else {
        if (m.meta_decl_array(
            typeid(a).name(),
            -1,
            sizeof(a),
            false,
            0,  //not a linear array
            [](const void* a) -> uints { return static_cast<const _HT*>(a)->size(); },
            0,//[](void* a, uints& i) -> void* {},
            0 //[](const void* a, uints& i) -> const void* {}
        ))
            m || *(VAL*)0;
    }

    return m;
}


COID_NAMESPACE_END

#endif //__COID_COMM_HASHKEYSET_META__HEADER_FILE__
