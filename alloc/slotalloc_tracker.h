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


/**
@brief Slot allocator with internal ring structure allowing to keep several layers and track changes.

For the active layer it records whether the individual items were created, deleted or possibly modified.
**/
template<bool TRACKING>
struct slotalloc_tracker_base
{
    typedef uint16 changeset_t;

    void initialize( dynarray<changeset_t>* ch ) {}

    void set_modified( uints k ) {}

    dynarray<changeset_t>* get_changeset() { return 0; }
    const dynarray<changeset_t>* get_changeset() const { return 0; }
    uint* get_frame() { return 0; }
};

template<>
struct slotalloc_tracker_base<true>
{
    typedef uint16 changeset_t;

    slotalloc_tracker_base()
        : _changeset(0), _frame(0)
    {}

    void initialize( dynarray<changeset_t>* ch ) {
        _changeset = ch;
        _frame = 0;
    }

    void set_modified( uints k )
    {
        //current frame is always at bit position 0
        (*_changeset)[k] |= 1;
    }

    dynarray<changeset_t>* get_changeset() { return _changeset; }
    const dynarray<changeset_t>* get_changeset() const { return _changeset; }
    uint* get_frame() { return &_frame; }

private:

    dynarray<changeset_t>* _changeset;  //< bit set per item that marks whether the item possibly changed
    uint _frame;                        //< current frame number
};

COID_NAMESPACE_END
