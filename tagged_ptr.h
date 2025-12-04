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

#include "namespace.h"
#include "commtypes.h"

COID_NAMESPACE_BEGIN

template <typename T>
class tagged_ptr
{
    union {
        struct {
            uints _addr : 48;
            uints _tag : 16;
        };
        intptr_t _ptrval;
        T* _ptr = nullptr;
    };

public:

    tagged_ptr() = default;
    tagged_ptr(nullptr_t) {}
    explicit tagged_ptr(T* p) { set_ptr(p, 0); }
    tagged_ptr(T* p, uint16 tag) { set_ptr(p, tag); }

    tagged_ptr(const tagged_ptr& other) {
        _ptrval = other._ptrval;
    }

    tagged_ptr(tagged_ptr&& other) {
        _ptrval = other._ptrval;
        other._ptrval = 0;
    }

    tagged_ptr(const tagged_ptr<std::remove_const_t<T>>& other) requires std::is_const_v<T> {
        _ptrval = other._ptrval;
    }

    tagged_ptr<T>& operator = (const tagged_ptr<T>& other) {
        _ptrval = other._ptrval;
        return *this;
    }



    T* get_ptr() const {
        intptr_t ptrval = (_ptrval << 16) >> 16;
        return (T*)ptrval;
    }

    void set_ptr(T* ptr, uint16 tag) {
        _ptrval = ((intptr_t)ptr & 0x0000ffffffffffff) | tag;
    }

    uint16 get_tag() const { return _tag; }
    void set_tag(uint16 tag) { _tag = tag; }

    intptr_t value() const { return _ptrval; }

    bool operator == (const tagged_ptr<T>& other) const { return _addr == other._addr; }
    bool operator != (const tagged_ptr<T>& other) const { return _addr != other._addr; }

    explicit operator bool() const { return _addr != 0; }

    T& operator*() { return get_ptr(); }
    const T& operator*() const { return get_ptr(); }

    T* operator->() { return get_ptr(); }
    const T* operator->() const { return get_ptr(); }
};

COID_NAMESPACE_END
