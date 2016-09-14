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
* Portions created by the Initial Developer are Copyright (C) 2016
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

#include "slotalloc.h"
#include "../binstring.h"

COID_NAMESPACE_BEGIN


////////////////////////////////////////////////////////////////////////////////
struct slotalloc_changeset
{
    uint16 mask;

    slotalloc_changeset() : mask(0)
    {}
};



/**
@brief Slot allocator base with optional modification tracking. Adds an extra array with change masks per item.
**/
template<bool TRACKING, class...Es>
struct slotalloc_tracker_base
    : public std::tuple<dynarray<Es>...>
{
    typedef slotalloc_changeset
        changeset_t;
    typedef std::tuple<dynarray<Es>...>
        extarray_t;

    enum : size_t { extarray_size = sizeof...(Es) };

    void swap( slotalloc_tracker_base& other ) {
        static_cast<extarray_t*>(this)->swap(other);
    }

    void set_modified( uints k ) {}

    dynarray<slotalloc_changeset>* get_changeset() { return 0; }
    const dynarray<slotalloc_changeset>* get_changeset() const { return 0; }
    uint* get_frame() { return 0; }
};

///
template<class...Es>
struct slotalloc_tracker_base<true, Es...>
    : public std::tuple<dynarray<Es>..., dynarray<slotalloc_changeset>>
{
    typedef slotalloc_changeset
        changeset_t;
    typedef std::tuple<dynarray<Es>..., dynarray<slotalloc_changeset>>
        extarray_t;

    enum : size_t { extarray_size = sizeof...(Es) + 1 };

    slotalloc_tracker_base()
        : _frame(0)
    {}

    void swap( slotalloc_tracker_base& other ) {
        static_cast<extarray_t*>(this)->swap(other);
        std::swap(_frame, other._frame);
    }

    void set_modified( uints k )
    {
        //current frame is always at bit position 0
        dynarray<slotalloc_changeset>& changeset = std::get<sizeof...(Es)>(*this);
        changeset[k].mask |= 1;
    }

    dynarray<slotalloc_changeset>* get_changeset() { return &std::get<sizeof...(Es)>(*this); }
    const dynarray<slotalloc_changeset>* get_changeset() const { return &std::get<sizeof...(Es)>(*this); }
    uint* get_frame() { return &_frame; }

private:

    ///Position of the changeset within ext arrays
    //static constexpr int CHANGESET_POS = sizeof...(Es);

    uint _frame;                        //< current frame number
};


COID_NAMESPACE_END
