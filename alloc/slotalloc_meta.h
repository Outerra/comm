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
 * Outerra.
 * Portions created by the Initial Developer are Copyright (C) 2025
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Cyril Gramblicka
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

#ifndef __COID_COMM_SLOTALLOC_META__HEADER_FILE__
#define __COID_COMM_SLOTALLOC_META__HEADER_FILE__


#include "../namespace.h"
#include "slotalloc.h"
#include "../metastream/metastream.h"

COID_NAMESPACE_BEGIN

template<class T, slotalloc_mode MODE = slotalloc_mode::base, class ...Es>
struct slotalloc_binstream_container: public binstream_containerT<T>
{
    using fnc_stream = binstream_container_base::fnc_stream;
    using slotalloc_t = slotalloc_base<T, MODE, Es...>;

    virtual const void* extract(uints n) override
    {
        DASSERT(_current != _end);
        const T* p = &(*_current);
        ++_current;
        return p;
    }

    virtual void* insert(uints n, const void* defval) override
    {
        T* p = _v.add();
        if (defval) 
        {
            *p = *static_cast<const T*>(defval);
        }
        return p;
    }

    virtual bool is_continuous() const override { return false; }

    virtual uints count() const override { return _v.count(); }

    slotalloc_binstream_container(const slotalloc_t& v)
        : _v(const_cast<slotalloc_t&>(v))
        , _current(_v.begin())
        , _end(_v.end())
    {
    }

    slotalloc_binstream_container(const slotalloc_t& v, fnc_stream fout, fnc_stream fin)
        : binstream_containerT<T>(fout, fin)
        , _v(const_cast<slotalloc_t&>(v))
        , _current(_v.begin())
        , _end(_v.end())
    {
    }

protected:
    slotalloc_t& _v;
    slotalloc_t::iterator _current, _end;
};


////////////////////////////////////////////////////////////////////////////////
template<class T, slotalloc_mode MODE = slotalloc_mode::base, class ...Es>
inline metastream& operator || (metastream& m, slotalloc_base<T, MODE, Es...>& a)
{
    if (m.stream_reading()) {
        a.reset();
        slotalloc_binstream_container<T, MODE, Es...> c(a);
        m.read_container(c);
    }
    else if (m.stream_writing()) {
        slotalloc_binstream_container<T, MODE, Es...> c(a);
        m.write_container(c);
    }
    else {
        if (m.meta_decl_array(
            typeid(a).name(),
            -1,
            sizeof(a),
            false,
            0,
            [](const void* p) -> uints { return static_cast<const slotalloc_base<T, MODE, Es...>*>(p)->count(); },
            0,
            0
        )) {
            T* v = 0;
            m || *v;
        }
    }
    return m;
}


COID_NAMESPACE_END

#endif //__COID_COMM_SLOTALLOC_META__HEADER_FILE__
