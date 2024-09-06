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
 * Portions created by the Initial Developer are Copyright (C) 2024
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jakub Lukasik
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

#ifndef __COID_COMM_UNIQUE__HEADER_FILE__
#define __COID_COMM_UNIQUE__HEADER_FILE__

#include "namespace.h"

#include "commtypes.h"
#include "binstream/binstream.h"
//#include <memory>


COID_NAMESPACE_BEGIN

///Template making pointers uniquely owned, and to be automatically destructed when the owner is destructed
template <class T>
class unique_ptr
{
    T* _p = nullptr;
public:
    unique_ptr() {}
    ~unique_ptr() { if (_p) { delete _p; } }

    unique_ptr(T* p) { _p = p; }

    unique_ptr(const unique_ptr& p) = delete;

    //unique_ptr(unique_ptr&& p)
    //{
    //    _p = p._p;
    //    p._p = nullptr;
    //}

    template <class U>
    unique_ptr(unique_ptr<U>&& p)
    {
        _p = p.eject();
    }

    template <typename... Args>
    static unique_ptr<T> emplace(Args&&... args) {
        return unique_ptr<T>(new T(static_cast<Args&&>(args)...));
    }

    T* ptr() const { return _p; }
    T*& ptr_ref() { return _p; }

    explicit operator bool() const { return _p != 0; }

    //operator T* (void) const { return _p; }

    int operator ==(const T* ptr) const { return ptr == _p; }
    int operator !=(const T* ptr) const { return ptr != _p; }

    T& operator *(void) { return *_p; }
    const T& operator *(void) const { return *_p; }

    #ifdef SYSTYPE_MSVC
    #pragma warning (disable : 4284)
    #endif //SYSTYPE_MSVC
    T* operator ->(void) { return _p; }
    const T* operator ->(void) const { return _p; }
    #ifdef SYSTYPE_MSVC
    #pragma warning (default : 4284)
    #endif //SYSTYPE_MSVC

    unique_ptr& operator = (T* p) {
        if (_p) delete _p;
        _p = p;
        return *this;
    }

    unique_ptr& operator = (unique_ptr&& p)
    {
        if (_p) {
            delete _p;
        }
        swap(p);
        return *this;
    }

    bool is_set() const { return _p != nullptr; }
    bool is_empty() const { return _p == nullptr; }

    T* eject() { T* tmp = _p; _p = nullptr; return tmp; }

    unique_ptr&& move() { return static_cast<unique_ptr&&>(*this); }

    void destroy()
    {
        if (_p)
            delete _p;
        _p = nullptr;
    }

    void swap(unique_ptr<T>& other)
    {
        std::swap(_p, other._p);
    }

    friend void swap(unique_ptr& a, unique_ptr& b) {
        std::swap(a._p, b._p);
    }
};

COID_NAMESPACE_END

#endif // __COID_COMM_UNIQUE__HEADER_FILE__
